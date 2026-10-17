// engine/core/shm_layout.h
// EGBOS ENGINE_V3 — Shared Memory Region Layout
//
// Specification  : EGBOS-SPEC-S1-001 §6.1–§6.4, §13.3, §14.3–§14.4
// ADR compliance : AD-006 (SHM compile-time constants — no runtime sizing)
// Claims         : CLAIM-WAL-001 (wal.durability, V4) — depends on these
//                  CLAIM-SPSC-001 (ipc.spsc, V4) — depends on kMarketEventsSlots
//
// Govcheck       : GC-E-010 (Warning) — no malloc/new/free/delete in this file
//                  GC-E-011 (pre-registered) — no CLOCK_REALTIME in this file
//                  AD-006 enforced by static_assert on every ring-size constant
//
// S0→S1 change   : S0 stub contained placeholder comments and two bare constants
//                  in egbos::core. S1 replaces this with the complete, production-
//                  correct SHM region map. The two S0 constants (kMaxInstruments,
//                  kShmRegionAlignment) are preserved in egbos::core for backward
//                  compatibility; all S1 constants live in egbos::shm.
//
// Invariants enforced here:
//   SHM-CONST : All region sizes are compile-time constants. Static assertions
//               enforce this; no runtime sizing is possible (AD-006).
//   SHM-MLOCK : shm_init_all() calls mlock(2) on every region; returns only after
//               success. mlock failure is a fatal startup error (spec §13.3).
//   SHM-NUMA  : shm_init_all() binds each region to the execution CPU socket via
//               mbind(2). NUMA allocation failure is a fatal startup error.
//
// Scope note    : All eight region paths and sizes are defined here in S1 even
//                 though layers L1–L7 are implemented in S2–S7. This is required
//                 for compile-time correctness checking across slice boundaries
//                 (spec §6.2 — "S1 requirement").
//
// Forbidden     : CLOCK_REALTIME — not used (pre-compliant with GC-E-011).
//                 malloc/new/free/delete — structurally absent (GC-E-010).
//                 Any runtime region creation — impossible by design (AD-006).

#pragma once

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "market_event_generated.h"
#include "spsc_ring.h"

// POSIX SHM / mmap / mlock / mbind headers (Linux)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

// ─────────────────────────────────────────────────────────────────────────────
// S0 compatibility: egbos::core constants preserved verbatim from S0 bootstrap.
// All code from S0 that references these names continues to compile unchanged.
// ─────────────────────────────────────────────────────────────────────────────
namespace egbos::core {

// Maximum number of instruments in the instrument universe.
// Derived region sizes (e.g. kLobSnapshotsRegionSize) scale with this.
// Changes require an ADR amendment to AD-006 and re-measurement of affected
// timing constants.
inline constexpr std::size_t kMaxInstruments = 256;

// SHM region alignment: all regions are page-aligned.
// mlock(2) operates at page granularity; sub-page alignment would make
// mlock partial-page, which is undefined behaviour on some kernels.
inline constexpr std::size_t kShmRegionAlignment = 4096;  // Page-aligned

}  // namespace egbos::core

