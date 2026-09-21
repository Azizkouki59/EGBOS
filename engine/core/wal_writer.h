// engine/core/wal_writer.h
// EGBOS ENGINE_V3 — Write-Ahead Log Writer
//
// Specification  : EGBOS-SPEC-S1-001 §4.1–§4.8, §13.1, §14.1, §19.1
// ADR compliance : AD-005 (io_uring single-CQE-consumer — one thread only)
//                  AD-012 (WAL-before-state ordering)
// Claims         : CLAIM-WAL-001 (wal.durability, V4)
//
// Govcheck       : GC-E-010 (Warning) — no malloc/new/free/delete in this file
//                  GC-E-011 (pre-registered) — no CLOCK_REALTIME in this file
//                  GC-WAL-001 (Blocking) — write_sync blocks until O_DSYNC returns
//
// Thread safety  : The WalWriter is single-threaded. Only the WAL writer thread
//                  may call write_sync() or any other non-const method. This is
//                  enforced by ASSERT_WAL_THREAD() on every CQE consumption (AD-005).
//
// Failure model  : All failures on the write path are TERMINAL_HALT (P-006).
//                  Short writes, io_uring errors, and I/O errors are fatal.
//                  The engine does not attempt recovery at runtime.
//
// Physical topology (§4.2):
//   WAL Primary: NVMe Gen4 — per-record O_DSYNC (authoritative recovery source)
//   WAL Shadow:  Second NVMe — async mirror (100ms lag tolerance)
//
// Forbidden      : CLOCK_REALTIME — not used (GC-E-011 compliant).
//                  malloc/new/free/delete — structurally absent (GC-E-010).

#pragma once

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/io_uring.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// io_uring syscall number fallbacks (kernel 5.1+, x86_64)
// ─────────────────────────────────────────────────────────────────────────────
#if defined(__linux__)
#ifndef __NR_io_uring_setup
#define __NR_io_uring_setup 425
#endif
#ifndef __NR_io_uring_enter
#define __NR_io_uring_enter 426
#endif
#ifndef __NR_io_uring_register
#define __NR_io_uring_register 427
#endif
#ifndef IORING_FEAT_SINGLE_MMAP
#define IORING_FEAT_SINGLE_MMAP (1U << 0)
#endif
#endif  // __linux__

namespace egbos::wal {

// ─────────────────────────────────────────────────────────────────────────────
// Constants (spec §4.3)
//
// PRICE_SCALE is a compile-time constant. Changing it requires an ADR.
// Floating-point is structurally forbidden in all WAL price fields.
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr int64_t kPriceScale = 100'000;

inline constexpr std::size_t kMaxPayloadSize = 256;

// ─────────────────────────────────────────────────────────────────────────────
// Record Type Enumeration (spec §4.4)
//
// Additional types in later slices require YAML enum registration —
// they may not be added ad hoc.
// ─────────────────────────────────────────────────────────────────────────────

enum class WalRecordType : uint32_t {
    ORDER_NEW                     = 1,   // S3
    FILL_APPLIED                  = 2,   // S3
    POSITION_SYNC                 = 3,   // S3
    MASS_CANCEL                   = 4,   // S3
    REGIME_CHANGE                 = 5,   // S3+
    SESSION_START                 = 6,   // S1 — clean-startup marker
    HITL_HALT_CONFIRM             = 7,   // S4+
    HITL_RESTART_AUTH              = 8,   // S4+
    HITL_RISK_LIMIT_CHANGE        = 9,   // S4+
    HITL_SIGNAL_WEIGHT_CHANGE     = 10,  // S4+
    HITL_REGISTRY_EMERGENCY_AMEND = 11,  // S4+
    CAPITAL_GATE_CLOCK_START      = 12,  // S7
};

// ─────────────────────────────────────────────────────────────────────────────
// WAL Record (spec §4.3 — canonical schema)
//
// Fixed-size header followed by variable payload (max 256 bytes).
// CRC32C covers bytes [0, offsetof(header_crc32c)) = [0, 60).
// tsc_after_write is populated in-memory after O_DSYNC return;
// its on-disk value is 0 (set before CRC computation).
// ─────────────────────────────────────────────────────────────────────────────

struct WalRecord {
    // Header fields — CRC32C covers all fields before header_crc32c
    uint64_t sequence;          // Monotonically increasing; never reset
    uint64_t tsc_before_write;  // CLOCK_MONOTONIC_RAW before O_DSYNC
    uint64_t tsc_after_write;   // CLOCK_MONOTONIC_RAW after O_DSYNC (latency audit)
    uint32_t record_type;       // WalRecordType enum value
    uint32_t instrument_id;     // Instrument universe ID
    uint64_t order_id;          // 0 for non-order records
    int64_t  quantity_delta;    // Signed — positive=buy, negative=sell
    int64_t  price;             // Fixed-point kPriceScale
    uint32_t payload_length;    // 0–256 bytes
    uint32_t header_crc32c;     // CRC32C of bytes [0, 60) of this header
    uint8_t  _pad[8];           // Pad (spec §4.3 canonical schema)