// ─────────────────────────────────────────────────────────────────────────────
// egbos::shm — Production SHM region constants and protocol structs (S1)
//
// All constants in this namespace are inline constexpr — zero runtime cost,
// zero heap involvement, available at all translation units including unit tests.
//
// Layout mirrors the authoritative SHM REGION MAP from spec §6.2 verbatim:
//
//   /dev/shm/egbos_market_events     [L0 → L1]   SPSC ring, 65536 slots × 128B
//   /dev/shm/egbos_lob_snapshots     [L1 → L2]   Double-buffered LOB, per instrument
//   /dev/shm/egbos_feature_vector    [L2 → L3]   256B FeatureVector, seqlock protected
//   /dev/shm/egbos_position_state    [L3 → L4]   Read-only L4 view (gross_exposure_ only)
//   /dev/shm/egbos_risk_limits       [L4 → L3]   Monotone latches: kill_switch, pos_limit
//   /dev/shm/egbos_exec_commands     [L3 → L5]   SPSC ring, 4096 slots × 64B
//   /dev/shm/egbos_regime_signal     [L6/L7 → L3] Redis Streams bridge output, 1KB
//   /dev/shm/egbos_governance_flags  [All layers]  Read-only: feed_stale, halt_required
//   ─────────────────────────────────────────────────────────────────────────
//   All regions: mlock(2) mandatory — no page faults on hot path
//   All regions: NUMA-local to execution CPU socket
// ─────────────────────────────────────────────────────────────────────────────
namespace egbos::shm {

// Convenience alias — avoids repeating egbos::core::kMaxInstruments below.
inline constexpr std::size_t kMaxInstruments   = egbos::core::kMaxInstruments;
inline constexpr std::size_t kPageSize         = egbos::core::kShmRegionAlignment;

// ─────────────────────────────────────────────────────────────────────────────
// §1  /dev/shm/egbos_market_events  [L0 → L1]
//     SPSC ring, 65536 slots × 128 bytes = 8 MB total
//     Spec §6.2 (verbatim): "SPSC ring, 65536 slots × 128B"
//     Spec §5.2: ring size must be power of 2 (bitmask modulo).
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kMarketEventsSlots    = 65536;
inline constexpr std::size_t kMarketEventsSlotSize = 128;   // bytes per slot
// SPSC Ring Header overhead requires adding ~192 bytes (3 cache lines).
// We pad to the next page boundary (4096).
inline constexpr std::size_t kMarketEventsRegionSize =
    ((kMarketEventsSlots * kMarketEventsSlotSize) + kPageSize) & ~(kPageSize - 1);
inline constexpr const char* kMarketEventsPath = "/egbos_market_events";

// AD-006: ring size must be power of 2. Bitmask modulo depends on this.
static_assert((kMarketEventsSlots & (kMarketEventsSlots - 1)) == 0,
    "kMarketEventsSlots must be a power of 2 — "
    "SpscRing uses bitmask modulo; non-power-of-2 breaks correctness (AD-006)");
static_assert(kMarketEventsSlots > 0,
    "kMarketEventsSlots must be non-zero (AD-006)");
static_assert(kMarketEventsSlotSize > 0,
    "kMarketEventsSlotSize must be non-zero (AD-006)");

// ─────────────────────────────────────────────────────────────────────────────
// §2  /dev/shm/egbos_lob_snapshots  [L1 → L2]
//     Double-buffered LOB, per instrument.
//     Spec §6.2: "Double-buffered LOB, per instrument"
//
//     Size derivation: 2 buffers × kMaxInstruments × kLobSnapshotPerInstrumentBytes.
//     kLobSnapshotPerInstrumentBytes is a page-aligned placeholder for S1; the
//     exact per-instrument LOB layout is specified in S2 when the LOB engine is
//     implemented. Deviation from this size requires an ADR amendment to AD-006.
// ─────────────────────────────────────────────────────────────────────────────
// kLobSnapshotPerInstrumentBytes must be large enough to hold double buffers.
// Each buffer is 4096 bytes (page size) to allow complex depth.
inline constexpr std::size_t kLobSnapshotPerBufferBytes = kPageSize; // 4096
inline constexpr std::size_t kLobSnapshotBuffers        = 2;         // double-buffer
inline constexpr std::size_t kLobSnapshotPerInstrumentBytes = 
    (kLobSnapshotPerBufferBytes * kLobSnapshotBuffers) + kPageSize; // Padding for active index
    
inline constexpr std::size_t kLobSnapshotsRegionSize =
    kMaxInstruments * kLobSnapshotPerInstrumentBytes;
inline constexpr const char* kLobSnapshotsPath = "/egbos_lob_snapshots";

static_assert(kLobSnapshotsRegionSize > 0,
    "kLobSnapshotsRegionSize must be non-zero (AD-006)");
static_assert(kLobSnapshotsRegionSize % kPageSize == 0,
    "kLobSnapshotsRegionSize must be page-aligned for mlock(2) compatibility");

// ─────────────────────────────────────────────────────────────────────────────
// §3  /dev/shm/egbos_feature_vector  [L2 → L3]  (legacy single-slot)
//     256-byte FeatureVector, seqlock protected.
//     Spec §6.2 (verbatim): "256B FeatureVector, seqlock protected"
//     Spec §6.4: seqlock protocol is verbatim from HTML §C3.4.
//
//     Region is page-sized (4096B) to satisfy mlock(2) page granularity.
//     The first 8 bytes are the seqlock sequence counter; next 256 bytes
//     are the feature vector data. Remaining bytes are reserved.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kFeatureVectorBytes      = 256;
inline constexpr std::size_t kFeatureVectorRegionSize = kPageSize; // page-aligned wrapper
inline constexpr const char* kFeatureVectorPath = "/egbos_feature_vector";

static_assert(kFeatureVectorBytes > 0,
    "kFeatureVectorBytes must be non-zero (AD-006)");
static_assert(kFeatureVectorRegionSize >= kFeatureVectorBytes + sizeof(uint64_t),
    "kFeatureVectorRegionSize must fit sequence counter + feature data");

// ─────────────────────────────────────────────────────────────────────────────
// §3b  /dev/shm/egbos_feature_slots  [L2 → L3]  (per-instrument production ABI)
//
// One FeatureSlot per instrument.  Architecture: per-instrument slot array
// (Architecture A — one fixed slot per instrument, kMaxInstruments slots total).
//
// Per-slot layout (512 bytes, cache-line aligned):
//   FeatureSlotHeader  (64 bytes, one cache line) — seqlock + metadata
//   payload            (kFeatureSlotPayloadBytes = 448 bytes) — feature data
//
// Seqlock protocol (writer):
//   1. sequence.fetch_add(1, seq_cst)   → sequence becomes odd (write in progress)
//   2. write all metadata + payload
//   3. atomic_thread_fence(seq_cst)
//   4. sequence.fetch_add(1, seq_cst)   → sequence becomes even (consistent)
//
// Seqlock protocol (reader):
//   1. seq1 = sequence.load(acquire)
//   2. if seq1 == 0: unpublished — return false
//   3. if seq1 & 1: torn write in progress — retry / return false
//   4. copy header fields + payload
//   5. atomic_thread_fence(acquire)
//   6. seq2 = sequence.load(acquire)
//   7. accept only if seq1 == seq2
//
// Each instrument's slot is independent — reading instrument i does NOT require
// any synchronization with instrument j.  L3 may read any single slot atomically
// without consulting other slots.
//
// Total region size = kMaxInstruments × kFeatureSlotBytes, page-rounded.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kFeatureSlotPayloadBytes = 448; // feature payload per instrument
inline constexpr std::size_t kFeatureSlotBytes        = 512; // header(64) + payload(448)
inline constexpr std::size_t kFeatureRegionSize       =
    ((kMaxInstruments * kFeatureSlotBytes) + kPageSize - 1) & ~(kPageSize - 1);
inline constexpr const char* kFeatureSlotsPath = "/egbos_feature_slots";

static_assert(kFeatureSlotPayloadBytes > 0,
    "kFeatureSlotPayloadBytes must be non-zero (AD-006)");
static_assert(kFeatureSlotBytes == 64 + kFeatureSlotPayloadBytes,
    "kFeatureSlotBytes must equal header(64) + payload");
static_assert(kFeatureSlotBytes % 64 == 0,
    "kFeatureSlotBytes must be a multiple of 64 (cache-line alignment)");
static_assert(kFeatureRegionSize >= kMaxInstruments * kFeatureSlotBytes,
    "kFeatureRegionSize must fit all instrument slots");
static_assert(kFeatureRegionSize % kPageSize == 0,
    "kFeatureRegionSize must be page-aligned for mlock(2)");



// ─────────────────────────────────────────────────────────────────────────────
// §4  /dev/shm/egbos_position_state  [L3 → L4]
//     Read-only L4 view — gross_exposure_ only.
//     Spec §6.2 (verbatim): "Read-only L4 view (gross_exposure_ only)"
//     Spec §6.3: "The only piece of L3 state that L4 may read directly is
//                 gross_exposure_ — a single std::atomic<int64_t>."
//
//     Region is page-sized. First 8 bytes are gross_exposure_ (int64_t atomic).
//     L4 accesses this ONLY via CROSS_PLANE_READ (cross_plane.h, GC-E-045).
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kPositionStateRegionSize = kPageSize;
inline constexpr const char* kPositionStatePath = "/egbos_position_state";

static_assert(kPositionStateRegionSize >= sizeof(int64_t),
    "kPositionStateRegionSize must fit at least one int64_t (gross_exposure_)");
static_assert(kPositionStateRegionSize % kPageSize == 0,
    "kPositionStateRegionSize must be page-aligned for mlock(2) compatibility");

// ─────────────────────────────────────────────────────────────────────────────
// §5  /dev/shm/egbos_risk_limits  [L4 → L3]
//     Monotone latches: kill_switch, pos_limit.
//     Spec §6.2 (verbatim): "Monotone latches: kill_switch, pos_limit"
//
//     Monotone = latches only move in one direction (kill_switch: false→true,
//     pos_limit: only decreases). L3 polls these; latches are set by L4.
//     Region is page-sized for mlock compatibility.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kRiskLimitsRegionSize = kPageSize;
inline constexpr const char* kRiskLimitsPath = "/egbos_risk_limits";

static_assert(kRiskLimitsRegionSize >= sizeof(int64_t) + sizeof(bool),
    "kRiskLimitsRegionSize must fit kill_switch (bool) + pos_limit (int64_t)");
static_assert(kRiskLimitsRegionSize % kPageSize == 0,
    "kRiskLimitsRegionSize must be page-aligned for mlock(2) compatibility");

// ─────────────────────────────────────────────────────────────────────────────
// §6  /dev/shm/egbos_exec_commands  [L3 → L5]
//     SPSC ring, 4096 slots × 64 bytes = 256 KB total.
//     Spec §6.2 (verbatim): "SPSC ring, 4096 slots × 64B"
//     Spec §5.2: ring size must be power of 2.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kExecCommandsSlots    = 4096;
inline constexpr std::size_t kExecCommandsSlotSize = 64;    // bytes per slot
inline constexpr std::size_t kExecCommandsRegionSize =
    kExecCommandsSlots * kExecCommandsSlotSize;             // 262,144 bytes
inline constexpr const char* kExecCommandsPath = "/egbos_exec_commands";

static_assert((kExecCommandsSlots & (kExecCommandsSlots - 1)) == 0,
    "kExecCommandsSlots must be a power of 2 — "
    "SpscRing uses bitmask modulo; non-power-of-2 breaks correctness (AD-006)");
static_assert(kExecCommandsSlots > 0,
    "kExecCommandsSlots must be non-zero (AD-006)");
static_assert(kExecCommandsSlotSize > 0,
    "kExecCommandsSlotSize must be non-zero (AD-006)");

// ─────────────────────────────────────────────────────────────────────────────
// §7  /dev/shm/egbos_regime_signal  [L6/L7 → L3]
//     Redis Streams bridge output, 1 KB payload.
//     Spec §6.2 (verbatim): "Redis Streams bridge output, 1KB"
//
//     Region is page-sized (4096B) for mlock compatibility.
//     The 1KB payload occupies the first kRegimeSignalBytes bytes.
//     Remaining bytes in the page are reserved.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kRegimeSignalBytes      = 1024;  // 1 KB payload
inline constexpr std::size_t kRegimeSignalRegionSize = kPageSize;
inline constexpr const char* kRegimeSignalPath = "/egbos_regime_signal";

static_assert(kRegimeSignalBytes > 0,
    "kRegimeSignalBytes must be non-zero (AD-006)");
static_assert(kRegimeSignalRegionSize >= kRegimeSignalBytes,
    "kRegimeSignalRegionSize must fit the 1KB regime signal payload");
static_assert(kRegimeSignalRegionSize % kPageSize == 0,
    "kRegimeSignalRegionSize must be page-aligned for mlock(2) compatibility");

// ─────────────────────────────────────────────────────────────────────────────
// §8  /dev/shm/egbos_governance_flags  [All layers]
//     Read-only governance flags visible to all layers.
//     Spec §6.2 (verbatim): "Read-only: feed_stale, halt_required"
//
//     Region is page-sized. All layers read these flags; only the governance
//     subsystem writes them. Reads go through std::atomic<bool> acquire loads.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kGovernanceFlagsRegionSize = kPageSize;
inline constexpr const char* kGovernanceFlagsPath = "/egbos_governance_flags";

static_assert(kGovernanceFlagsRegionSize >= 2 * sizeof(bool),
    "kGovernanceFlagsRegionSize must fit feed_stale and halt_required");
static_assert(kGovernanceFlagsRegionSize % kPageSize == 0,
    "kGovernanceFlagsRegionSize must be page-aligned for mlock(2) compatibility");

// ─────────────────────────────────────────────────────────────────────────────
// Protocol structs — overlay on SHM region memory
//
// These structs are placed at the base address of their corresponding SHM
// region by shm_init_all(). All atomic members use the minimum required
// memory order per the seqlock / latch protocols in spec §6.3–§6.4.
// ─────────────────────────────────────────────────────────────────────────────

#include "spsc_ring.h"

// ── MarketEventsRegion ─────────────────────────────────────────────────────
// Layout at /dev/shm/egbos_market_events.
// Uses SpscRing which enforces alignas(64) on head and tail.
//
// Slot layout (128 bytes, ABI-stable):
//   bytes  0-63 : RawMarketEvent (FlatBuffers struct, exactly 64 bytes)
//   bytes 64-127: SlotFrameMetadata (frame-level sequencing data, 64 bytes)
//
// The upper 64 bytes were previously zeroed and unused.  SlotFrameMetadata
// occupies them to carry Binance U/u (first/final_update_id) needed for
// correct frame-level gap detection in L1Consumer.  The 128-byte ABI is
// preserved — no change to kMarketEventsSlotSize or SpscRing instantiation.
struct MarketEventSlot {
    alignas(128) uint8_t data[kMarketEventsSlotSize];
};
static_assert(kMarketEventsSlotSize == 128,
    "MarketEventSlot ABI contract: slot must be exactly 128 bytes");
static_assert(sizeof(egbos::events::RawMarketEvent) == 64,
    "RawMarketEvent must occupy exactly 64 bytes (lower half of MarketEventSlot)");

// ── SlotFrameMetadata ─────────────────────────────────────────────────────
// Occupies bytes 64-127 of MarketEventSlot (upper half).
// Written by FeedIngestor::normalize_and_forward for every real-event slot.
// Read by L1Consumer for frame-aware gap detection.
//
// Frame continuity rule (Binance WebSocket @depth):
//   Valid continuation : frame.first_update_id == prev_frame.final_update_id + 1
//   Same frame         : slot.final_update_id  == previous slot.final_update_id
//   Genuine gap        : frame.first_update_id != prev_frame.final_update_id + 1
//                        AND slot is from a different frame
//
// This struct is NOT part of the FlatBuffers schema.  It is EGBOS-internal
// slot metadata only and must not be persisted to Tier-0 in isolation.
struct SlotFrameMetadata {
    uint64_t first_update_id;    // Binance U: first update ID of this frame
    uint64_t final_update_id;    // Binance u: final update ID of this frame
                                 //   (== RawMarketEvent::sequence_num)
    uint32_t frame_level_index;  // 0-based index of this level within its frame
                                 //   (diagnostic only; not used for gap detection)
    uint8_t  _reserved[44];      // future use — must be zero
};
static_assert(sizeof(SlotFrameMetadata) == 64,
    "SlotFrameMetadata must be exactly 64 bytes (upper half of MarketEventSlot)");
static_assert(offsetof(SlotFrameMetadata, first_update_id)   == 0,  "layout check");
static_assert(offsetof(SlotFrameMetadata, final_update_id)   == 8,  "layout check");
static_assert(offsetof(SlotFrameMetadata, frame_level_index) == 16, "layout check");

// Returns a const reference to the SlotFrameMetadata embedded in a slot.
inline const SlotFrameMetadata& slot_frame_meta(const MarketEventSlot& slot) noexcept {
    return *reinterpret_cast<const SlotFrameMetadata*>(slot.data + 64);
}
// Mutable overload used by the ingestor writer.
inline SlotFrameMetadata& slot_frame_meta(MarketEventSlot& slot) noexcept {
    return *reinterpret_cast<SlotFrameMetadata*>(slot.data + 64);
}

using MarketEventsRegion = core::SpscRing<MarketEventSlot, kMarketEventsSlots>;

// ── LobSnapshotsRegion ─────────────────────────────────────────────────────
// Layout at /dev/shm/egbos_lob_snapshots.
// Double-buffered LOB using Seqlocks for wait-free L1 -> L2 read access.

struct LobSnapshotData {
    alignas(64) uint8_t payload[kLobSnapshotPerBufferBytes - 64]; 
};

struct LobSnapshotBuffer {
    // Seqlock protocol for this specific buffer
    alignas(64) std::atomic<uint64_t> sequence{0};
    LobSnapshotData data;
};

// Ensure the header occupies exactly one page before the buffers start.
struct alignas(kPageSize) LobSnapshotInstrumentHeader {
    // The active buffer index (0 or 1). L2 reads from this buffer.
    // L1 writes to the inactive buffer (active_buffer ^ 1), then flips this.
    std::atomic<uint8_t> active_buffer{0};
};

struct LobSnapshotInstrument {
    LobSnapshotInstrumentHeader header;
    LobSnapshotBuffer buffers[kLobSnapshotBuffers];
};

struct LobSnapshotsRegion {
    LobSnapshotInstrument instruments[kMaxInstruments];
};

static_assert(sizeof(LobSnapshotsRegion) <= kLobSnapshotsRegionSize,
    "LobSnapshotsRegion exceeds allocated SHM size");

// ── FeatureVectorRegion ────────────────────────────────────────────────────
//
// Layout at /dev/shm/egbos_feature_vector (kFeatureVectorRegionSize bytes).
// Implements the seqlock protocol from spec §6.4 verbatim (HTML §C3.4).
//
// Writer: L2 feature engine. Reader: L3 position engine.
// The seqlock guarantees that L3 never observes a torn write.
//
// Invariants:
//   - An odd sequence_ value means a write is in progress — reader must spin.
//   - An even sequence_ value means the data is complete and consistent.
//   - If seq1 != seq2 after the copy, the data was overwritten mid-read; retry.
struct FeatureVectorRegion {
    // Seqlock sequence counter. Even = consistent. Odd = write in progress.
    // Must be seq_cst on fetch_add so the fence below is sound.
    std::atomic<uint64_t> sequence{0};

    // Pad between sequence and data to prevent false-sharing between
    // the sequence counter (written by L2) and the data array (written by L2,
    // read by L3). Both accesses come from different cache lines this way.
    alignas(64) uint8_t data[kFeatureVectorBytes]{};

    // ── Writer (L2 feature engine) — spec §6.4 verbatim ───────────────────
    //
    // src must point to kFeatureVectorBytes bytes of valid feature data.
    // Caller must be the sole writer (SPSC-SINGLE enforced at layer level).
    // Does NOT call malloc/new (GC-E-010).
    void write(const uint8_t* src) noexcept {
        // Phase 1: Mark write in progress — sequence becomes odd.
        sequence.fetch_add(1, std::memory_order_seq_cst);

        // Phase 2: Copy feature data into the shared region.
        std::memcpy(data, src, kFeatureVectorBytes);

        // Phase 3: Full fence — ensures data copy is visible before sequence
        // increment. seq_cst fence is the strongest barrier; reader's acquire
        // fence pairs with this.
        std::atomic_thread_fence(std::memory_order_seq_cst);

        // Phase 4: Mark write complete — sequence becomes even again.
        sequence.fetch_add(1, std::memory_order_seq_cst);
    }

    // ── Reader (L3 position engine) — spec §6.4 verbatim ─────────────────
    //
    // dst must point to kFeatureVectorBytes bytes of caller-owned storage.
    // Spins until a consistent snapshot is obtained.
    // Does NOT call malloc/new (GC-E-010).
    void read(uint8_t* dst) const noexcept {
        uint64_t seq1;
        uint64_t seq2;
        do {
            // Observe sequence. Acquire ensures we see the writer's data.
            seq1 = sequence.load(std::memory_order_acquire);

            // Odd sequence = write in progress. Spin without copying.
            if (seq1 & 1u) {
                continue;
            }

            // Copy data while sequence is even (write may be in progress
            // on another writer thread — but seqlock has single-writer invariant,
            // so this branch is taken only when a write is not in flight).
            std::memcpy(dst, data, kFeatureVectorBytes);

            // Acquire fence — ensures the copy above is ordered before the
            // subsequent sequence load.
            std::atomic_thread_fence(std::memory_order_acquire);

            // Re-read sequence. If it changed, a write occurred during our copy
            // and the data may be torn — retry.
            seq2 = sequence.load(std::memory_order_acquire);
        } while (seq1 != seq2);
    }
};

static_assert(sizeof(FeatureVectorRegion) <=  kFeatureVectorRegionSize,
    "FeatureVectorRegion does not fit in kFeatureVectorRegionSize");

// ── FeatureSlotHeader ─────────────────────────────────────────────────────
//
// One cache line (64 bytes).  Sits at the base of every FeatureSlot.
// Written atomically under the seqlock protocol documented in §3b above.
//
// Byte map (all values little-endian):
//   [  0 –  7]  sequence       uint64_t  — seqlock counter (even=consistent)
//   [  8 – 11]  instrument_id  uint32_t  — identifies which instrument owns this slot
//   [ 12 ]      schema_version uint8_t   — reject if unrecognised
//   [ 13 ]      reserved_a     uint8_t   — must be 0
//   [ 14 – 15]  payload_count  uint16_t  — number of valid feature entries in payload
//   [ 16 – 23]  source_seq     uint64_t  — L2 snapshot sequence that produced these features
//   [ 24 – 31]  pub_ts_ns      uint64_t  — publication timestamp (CLOCK_MONOTONIC_RAW, ns)
//   [ 32 – 39]  validity_mask  uint64_t  — bit N=1 ↔ feature N is valid
//   [ 40 – 43]  payload_bytes        uint32_t    — meaningful bytes in payload (≤ kFeatureSlotPayloadBytes)
//   [ 44 – 51]  source_event_ts_ns   uint64_t    — L0 ingestion timestamp (CLOCK_MONOTONIC_RAW, ns)
//   [ 52 – 63]  reserved_b           uint8_t[12] — must be 0; forward-compatible growth
//
// Field ordering: largest-alignment types first to avoid implicit padding.
struct FeatureSlotHeader {
    std::atomic<uint64_t> sequence{0};   // bytes  0– 7: seqlock counter
    uint32_t  instrument_id{0};          // bytes  8–11
    uint8_t   schema_version{0};         // byte  12
    uint8_t   reserved_a{0};             // byte  13: must be 0
    uint16_t  payload_count{0};          // bytes 14–15: number of valid features
    uint64_t  source_seq{0};             // bytes 16–23: L1 snapshot seq that triggered this
    uint64_t  pub_ts_ns{0};             // bytes 24–31: CLOCK_MONOTONIC_RAW ns
    uint64_t  validity_mask{0};          // bytes 32–39: bit N=1 → feature N valid
    uint32_t  payload_bytes{0};          // bytes 40–43: meaningful bytes in payload
    uint64_t  source_event_ts_ns{0};    // bytes 44–51: L0 ingestion timestamp (CLOCK_MONOTONIC_RAW, ns)
    uint8_t   reserved_b[12]{};          // bytes 52–63: must be 0
};

static_assert(sizeof(FeatureSlotHeader) == 64,
    "FeatureSlotHeader must be exactly 64 bytes (one cache line)");
static_assert(std::is_standard_layout_v<FeatureSlotHeader>,
    "FeatureSlotHeader must be standard-layout for safe SHM overlay");
static_assert(alignof(FeatureSlotHeader) == alignof(std::atomic<uint64_t>),
    "FeatureSlotHeader alignment must match atomic<uint64_t>");
static_assert(offsetof(FeatureSlotHeader, instrument_id)  ==  8, "FeatureSlotHeader layout");
static_assert(offsetof(FeatureSlotHeader, schema_version) == 12, "FeatureSlotHeader layout");
static_assert(offsetof(FeatureSlotHeader, reserved_a)     == 13, "FeatureSlotHeader layout");
static_assert(offsetof(FeatureSlotHeader, payload_count)  == 14, "FeatureSlotHeader layout");
static_assert(offsetof(FeatureSlotHeader, source_seq)     == 16, "FeatureSlotHeader layout");
static_assert(offsetof(FeatureSlotHeader, pub_ts_ns)      == 24, "FeatureSlotHeader layout");
static_assert(offsetof(FeatureSlotHeader, validity_mask)  == 32, "FeatureSlotHeader layout");
static_assert(offsetof(FeatureSlotHeader, payload_bytes)         == 40, "FeatureSlotHeader layout");
static_assert(offsetof(FeatureSlotHeader, source_event_ts_ns)    == 44, "FeatureSlotHeader layout");
static_assert(offsetof(FeatureSlotHeader, reserved_b)            == 52, "FeatureSlotHeader layout");

// ── FeatureSlot ───────────────────────────────────────────────────────────
//
// One slot per instrument in FeatureRegion.  512 bytes, alignas(64).
// Provides writer and non-spinning reader APIs.
//
// NO dynamic allocation.  NO pointers.  NO floats for prices/quantities.
// Trivially copyable payload — safe to memcpy under seqlock.
struct alignas(64) FeatureSlot {
    FeatureSlotHeader header;
    uint8_t payload[kFeatureSlotPayloadBytes]{};