    // Payload (flexible array; payload_crc32c is payload[last-4..last])
    uint8_t payload[0];  // NOLINT(modernize-avoid-c-arrays)
};

// ── Struct layout verification ───────────────────────────────────────────────
// These static_asserts catch field reordering or sizing errors that would
// corrupt the wire format or CRC computation.

static_assert(offsetof(WalRecord, sequence) == 0, "sequence at offset 0");
static_assert(offsetof(WalRecord, tsc_before_write) == 8,
              "tsc_before_write at offset 8");
static_assert(offsetof(WalRecord, tsc_after_write) == 16,
              "tsc_after_write at offset 16");
static_assert(offsetof(WalRecord, record_type) == 24, "record_type at offset 24");
static_assert(offsetof(WalRecord, instrument_id) == 28,
              "instrument_id at offset 28");
static_assert(offsetof(WalRecord, order_id) == 32, "order_id at offset 32");
static_assert(offsetof(WalRecord, quantity_delta) == 40,
              "quantity_delta at offset 40");
static_assert(offsetof(WalRecord, price) == 48, "price at offset 48");
static_assert(offsetof(WalRecord, payload_length) == 56,
              "payload_length at offset 56");
static_assert(offsetof(WalRecord, header_crc32c) == 60,
              "header_crc32c at offset 60 — CRC covers bytes [0, 60)");
static_assert(offsetof(WalRecord, _pad) == 64, "_pad at offset 64");
static_assert(offsetof(WalRecord, payload) == 72, "payload at offset 72");
static_assert(std::is_trivially_copyable_v<WalRecord>,
              "WalRecord must be trivially copyable for memcpy on write path");

// CRC32C coverage: bytes [0, kWalCrcCoverageBytes)
inline constexpr std::size_t kWalCrcCoverageBytes =
    offsetof(WalRecord, header_crc32c);
static_assert(kWalCrcCoverageBytes == 60,
              "CRC covers first 60 bytes of header");

// Header wire size (all fields including _pad, excluding flexible payload)
inline constexpr std::size_t kWalHeaderWireSize = offsetof(WalRecord, payload);
static_assert(kWalHeaderWireSize == 72, "Header wire size is 72 bytes");

// Maximum total wire size for one WAL record
inline constexpr std::size_t kMaxWalRecordWireSize =
    kWalHeaderWireSize + kMaxPayloadSize;

// ─────────────────────────────────────────────────────────────────────────────
// CRC32C Computation (Castagnoli polynomial 0x82F63B78)
//
// Software implementation with a compile-time generated lookup table.
// No malloc, no runtime initialisation. Pure constexpr table.
// ─────────────────────────────────────────────────────────────────────────────

namespace detail {

inline constexpr uint32_t kCrc32cPoly = 0x82F63B78;

constexpr uint32_t crc32c_table_entry(uint32_t idx) noexcept {
    uint32_t crc = idx;
    for (int j = 0; j < 8; ++j) {
        crc = (crc & 1U) ? ((crc >> 1) ^ kCrc32cPoly) : (crc >> 1);
    }
    return crc;
}

struct Crc32cTable {
    uint32_t entries[256];
    constexpr Crc32cTable() noexcept : entries{} {
        for (uint32_t i = 0; i < 256; ++i) {
            entries[i] = crc32c_table_entry(i);
        }
    }
};

inline constexpr Crc32cTable kCrc32cTab{};

inline uint32_t compute_crc32c(const void* data, std::size_t len) noexcept {
    uint32_t crc = 0xFFFFFFFF;
    const auto* p = static_cast<const uint8_t*>(data);
    for (std::size_t i = 0; i < len; ++i) {
        crc = kCrc32cTab.entries[(crc ^ p[i]) & 0xFFU] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ─────────────────────────────────────────────────────────────────────────────
// Minimal io_uring wrapper state — synchronous WRITE_FIXED only.
// Uses raw Linux syscalls. No liburing dependency. (spec §4.5, AD-005)
// ─────────────────────────────────────────────────────────────────────────────

#if defined(__linux__)

struct UringState {
    int fd{-1};

    // SQ ring
    void* sq_ring_ptr{nullptr};
    std::size_t sq_ring_sz{0};
    unsigned* sq_head{nullptr};
    unsigned* sq_tail{nullptr};
    unsigned* sq_mask{nullptr};
    unsigned* sq_array{nullptr};

    // CQ ring
    void* cq_ring_ptr{nullptr};
    std::size_t cq_ring_sz{0};
    unsigned* cq_head{nullptr};
    unsigned* cq_tail{nullptr};
    unsigned* cq_mask{nullptr};
    struct io_uring_cqe* cqes{nullptr};

    // SQEs
    struct io_uring_sqe* sqes{nullptr};
    void* sqes_ptr{nullptr};
    std::size_t sqes_sz{0};

    // Cleanup tracking
    bool single_mmap{false};
};

#endif  // __linux__

}  // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers — anonymous namespace for internal linkage per TU.
// Matches project convention (cf. shm_layout.h, pool_allocator.h).
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// TERMINAL_HALT — fatal WAL error (spec §19.1, P-006)
// Capital safety over continuity. No recovery attempted at runtime.
[[noreturn]] inline void wal_fatal(const char* op) noexcept {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "EGBOS TERMINAL_HALT: WAL writer failed during %s "
                  "(errno=%d). Capital safety enforced — engine halted.\n",
                  op, errno);
    // Write directly to stderr fd — no heap allocation
    (void)::write(STDERR_FILENO, buf, std::strlen(buf));
    std::abort();
}

#if defined(__linux__)

// Timestamp helper — CLOCK_MONOTONIC_RAW (GC-E-011 compliant, §4.3)
// CLOCK_REALTIME is forbidden.
inline uint64_t monotonic_raw_ns() noexcept {
    struct timespec ts;
    ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
}

// ── io_uring lifecycle helpers ───────────────────────────────────────────────

inline void uring_init(detail::UringState& u, unsigned entries) noexcept {
    struct io_uring_params params {};
    int fd = static_cast<int>(
        ::syscall(__NR_io_uring_setup, entries, &params));
    if (fd < 0) {
        wal_fatal("io_uring_setup");
    }
    u.fd = fd;

    // Calculate mmap sizes from kernel-returned offsets
    u.sq_ring_sz =
        params.sq_off.array + params.sq_entries * sizeof(unsigned);
    u.cq_ring_sz =
        params.cq_off.cqes + params.cq_entries * sizeof(struct io_uring_cqe);
    u.sqes_sz = params.sq_entries * sizeof(struct io_uring_sqe);

    // Handle IORING_FEAT_SINGLE_MMAP (kernel 5.4+)
    u.single_mmap = (params.features & IORING_FEAT_SINGLE_MMAP) != 0;
    if (u.single_mmap) {
        if (u.cq_ring_sz > u.sq_ring_sz) {
            u.sq_ring_sz = u.cq_ring_sz;
        }
        u.cq_ring_sz = u.sq_ring_sz;
    }

    // Map SQ ring
    u.sq_ring_ptr = ::mmap(nullptr, u.sq_ring_sz,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_POPULATE, fd,
                           IORING_OFF_SQ_RING);
    if (u.sq_ring_ptr == MAP_FAILED) {
        wal_fatal("mmap(sq_ring)");
    }

    // Map CQ ring
    if (u.single_mmap) {
        u.cq_ring_ptr = u.sq_ring_ptr;
    } else {
        u.cq_ring_ptr = ::mmap(nullptr, u.cq_ring_sz,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_POPULATE, fd,
                               IORING_OFF_CQ_RING);
        if (u.cq_ring_ptr == MAP_FAILED) {
            wal_fatal("mmap(cq_ring)");
        }
    }

    // Map SQE array
    u.sqes_ptr = ::mmap(nullptr, u.sqes_sz,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_POPULATE, fd,
                        IORING_OFF_SQES);
    if (u.sqes_ptr == MAP_FAILED) {
        wal_fatal("mmap(sqes)");
    }
    u.sqes = static_cast<struct io_uring_sqe*>(u.sqes_ptr);

    // Resolve SQ ring pointers from kernel-supplied offsets
    auto* sq = static_cast<uint8_t*>(u.sq_ring_ptr);
    u.sq_head = reinterpret_cast<unsigned*>(sq + params.sq_off.head);
    u.sq_tail = reinterpret_cast<unsigned*>(sq + params.sq_off.tail);
    u.sq_mask = reinterpret_cast<unsigned*>(sq + params.sq_off.ring_mask);
    u.sq_array = reinterpret_cast<unsigned*>(sq + params.sq_off.array);

    // Resolve CQ ring pointers from kernel-supplied offsets
    auto* cq = static_cast<uint8_t*>(u.cq_ring_ptr);
    u.cq_head = reinterpret_cast<unsigned*>(cq + params.cq_off.head);
    u.cq_tail = reinterpret_cast<unsigned*>(cq + params.cq_off.tail);
    u.cq_mask = reinterpret_cast<unsigned*>(cq + params.cq_off.ring_mask);
    u.cqes = reinterpret_cast<struct io_uring_cqe*>(
        cq + params.cq_off.cqes);
}

inline void uring_register_buffers(detail::UringState& u,
                                   struct iovec* iovs,
                                   unsigned nr) noexcept {
    long ret = ::syscall(__NR_io_uring_register, u.fd,
                         IORING_REGISTER_BUFFERS, iovs, nr);
    if (ret < 0) {
        wal_fatal("io_uring_register(BUFFERS)");
    }
}

// Submit a WRITE_FIXED SQE and wait for exactly one CQE.
// Returns the CQE result (bytes written on success, negative errno on error).
inline int32_t uring_write_fixed_sync(detail::UringState& u,
                                      int fd,
                                      const void* buf,
                                      uint32_t len,
                                      uint64_t offset,
                                      uint16_t buf_index) noexcept {
    // Get current SQ tail
    unsigned tail = __atomic_load_n(u.sq_tail, __ATOMIC_RELAXED);
    unsigned mask = *u.sq_mask;
    unsigned idx = tail & mask;

    // Fill SQE — IORING_OP_WRITE_FIXED (spec §4.5)
    struct io_uring_sqe* sqe = &u.sqes[idx];
    std::memset(sqe, 0, sizeof(*sqe));
    sqe->opcode = IORING_OP_WRITE_FIXED;
    sqe->fd = fd;
    sqe->off = offset;
    sqe->addr = reinterpret_cast<uint64_t>(buf);
    sqe->len = len;
    sqe->buf_index = buf_index;
    sqe->user_data = 1;

    // Map SQ index and advance tail (release — kernel sees SQE)
    u.sq_array[idx] = idx;
    __atomic_store_n(u.sq_tail, tail + 1, __ATOMIC_RELEASE);

    // Submit 1 SQE and wait for 1 CQE (synchronous)
    long ret = ::syscall(__NR_io_uring_enter, u.fd,
                         1U,   // to_submit
                         1U,   // min_complete
                         IORING_ENTER_GETEVENTS,
                         nullptr, static_cast<std::size_t>(0));
    if (ret < 0) {
        return static_cast<int32_t>(-errno);
    }

    // Read CQE result
    unsigned ch = __atomic_load_n(u.cq_head, __ATOMIC_ACQUIRE);
    unsigned ct = __atomic_load_n(u.cq_tail, __ATOMIC_ACQUIRE);
    if (ch == ct) {
        return -1;  // No CQE (should not happen with min_complete=1)
    }

    unsigned ci = ch & *u.cq_mask;
    int32_t result = u.cqes[ci].res;

    // Advance CQ head (release — kernel can reuse CQE slot)
    __atomic_store_n(u.cq_head, ch + 1, __ATOMIC_RELEASE);

    return result;
}

inline void uring_destroy(detail::UringState& u) noexcept {
    if (u.sqes_ptr != nullptr && u.sqes_ptr != MAP_FAILED) {
        ::munmap(u.sqes_ptr, u.sqes_sz);
    }
    if (!u.single_mmap && u.cq_ring_ptr != nullptr &&
        u.cq_ring_ptr != MAP_FAILED) {
        ::munmap(u.cq_ring_ptr, u.cq_ring_sz);
    }
    if (u.sq_ring_ptr != nullptr && u.sq_ring_ptr != MAP_FAILED) {
        ::munmap(u.sq_ring_ptr, u.sq_ring_sz);
    }
    if (u.fd >= 0) {
        ::close(u.fd);
    }
    u.fd = -1;
}

#endif  // __linux__

}  // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// ASSERT_WAL_THREAD macro (spec §4.5, §14.1)
//
// In DEBUG builds: runtime thread identity check on every CQE consumption.
// In release builds (NDEBUG): compiles to nothing ("compile-time macro").
// Violation at runtime triggers TERMINAL_HALT (P-006).
// ─────────────────────────────────────────────────────────────────────────────

#ifdef NDEBUG
#define ASSERT_WAL_THREAD() ((void)0)
#else
#define ASSERT_WAL_THREAD() assert_wal_thread_impl()
#endif

// ─────────────────────────────────────────────────────────────────────────────
// WalWriter (spec §14.1)
//
// Thread-safe: NO — single-threaded by design (AD-005).
// Heap allocation: NONE on the hot path (GC-E-010).
// CLOCK_REALTIME: NONE (GC-E-011).
//
// The write path:
//   1. Set sequence (WAL-SEQ: monotonically increasing, never reset)
//   2. Record tsc_before_write (CLOCK_MONOTONIC_RAW)
//   3. Compute CRC32C over header bytes [0, 60)
//   4. Copy record to registered io_uring buffer
//   5. Submit IORING_OP_WRITE_FIXED and wait for CQE
//   6. Verify full write (short write = TERMINAL_HALT)
//   7. Record tsc_after_write (latency audit)
//   8. Write to shadow (async mirror, best-effort)
// ─────────────────────────────────────────────────────────────────────────────

class WalWriter {
public:
    // ── Recovery result from WAL scan (spec §4.7) ────────────────────────────