    // ── Writer API (L2 FeatureEngine — single writer per slot) ────────────
    //
    // publish() atomically commits header metadata and payload in one seqlock
    // transaction.  Caller must be the sole writer for this slot.
    // src_payload must point to at least src_payload_bytes bytes.
    // src_payload_bytes must be <= kFeatureSlotPayloadBytes.
    // Returns false if src_payload_bytes > kFeatureSlotPayloadBytes.
    bool publish(
        uint32_t  instrument_id,
        uint8_t   schema_ver,
        uint64_t  source_seq,
        uint64_t  pub_ts_ns,
        uint64_t  source_event_ts_ns,
        uint64_t  validity_mask,
        uint16_t  feature_count,
        const uint8_t* src_payload,
        uint32_t  src_payload_bytes) noexcept
    {
        if (src_payload_bytes > kFeatureSlotPayloadBytes) {
            return false;
        }
        if (src_payload == nullptr && src_payload_bytes != 0u) {
            return false;
        }

        // Phase 1: begin write — sequence becomes odd.
        header.sequence.fetch_add(1, std::memory_order_seq_cst);
        // Order the odd sequence store before all following data stores
        // (a seq_cst RMW does not order later stores on weak memory models).
        std::atomic_thread_fence(std::memory_order_release);

        // Phase 2: write metadata (non-atomic fields — protected by seqlock).
        header.instrument_id       = instrument_id;
        header.schema_version      = schema_ver;
        header.reserved_a          = 0u;
        header.payload_count       = feature_count;
        header.source_seq          = source_seq;
        header.pub_ts_ns           = pub_ts_ns;
        header.source_event_ts_ns  = source_event_ts_ns;
        header.validity_mask       = validity_mask;
        header.payload_bytes       = src_payload_bytes;
        std::memset(header.reserved_b, 0, sizeof(header.reserved_b));

        // Phase 3: write payload.
        if (src_payload && src_payload_bytes > 0) {
            std::memcpy(payload, src_payload, src_payload_bytes);
            if (src_payload_bytes < kFeatureSlotPayloadBytes) {
                std::memset(payload + src_payload_bytes, 0,
                            kFeatureSlotPayloadBytes - src_payload_bytes);
            }
        } else {
            std::memset(payload, 0, kFeatureSlotPayloadBytes);
        }

        // Phase 4: full fence then mark write complete — sequence becomes even.
        std::atomic_thread_fence(std::memory_order_seq_cst);
        header.sequence.fetch_add(1, std::memory_order_seq_cst);
        return true;
    }

    // ── Reader API (L3 DecisionEngine — non-spinning, returns false on torn) ──
    //
    // try_read() returns false if:
    //   (a) sequence == 0  → slot was never published
    //   (b) sequence & 1   → write in progress (torn) — caller decides to retry
    //   (c) seq1 != seq2   → write occurred during copy (torn)
    //   (d) out_schema_ver != expected_schema_ver  → caller must check
    //
    // On failure (returns false), all out_* fields and out_payload are
    // UNDEFINED and must not be used.
    // On success (returns true), all out_* fields are populated from a
    // consistent snapshot.  out_payload must point to kFeatureSlotPayloadBytes
    // bytes of caller-owned storage.
    [[nodiscard]] bool try_read(
        uint32_t  expected_instrument_id,
        uint32_t& out_instrument_id,
        uint8_t&  out_schema_ver,
        uint64_t& out_source_seq,
        uint64_t& out_pub_ts_ns,
        uint64_t& out_source_event_ts_ns,
        uint64_t& out_validity_mask,
        uint16_t& out_payload_count,
        uint32_t& out_payload_bytes,
        uint8_t*  out_payload) const noexcept
    {
        const uint64_t seq1 = header.sequence.load(std::memory_order_acquire);

        if (seq1 == 0u) return false;   // never published
        if (seq1 & 1u)  return false;   // write in progress

        // Copy non-atomic metadata.
        out_instrument_id       = header.instrument_id;
        out_schema_ver          = header.schema_version;
        out_source_seq          = header.source_seq;
        out_pub_ts_ns           = header.pub_ts_ns;
        out_source_event_ts_ns  = header.source_event_ts_ns;
        out_validity_mask       = header.validity_mask;
        out_payload_count       = header.payload_count;
        out_payload_bytes       = header.payload_bytes;
        if (out_payload) {
            std::memcpy(out_payload, payload, kFeatureSlotPayloadBytes);
        }

        std::atomic_thread_fence(std::memory_order_acquire);
        const uint64_t seq2 = header.sequence.load(std::memory_order_acquire);

        if (seq1 != seq2) return false; // torn — write happened during copy

        if (out_payload_bytes > kFeatureSlotPayloadBytes) {
            return false;
        }

        // Reject if instrument_id mismatch (detects slot aliasing bugs).
        if (out_instrument_id != expected_instrument_id) return false;

        return true;
    }
};

static_assert(sizeof(FeatureSlot) == kFeatureSlotBytes,
    "FeatureSlot must be exactly kFeatureSlotBytes (512) bytes");
static_assert(alignof(FeatureSlot) == 64,
    "FeatureSlot must be 64-byte aligned (cache-line)");
static_assert(std::is_standard_layout_v<FeatureSlot>,
    "FeatureSlot must be standard-layout for safe SHM overlay");
static_assert(offsetof(FeatureSlot, payload) == 64,
    "FeatureSlot::payload must start at byte 64 (after one header cache line)");

// ── FeatureRegion ─────────────────────────────────────────────────────────
//
// Layout at /dev/shm/egbos_feature_slots.
// One FeatureSlot per instrument.  L2 writes; L3 reads.
// Each slot is independent — per-instrument reads require no cross-slot lock.
//
// Instrument count == kMaxInstruments (compile-time checked).
// Total size: kMaxInstruments × kFeatureSlotBytes, page-rounded.
struct FeatureRegion {
    FeatureSlot slots[kMaxInstruments];
};

static_assert(sizeof(FeatureRegion) <= kFeatureRegionSize,
    "FeatureRegion exceeds kFeatureRegionSize — adjust kFeatureRegionSize");
static_assert(sizeof(FeatureRegion) == kMaxInstruments * kFeatureSlotBytes,
    "FeatureRegion size must equal kMaxInstruments * kFeatureSlotBytes");
static_assert(std::is_standard_layout_v<FeatureRegion>,
    "FeatureRegion must be standard-layout for safe SHM overlay");



// ── PositionStateView ──────────────────────────────────────────────────────
//
// Layout at /dev/shm/egbos_position_state (kPositionStateRegionSize bytes).
// L3 writes; L4 reads ONLY via CROSS_PLANE_READ (cross_plane.h, GC-E-045).
//
// Spec §6.3: "The only piece of L3 state that L4 may read directly is
// gross_exposure_ — a single std::atomic<int64_t>. All other L3 state is
// structurally inaccessible to L4."
//
// This struct contains ONLY gross_exposure_. Additional L3 state must NOT
// be placed here; GC-E-045 enforces this at Phase 4.
struct PositionStateView {
    // Net gross exposure in fixed-point units (PRICE_SCALE = 100,000).
    // L4 reads this via CROSS_PLANE_READ with memory_order_acquire.
    // L3 writes this via atomic store with memory_order_release.
    std::atomic<int64_t> gross_exposure_{0};