    struct RecoveryResult {
        uint64_t valid_records;           // Number of valid records found
        uint64_t truncation_byte_offset;  // File offset of truncation point
        uint64_t last_valid_sequence;     // Sequence number of last valid record
        bool was_clean;  // true if WAL ended without corruption/gaps
    };

    // ── Constructor ──────────────────────────────────────────────────────────
    // Opens WAL primary (O_DSYNC) and shadow files, initialises io_uring,
    // scans existing WAL for sequence continuity.
    // Fatal on any setup failure (spec §19.1).

#if defined(__linux__)
    WalWriter(const char* primary_path, const char* shadow_path) noexcept {
        // 1. Open primary WAL file with O_DSYNC (spec §4.5)
        //    After write() returns, data is on stable storage.
        primary_fd_ = ::open(primary_path, O_RDWR | O_CREAT | O_DSYNC,
                             S_IRUSR | S_IWUSR);
        if (primary_fd_ < 0) {
            wal_fatal("open(primary WAL with O_DSYNC)");
        }

        // 2. Open shadow WAL file — no O_DSYNC (spec §4.2: async mirror)
        shadow_fd_ =
            ::open(shadow_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (shadow_fd_ < 0) {
            wal_fatal("open(shadow WAL)");
        }

        // 3. Scan primary WAL for last valid sequence and write offset
        //    (spec §4.7: recovery protocol steps 1–2)
        RecoveryResult scan = scan_wal_fd(primary_fd_);
        sequence_ = (scan.valid_records > 0)
                        ? (scan.last_valid_sequence + 1)
                        : 0;
        write_offset_ = scan.truncation_byte_offset;

        // 4. Truncate file at the valid boundary — discard trailing
        //    corruption (spec §4.7 step 1: "All records after are discarded")
        struct stat st;
        if (::fstat(primary_fd_, &st) == 0) {
            if (static_cast<uint64_t>(st.st_size) > write_offset_) {
                if (::ftruncate(primary_fd_,
                                static_cast<off_t>(write_offset_)) != 0) {
                    wal_fatal("ftruncate(primary WAL at truncation point)");
                }
            }
        }

        // Align shadow to primary truncation point
        if (::fstat(shadow_fd_, &st) == 0) {
            if (static_cast<uint64_t>(st.st_size) > write_offset_) {
                (void)::ftruncate(shadow_fd_,
                                  static_cast<off_t>(write_offset_));
            }
        }

        // 5. Initialise io_uring (spec §4.5, AD-005)
        //    4 SQ entries — only 1 used at a time for synchronous writes
        uring_init(uring_, 4);

        // 6. Register write buffer for IORING_OP_WRITE_FIXED
        //    (spec §4.5: "kernel-bypass buffer registration")
        registered_iov_.iov_base = write_buffer_;
        registered_iov_.iov_len = sizeof(write_buffer_);
        uring_register_buffers(uring_, &registered_iov_, 1);

        // 7. Record WAL writer thread identity (AD-005)
        //    First call to ASSERT_WAL_THREAD() registers the thread.
    }
#endif  // __linux__

    // ── Destructor ───────────────────────────────────────────────────────────

#if defined(__linux__)
    ~WalWriter() noexcept {
        uring_destroy(uring_);
        if (shadow_fd_ >= 0) {
            ::close(shadow_fd_);
        }
        if (primary_fd_ >= 0) {
            ::close(primary_fd_);
        }
    }
#endif  // __linux__

    // Non-copyable, non-moveable
    WalWriter(const WalWriter&) = delete;
    WalWriter& operator=(const WalWriter&) = delete;
    WalWriter(WalWriter&&) = delete;
    WalWriter& operator=(WalWriter&&) = delete;

    // ── write_sync (spec §14.1) ──────────────────────────────────────────────
    // Writes one WAL record to stable storage. Blocks until O_DSYNC returns.
    //
    // Sets: record->sequence, record->tsc_before_write, record->tsc_after_write,
    //       record->header_crc32c
    //
    // Short write or io_uring error: TERMINAL_HALT (spec §19.1).

#if defined(__linux__)
    void write_sync(WalRecord* record) noexcept {
        ASSERT_WAL_THREAD();

        // ── Step 1: Set sequence (WAL-SEQ invariant) ─────────────────────────
        // Monotonically increasing, never reset, never reused (spec §4.8).
        record->sequence = sequence_++;

        // ── Step 2: Timestamp before write (spec §4.3) ───────────────────────
        // CLOCK_MONOTONIC_RAW — CLOCK_REALTIME forbidden (GC-E-011).
        record->tsc_before_write = monotonic_raw_ns();

        // tsc_after_write = 0 for CRC computation; it is populated after
        // O_DSYNC returns (the on-disk value is always 0).
        record->tsc_after_write = 0;

        // Clear _pad for deterministic wire format
        std::memset(record->_pad, 0, sizeof(record->_pad));

        // ── Validate payload_length (spec §4.3: 0–256 bytes) ──────────────
        // Runtime check — always active regardless of NDEBUG (GC-WAL-BUG1).
        if (record->payload_length > kMaxPayloadSize) {
            wal_fatal("write_sync: payload_length exceeds kMaxPayloadSize");
        }

        // ── Step 2b: Payload CRC32C (spec §4.3 payload[last-4..last]) ───────
        // If payload_length >= 4: CRC32C of first (payload_length - 4) bytes
        // is written into the last 4 bytes of the payload, little-endian.
        // If payload_length < 4: no payload CRC is possible — only the header
        // CRC32C (bytes [0, 60)) protects the record. See static_assert below.
        //
        // static_assert: payload_length is a runtime value; we document here
        // that the minimum payload size required for CRC coverage is 4 bytes.
        // Records with payload_length in [0, 3] are protected by header CRC
        // only — this is intentional per spec §4.3.
        // Bound: payload_length <= kMaxPayloadSize (checked above), so
        // crc_cover_len + 4 <= 256. Caller must back the record with at least
        // kWalHeaderWireSize + payload_length bytes. payload[0] is a flexible
        // array, so -Wstringop-overflow (size 0) is a false positive when
        // payload_length is a runtime value. All current callers use 0.
        if (record->payload_length >= 4) {
            const uint32_t crc_cover_len = record->payload_length - 4;
            const uint32_t payload_crc = detail::compute_crc32c(
                record->payload, crc_cover_len);
            // Write CRC32C in the last 4 bytes of the payload (little-endian)
            std::memcpy(record->payload + crc_cover_len,
                        &payload_crc, sizeof(payload_crc));
        }

        // ── Step 3: Compute header CRC32C (spec §4.3, §14.1) ────────────────
        // CRC covers header bytes [0, 60) — all fields before header_crc32c.
        record->header_crc32c =
            detail::compute_crc32c(record, kWalCrcCoverageBytes);

        // ── Step 4: Copy to registered io_uring buffer ───────────────────────
        const uint32_t wire_size = static_cast<uint32_t>(
            kWalHeaderWireSize + record->payload_length);
        std::memcpy(write_buffer_, record, wire_size);

        // ── Step 5: Submit WRITE_FIXED and wait for CQE (spec §4.5) ─────────
        const uint64_t current_offset = write_offset_;
        int32_t result = uring_write_fixed_sync(
            uring_, primary_fd_, write_buffer_, wire_size, current_offset, 0);

        // ── Step 6: Verify full write (spec §14.1, §19.1) ───────────────────
        // "Return value must equal buffer length; short writes are fatal
        //  errors — not retryable conditions." (spec §4.5)
        if (result != static_cast<int32_t>(wire_size)) {
            wal_fatal("write_sync: short write or io_uring error");
        }

        // Advance write offset
        write_offset_ += wire_size;

        // ── Step 7: Timestamp after write (spec §4.3) ────────────────────────
        // Populated in-memory for latency auditing (WAL-ORDER invariant).
        record->tsc_after_write = monotonic_raw_ns();

        // ── Step 8: Shadow mirror (spec §4.2) ────────────────────────────────
        // Async write to shadow — best-effort. Shadow failure is a P1 alert,
        // not TERMINAL_HALT.
        write_shadow(write_buffer_, wire_size, current_offset);
    }
#endif  // __linux__

    // ── start_session (spec §4.4) ────────────────────────────────────────────
    // Writes a SESSION_START record — the clean-startup marker enabling
    // recovery to distinguish clean vs unclean shutdown.

#if defined(__linux__)
    void start_session() noexcept {
        // Stack-local buffer — no payload, no heap allocation
        alignas(8) uint8_t buf[kWalHeaderWireSize]{};
        auto* rec = reinterpret_cast<WalRecord*>(buf);
        rec->record_type = static_cast<uint32_t>(WalRecordType::SESSION_START);
        rec->instrument_id = 0;
        rec->order_id = 0;
        rec->quantity_delta = 0;
        rec->price = 0;
        rec->payload_length = 0;
        write_sync(rec);
    }
#endif  // __linux__

    // ── scan_wal (spec §4.7: recovery protocol) ─────────────────────────────
    // Scans a WAL file and validates all records (CRC32C + sequence
    // monotonicity). Returns metadata about the valid prefix.
    //
    // Steps 1–2 of the five-step recovery protocol:
    //   1. CRC32C Verification — first CRC failure = truncation point
    //   2. Sequence Gap Detection — gaps = truncation point
    //
    // Step 3 (State Reconstruction) is the caller's responsibility.
    // Step 4 (RFP Reconciliation) is blocked until S5.
    // Step 5 (Recovery Sign-Off) is synthetic in S1.

#if defined(__linux__)
    static RecoveryResult scan_wal(const char* path) noexcept {
        int fd = ::open(path, O_RDONLY);
        if (fd < 0) {
            // File doesn't exist — fresh start, no recovery needed
            return RecoveryResult{0, 0, 0, true};
        }
        RecoveryResult result = scan_wal_fd(fd);
        ::close(fd);
        return result;
    }
#endif  // __linux__

    // ── Accessors ────────────────────────────────────────────────────────────

    [[nodiscard]] uint64_t current_sequence() const noexcept {
        return sequence_;
    }

    [[nodiscard]] uint64_t write_offset() const noexcept {
        return write_offset_;
    }

private:
    // ── Thread identity assertion (AD-005, spec §4.5) ────────────────────────
    // Registers thread identity on first call; asserts same thread on
    // subsequent calls. Matches SpscRing convention.

    void assert_wal_thread_impl() noexcept {
#if defined(__linux__) && !defined(NDEBUG)
        const pthread_t id = ::pthread_self();
        pthread_t expected = 0;
        if (wal_thread_id_.compare_exchange_strong(
                expected, id, std::memory_order_relaxed)) {
            // First call — thread identity registered
        } else {
            assert(
                ::pthread_equal(expected, id) &&
                "TERMINAL_HALT: concurrent CQE consumer detected (AD-005)");
        }
#endif
    }

    // ── Shadow mirror write (spec §4.2) ──────────────────────────────────────
    // Best-effort pwrite to shadow fd at the same offset as the primary.
    // Shadow failure is a P1 operational alert — not TERMINAL_HALT.

#if defined(__linux__)
    void write_shadow(const void* data, std::size_t len,
                      uint64_t offset) noexcept {
        if (shadow_fd_ < 0) {
            return;
        }
        // pwrite at explicit offset — mirrors primary file position
        (void)::pwrite(shadow_fd_, data, len, static_cast<off_t>(offset));
    }
#endif  // __linux__

    // ── Internal WAL scan (spec §4.7) ────────────────────────────────────────
    // Reads records from an already-open fd using pread (no lseek state
    // mutation). Validates CRC32C and sequence monotonicity.

#if defined(__linux__)
    static RecoveryResult scan_wal_fd(int fd) noexcept {
        RecoveryResult result{0, 0, 0, true};

        struct stat st;
        if (::fstat(fd, &st) != 0 || st.st_size == 0) {
            return result;  // Empty or unreadable — fresh start
        }

        const uint64_t file_size = static_cast<uint64_t>(st.st_size);
        uint64_t offset = 0;
        uint64_t prev_seq = 0;
        bool first_record = true;

        // Stack-local read buffer — no malloc (GC-E-010)
        alignas(8) uint8_t buf[kMaxWalRecordWireSize];

        while (offset + kWalHeaderWireSize <= file_size) {
            // Read header
            ssize_t n = ::pread(fd, buf, kWalHeaderWireSize,
                                static_cast<off_t>(offset));
            if (n != static_cast<ssize_t>(kWalHeaderWireSize)) {
                result.was_clean = false;
                break;  // Partial read — truncation point
            }

            const auto* rec = reinterpret_cast<const WalRecord*>(buf);

            // Validate payload_length
            if (rec->payload_length > kMaxPayloadSize) {
                result.was_clean = false;
                break;  // Invalid payload length — truncation point
            }

            // Read payload if present
            if (rec->payload_length > 0) {
                if (offset + kWalHeaderWireSize + rec->payload_length >
                    file_size) {
                    result.was_clean = false;
                    break;  // Incomplete payload — truncation point
                }
                n = ::pread(fd, buf + kWalHeaderWireSize,
                            rec->payload_length,
                            static_cast<off_t>(offset + kWalHeaderWireSize));
                if (n != static_cast<ssize_t>(rec->payload_length)) {
                    result.was_clean = false;
                    break;  // Partial payload read — truncation point
                }
            }

            // Step 1: Header CRC32C Verification (spec §4.7 step 1)
            // "First CRC failure = WAL truncation point."
            uint32_t computed_crc =
                detail::compute_crc32c(buf, kWalCrcCoverageBytes);
            if (computed_crc != rec->header_crc32c) {
                result.was_clean = false;
                break;  // WAL-CRC: truncation at corrupted record
            }

            // Step 1b: Payload CRC32C Verification (spec §4.3 payload[last-4..last])
            // If payload_length >= 4: recompute CRC32C of first
            // (payload_length - 4) bytes and compare to last 4 bytes.
            // Mismatch = truncation point (payload corruption).
            // If payload_length < 4: no payload CRC — header CRC is sole
            // integrity check for this record (intentional, spec §4.3).
            if (rec->payload_length >= 4) {
                const uint32_t crc_cover_len = rec->payload_length - 4;
                const uint32_t computed_payload_crc = detail::compute_crc32c(
                    buf + kWalHeaderWireSize, crc_cover_len);
                uint32_t stored_payload_crc;
                std::memcpy(&stored_payload_crc,
                            buf + kWalHeaderWireSize + crc_cover_len,
                            sizeof(stored_payload_crc));
                if (computed_payload_crc != stored_payload_crc) {
                    result.was_clean = false;
                    break;  // WAL-PAYLOAD-CRC: truncation at payload-corrupted record
                }
            }

            // Step 2: Sequence Gap Detection (spec §4.7 step 2)
            // "Any gap in sequence numbers indicates a partial write."
            if (!first_record && rec->sequence != prev_seq + 1) {
                result.was_clean = false;
                break;  // Sequence gap — truncation point
            }

            // Valid record — advance
            prev_seq = rec->sequence;
            first_record = false;
            result.valid_records++;
            result.last_valid_sequence = rec->sequence;
            offset += kWalHeaderWireSize + rec->payload_length;
            result.truncation_byte_offset = offset;
        }

        // If we did not consume the entire file, trailing bytes exist
        if (offset < file_size) {
            result.was_clean = false;
        }

        return result;
    }
#endif  // __linux__

    // ── Member variables ─────────────────────────────────────────────────────
    // All storage is member-resident — no heap allocation.

    int primary_fd_{-1};
    int shadow_fd_{-1};

#if defined(__linux__)
    detail::UringState uring_{};
#endif

    // Sequence counter — WAL-SEQ invariant (spec §4.8)
    // Monotonically increasing, never reset, never reused.
    uint64_t sequence_{0};

    // Current write offset in WAL file (bytes)
    uint64_t write_offset_{0};

    // Thread identity for CQE consumer assertion (AD-005)
#if defined(__linux__) && !defined(NDEBUG)
    std::atomic<pthread_t> wal_thread_id_{0};
#endif

    // Registered io_uring write buffer — cache-line aligned for DMA
    // alignment and to prevent false sharing.
    alignas(64) uint8_t write_buffer_[kMaxWalRecordWireSize]{};

#if defined(__linux__)
    struct iovec registered_iov_ {};
#endif
};

// Cleanup macro — do not pollute downstream translation units
#undef ASSERT_WAL_THREAD

}  // namespace egbos::wal