    // Remainder of the page is reserved — do NOT add fields without an ADR.
    // Adding fields here constitutes a change to the L3→L4 interface boundary
    // and must be accompanied by re-evaluation of CLAIM-WAL-001 dependencies.
    uint8_t _reserved[kPositionStateRegionSize - sizeof(std::atomic<int64_t>)]{};
};

static_assert(sizeof(PositionStateView) == kPositionStateRegionSize,
    "PositionStateView must fill exactly kPositionStateRegionSize bytes "
    "(reserved field must account for the full page)");

// ── RiskLimitsRegion ───────────────────────────────────────────────────────
//
// Layout at /dev/shm/egbos_risk_limits (kRiskLimitsRegionSize bytes).
// L4 writes; L3 reads. Latches are monotone:
//   kill_switch: false → true only (never reset without HITL)
//   pos_limit:   only decreases (never increases without HITL)
//
// Spec §6.2: "Monotone latches: kill_switch, pos_limit"
//
// Field ordering: int64_t first, bool second.
// Rationale: placing the largest-alignment field first eliminates any implicit
// compiler padding between members. The _reserved array calculation is then
// exact: sizeof(int64_t) + sizeof(bool) + sizeof(_reserved) = page size.
// The static_assert below verifies this on every target platform.
struct RiskLimitsRegion {
    // Position limit latch (fixed-point, PRICE_SCALE).
    // L3 enforces: abs(new_position) <= pos_limit.
    // L4 may only decrease this value (monotone property).
    // Placed first: alignof(int64_t) == 8; no padding needed before this field.
    std::atomic<int64_t> pos_limit{
        static_cast<int64_t>(1e15)};  // Large initial value — L4 constrains downward

    // Kill switch latch. Once set to true by L4, L3 halts immediately.
    // L3 reads this with acquire; L4 sets with release.
    // Placed second: alignof(bool) == 1; no padding needed after int64_t.
    std::atomic<bool> kill_switch{false};

    // Reserved. Do NOT add fields without an ADR.
    // Exact formula: kRiskLimitsRegionSize - sizeof(int64_t) - sizeof(bool).
    // No implicit padding exists between the two fields above (int64_t first).
    // The static_assert below enforces this on every target platform.
    uint8_t _reserved[
        kRiskLimitsRegionSize
        - sizeof(std::atomic<int64_t>)   // pos_limit: 8 bytes
        - sizeof(std::atomic<bool>)       // kill_switch: 1 byte
    ]{};
};

static_assert(sizeof(RiskLimitsRegion) == kRiskLimitsRegionSize,
    "RiskLimitsRegion must fill exactly kRiskLimitsRegionSize bytes. "
    "If this fires, the _reserved field calculation is wrong — "
    "check for unexpected compiler padding between members.");

// ── GovernanceFlags ────────────────────────────────────────────────────────
//
// Layout at /dev/shm/egbos_governance_flags (kGovernanceFlagsRegionSize bytes).
// Read-only from all execution-plane layers. Written by governance subsystem.
//
// Spec §6.2: "Read-only: feed_stale, halt_required"
// All layers check halt_required on every event loop iteration.
struct GovernanceFlags {
    // Market data feed is stale (no update within the staleness threshold).
    // All layers must observe this and cease trading until feed is live.
    std::atomic<bool> feed_stale{false};

    // Governance has determined that the engine must halt immediately.
    // All layers must observe this and transition to HALTED state.
    std::atomic<bool> halt_required{false};

    // Reserved. Do NOT add flags without an ADR.
    uint8_t _reserved[kGovernanceFlagsRegionSize
                      - 2 * sizeof(std::atomic<bool>)]{};
};

static_assert(sizeof(GovernanceFlags) == kGovernanceFlagsRegionSize,
    "GovernanceFlags must fill exactly kGovernanceFlagsRegionSize bytes");

// ─────────────────────────────────────────────────────────────────────────────
// shm_init_all() — Startup initialiser
//
// Opens, maps, and mlocks all eight SHM regions. Must be called ONCE at engine
// startup, before any SpscRing, WalWriter, or protocol struct is used.
//
// Failure model (spec §13.3, §19.3):
//   - shm_open failure   → TERMINAL_HALT (prints diagnostic, calls abort())
//   - mmap failure       → TERMINAL_HALT
//   - mlock failure      → TERMINAL_HALT (no page faults permitted on hot path)
//   - mbind failure      → TERMINAL_HALT (NUMA correctness required)
//
// This function does NOT call malloc or new (GC-E-010).
// It uses only POSIX syscalls and stack-local variables.
//
// Thread safety: NOT thread-safe. Must be called from the engine's single
// startup thread before any other threads are launched.
// ─────────────────────────────────────────────────────────────────────────────

// Internal helper — shared fatal abort path (avoids code duplication).
// Not part of the public interface; declared in anonymous namespace to give
// it internal linkage and prevent ODR collisions across translation units.
namespace {

[[noreturn]] inline void shm_fatal(const char* region, const char* op) noexcept {
    // Write to stderr without malloc. snprintf is safe here (startup only).
    char buf[512];
    if (std::strcmp(op, "mbind") == 0) {
        std::snprintf(buf, sizeof(buf),
            "EGBOS TERMINAL_HALT: shm_init_all() failed during %s on region %s "
            "(errno=%d). Capital safety enforced — engine will not start. "
            "NUMA topology diagnostic: check libnuma/numactl configuration or "
            "kernel NUMA support.\n",
            op, region, errno);
    } else {
        std::snprintf(buf, sizeof(buf),
            "EGBOS TERMINAL_HALT: shm_init_all() failed during %s on region %s "
            "(errno=%d). Capital safety enforced — engine will not start.\n",
            op, region, errno);
    }
    // write() is async-signal-safe; use it to avoid any buffering.
    (void)::write(STDERR_FILENO, buf, std::strlen(buf));
    std::abort();
}

// Opens one SHM region: shm_open → ftruncate → mmap → mlock.
// Returns the mapped address. Aborts on any failure.
// Does NOT call malloc or new.
[[nodiscard]] inline void* open_shm_region(
        const char* path,
        std::size_t size,
        bool        writable) noexcept
{
    const int flags = writable
                      ? (O_CREAT | O_RDWR)
                      : (O_CREAT | O_RDWR);  // Both sides open RDWR at init time;
                                              // read-only semantics are per-process.
    const int fd = ::shm_open(path, flags, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        shm_fatal(path, "shm_open");
    }

    // Set the region size. ftruncate is idempotent when the region already
    // exists at the correct size.
    if (::ftruncate(fd, static_cast<off_t>(size)) == -1) {
        ::close(fd);
        shm_fatal(path, "ftruncate");
    }

    // Map into process address space.
    void* const addr = ::mmap(
        nullptr,
        size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0);
    if (addr == MAP_FAILED) {
        ::close(fd);
        shm_fatal(path, "mmap");
    }

    // fd is no longer needed after mmap; the mapping persists.
    ::close(fd);

    // Lock all pages into RAM — no page faults on the hot path (SHM-MLOCK).
    if (::mlock(addr, size) == -1) {
        // mlock may fail with EPERM if RLIMIT_MEMLOCK is insufficient.
        // The engine must not start without locked memory.
        shm_fatal(path, "mlock");
    }

    // NUMA binding (SHM-NUMA): mbind() pins the pages to the local NUMA node.
    // MPOL_PREFERRED causes the kernel to prefer local allocation.
    // MPOL_MF_MOVE moves any already-allocated pages to the local node.
    //
    // Note: mbind() requires <numaif.h> on glibc systems. We call the syscall
    // directly via syscall(2) to avoid a hard dependency on libnuma.
    // sys/syscall.h defines __NR_mbind; kernel ABI is stable.
#if defined(__linux__)
    {
        // MPOL_PREFERRED = 1, MPOL_MF_MOVE = 2. We hardcode these to avoid
        // including linux/mempolicy.h directly.
        const int mpol_preferred = 1;
        const int mpol_mf_move   = 2;

        long ret = ::syscall(__NR_mbind, addr, size, mpol_preferred, nullptr, 0, mpol_mf_move);
        if (ret == -1) {
            shm_fatal(path, "mbind");
        }
    }
#endif

    return addr;
}

}  // anonymous namespace

// ── ShmRegionPointers ────────────────────────────────────────────────────────
//
// Returned by shm_init_all(). Contains the base addresses of all eight mapped
// SHM regions. The caller uses these to construct the SpscRings and protocol
// structs.
struct ShmRegionPointers {
    void* market_events;
    void* lob_snapshots;
    void* feature_vector;   // legacy single-slot (preserved for compat)
    void* feature_slots;    // per-instrument production FeatureRegion (§3b)
    void* position_state;
    void* risk_limits;
    void* exec_commands;
    void* regime_signal;
    void* governance_flags;
};

// shm_init_all — opens and locks all nine SHM regions.
// Must be called once at startup. Fatal on any failure.
inline ShmRegionPointers shm_init_all() noexcept {
    // Open all nine regions in the canonical order from spec §6.2.
    // Each call to open_shm_region() is fatal on failure — this function
    // returns only when ALL regions are successfully mapped and locked.

    ShmRegionPointers ptrs{};

    ptrs.market_events    = open_shm_region(kMarketEventsPath,     kMarketEventsRegionSize,     true);
    ptrs.lob_snapshots    = open_shm_region(kLobSnapshotsPath,     kLobSnapshotsRegionSize,     true);
    ptrs.feature_vector   = open_shm_region(kFeatureVectorPath,    kFeatureVectorRegionSize,    true);
    ptrs.feature_slots    = open_shm_region(kFeatureSlotsPath,     kFeatureRegionSize,          true);
    ptrs.position_state   = open_shm_region(kPositionStatePath,    kPositionStateRegionSize,    true);
    ptrs.risk_limits      = open_shm_region(kRiskLimitsPath,       kRiskLimitsRegionSize,       true);
    ptrs.exec_commands    = open_shm_region(kExecCommandsPath,     kExecCommandsRegionSize,     true);
    ptrs.regime_signal    = open_shm_region(kRegimeSignalPath,     kRegimeSignalRegionSize,     true);
    ptrs.governance_flags = open_shm_region(kGovernanceFlagsPath,  kGovernanceFlagsRegionSize,  true);

    // All nine regions opened, mapped, and mlocked.
    return ptrs;
}

// ─────────────────────────────────────────────────────────────────────────────
// Compile-time totals — for documentation and regression detection.
// These are not used at runtime; they exist solely to make size changes
// immediately visible in the build output (static_assert failure message
// will show the new total, making the change intentional-or-bug obvious).
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kTotalShmBytes =
    kMarketEventsRegionSize    +   //  8,388,608 bytes  (65536 × 128)
    kLobSnapshotsRegionSize    +   //  2,097,152 bytes  (2 × 256 × 4096)
    kFeatureVectorRegionSize   +   //      4,096 bytes  (legacy single-slot)
    kFeatureRegionSize         +   //    131,072 bytes  (256 × 512 per-instrument slots)
    kPositionStateRegionSize   +   //      4,096 bytes
    kRiskLimitsRegionSize      +   //      4,096 bytes
    kExecCommandsRegionSize    +   //    262,144 bytes  (4096 × 64)
    kRegimeSignalRegionSize    +   //      4,096 bytes
    kGovernanceFlagsRegionSize;    //      4,096 bytes
                                   // ──────────────────
                                   // = 10,899,456 bytes (≈ 10.39 MiB) total SHM

static_assert(kTotalShmBytes > 0,
    "kTotalShmBytes must be non-zero — all regions accounted for");

}  // namespace egbos::shm
