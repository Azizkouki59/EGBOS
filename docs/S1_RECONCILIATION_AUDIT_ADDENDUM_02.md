# S1 RECONCILIATION AUDIT — ENGINE HEADER EVIDENCE & SCALE CHECK

## Addendum 02 to EGBOS-RECON-S1-001

---

| Field              | Value                                                                  |
|--------------------|------------------------------------------------------------------------|
| **Addendum ID**    | EGBOS-RECON-S1-001-ADD-02                                              |
| **Parent Reports** | `docs/S1_RECONCILIATION_AUDIT.md`, `docs/S1_RECONCILIATION_AUDIT_ADDENDUM.md` |
| **Classification** | Verification Only — No Resolution Authority                            |
| **Date**           | 2026-07-11                                                             |
| **Trigger**        | ADD-01 Action 5 used prose summaries; this addendum supplies raw code excerpts and scale analysis |

> [!NOTE]
> **Line-count tool discrepancy**: The file viewer tool reports `wal_writer.h` as 847 lines and `S0.html` as 1800 lines. `wc -l` returns 846 and 1799 respectively. This is a known artifact: `wc -l` counts newline characters; a file whose last line has no trailing `\n` reports one fewer. The content is identical in both views. No stop condition applies.

---

### RAW EXCERPTS (grouped by file)

---

## `engine/core/wal_writer.h`

### 1a — All `#include` directives (lines 28–50)

```cpp
28: #pragma once
29:
30: #include <atomic>
31: #include <cassert>
32: #include <cerrno>
33: #include <cstddef>
34: #include <cstdint>
35: #include <cstdio>
36: #include <cstdlib>
37: #include <cstring>
38: #include <type_traits>
39:
40: #if defined(__linux__)
41: #include <fcntl.h>
42: #include <linux/io_uring.h>
43: #include <pthread.h>
44: #include <sys/mman.h>
45: #include <sys/stat.h>
46: #include <sys/syscall.h>
47: #include <sys/uio.h>
48: #include <time.h>
49: #include <unistd.h>
50: #endif
```

---

### 1b — ASSERT_WAL_THREAD macro definition (lines 440–452) and `assert_wal_thread_impl()` (lines 694–707)

**Macro (public, top of class):**

```cpp
440: // ─────────────────────────────────────────────────────────────────────────────
441: // ASSERT_WAL_THREAD macro (spec §4.5, §14.1)
442: //
443: // In DEBUG builds: runtime thread identity check on every CQE consumption.
444: // In release builds (NDEBUG): compiles to nothing ("compile-time macro").
445: // Violation at runtime triggers TERMINAL_HALT (P-006).
446: // ─────────────────────────────────────────────────────────────────────────────
447:
448: #ifdef NDEBUG
449: #define ASSERT_WAL_THREAD() ((void)0)
450: #else
451: #define ASSERT_WAL_THREAD() assert_wal_thread_impl()
452: #endif
```

**Implementation (private method, lines 694–707):**

```cpp
694:     void assert_wal_thread_impl() noexcept {
695: #if defined(__linux__) && !defined(NDEBUG)
696:         const pthread_t id = ::pthread_self();
697:         pthread_t expected = 0;
698:         if (wal_thread_id_.compare_exchange_strong(
699:                 expected, id, std::memory_order_relaxed)) {
700:             // First call — thread identity registered
701:         } else {
702:             assert(
703:                 ::pthread_equal(expected, id) &&
704:                 "TERMINAL_HALT: concurrent CQE consumer detected (AD-005)");
705:         }
706: #endif
707:     }
```

---

### 1c — TERMINAL_HALT / short-write failure handler `wal_fatal()` (lines 256–267)

```cpp
256: // TERMINAL_HALT — fatal WAL error (spec §19.1, P-006)
257: // Capital safety over continuity. No recovery attempted at runtime.
258: [[noreturn]] inline void wal_fatal(const char* op) noexcept {
259:     char buf[512];
260:     std::snprintf(buf, sizeof(buf),
261:                   "EGBOS TERMINAL_HALT: WAL writer failed during %s "
262:                   "(errno=%d). Capital safety enforced — engine halted.\n",
263:                   op, errno);
264:     // Write directly to stderr fd — no heap allocation
265:     (void)::write(STDERR_FILENO, buf, std::strlen(buf));
266:     std::abort();
267: }
```

The call site that invokes `wal_fatal` on short-write (lines 614–619 inside `write_sync`):

```cpp
614:         // ── Step 6: Verify full write (spec §14.1, §19.1) ───────────────────
615:         // "Return value must equal buffer length; short writes are fatal
616:         //  errors — not retryable conditions." (spec §4.5)
617:         if (result != static_cast<int32_t>(wire_size)) {
618:             wal_fatal("write_sync: short write or io_uring error");
619:         }
```

---

### 1d — Full `write_sync()` function (lines 576–633)

```cpp
576: #if defined(__linux__)
577:     void write_sync(WalRecord* record) noexcept {
578:         ASSERT_WAL_THREAD();
579:
580:         // ── Step 1: Set sequence (WAL-SEQ invariant) ─────────────────────────
581:         // Monotonically increasing, never reset, never reused (spec §4.8).
582:         record->sequence = sequence_++;
583:
584:         // ── Step 2: Timestamp before write (spec §4.3) ───────────────────────
585:         // CLOCK_MONOTONIC_RAW — CLOCK_REALTIME forbidden (GC-E-011).
586:         record->tsc_before_write = monotonic_raw_ns();
587:
588:         // tsc_after_write = 0 for CRC computation; it is populated after
589:         // O_DSYNC returns (the on-disk value is always 0).
590:         record->tsc_after_write = 0;
591:
592:         // Clear _pad for deterministic wire format
593:         std::memset(record->_pad, 0, sizeof(record->_pad));
594:
595:         // Validate payload_length (spec §4.3: 0–256 bytes)
596:         assert(record->payload_length <= kMaxPayloadSize &&
597:                "WAL record payload exceeds maximum 256 bytes");
598:
599:         // ── Step 3: Compute CRC32C (spec §4.3, §14.1) ───────────────────────
600:         // CRC covers header bytes [0, 60) — all fields before header_crc32c.
601:         record->header_crc32c =
602:             detail::compute_crc32c(record, kWalCrcCoverageBytes);
603:
604:         // ── Step 4: Copy to registered io_uring buffer ───────────────────────
605:         const uint32_t wire_size = static_cast<uint32_t>(
606:             kWalHeaderWireSize + record->payload_length);
607:         std::memcpy(write_buffer_, record, wire_size);
608:
609:         // ── Step 5: Submit WRITE_FIXED and wait for CQE (spec §4.5) ─────────
610:         const uint64_t current_offset = write_offset_;
611:         int32_t result = uring_write_fixed_sync(
612:             uring_, primary_fd_, write_buffer_, wire_size, current_offset, 0);
613:
614:         // ── Step 6: Verify full write (spec §14.1, §19.1) ───────────────────
615:         // "Return value must equal buffer length; short writes are fatal
616:         //  errors — not retryable conditions." (spec §4.5)
617:         if (result != static_cast<int32_t>(wire_size)) {
618:             wal_fatal("write_sync: short write or io_uring error");
619:         }
620:
621:         // Advance write offset
622:         write_offset_ += wire_size;
623:
624:         // ── Step 7: Timestamp after write (spec §4.3) ────────────────────────
625:         // Populated in-memory for latency auditing (WAL-ORDER invariant).
626:         record->tsc_after_write = monotonic_raw_ns();
627:
628:         // ── Step 8: Shadow mirror (spec §4.2) ────────────────────────────────
629:         // Async write to shadow — best-effort. Shadow failure is a P1 alert,
630:         // not TERMINAL_HALT.
631:         write_shadow(write_buffer_, wire_size, current_offset);
632:     }
633: #endif  // __linux__
```

---

## `engine/core/spsc_ring.h`

### 2a — Power-of-two capacity `static_assert` (lines 34–36)

```cpp
34:     // ── Compile-time Constraints (Spec §5.2, §14.2) ──────────────────────────
35:     static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
36:         "SpscRing capacity must be a power of 2 (AD-006)");
```

### 2b — Full `push()` function (lines 55–91)

```cpp
55:     // ── Producer Interface (Spec §5.2, §5.3, §13.2, §14.2) ───────────────────
56:     // push() — called from producer thread only; spins on full
57:     void push(const T& item) noexcept {
58:         assert_producer_thread();
59:
60:         // Producer reads its own index (relaxed)
61:         const uint64_t head = head_.load(std::memory_order_relaxed);
62:
63:         // Check if full (using cached tail to avoid atomic read if possible)
64:         if (head - tail_cache_ == Capacity) {
65:             std::size_t spins = 0;
66:             while (true) {
67:                 // Producer reads consumer index with acquire
68:                 tail_cache_ = tail_.load(std::memory_order_acquire);
69:                 if (head - tail_cache_ < Capacity) {
70:                     break; // Space available
71:                 }
72:
73:                 // Spin yield backpressure (§5.3)
74:                 if (++spins >= kSpscSpinYieldThreshold) {
75: #if defined(__linux__)
76:                     ::sched_yield();
77: #endif
78:                 } else {
79: #if defined(__x86_64__) || defined(_M_X64)
80:                     _mm_pause();
81: #endif
82:                 }
83:             }
84:         }
85:
86:         // Write to slot
87:         slots_[head & kMask] = item;
88:
89:         // Producer index: seq_cst on write per spec §5.2
90:         head_.store(head + 1, std::memory_order_seq_cst);
91:     }
```

### 2c — Full `pop()` function (lines 93–117)

```cpp
93:     // ── Consumer Interface (Spec §5.2, §13.2, §14.2) ─────────────────────────
94:     // pop() — called from consumer thread only; returns false if empty
95:     [[nodiscard]] bool pop(T& item) noexcept {
96:         assert_consumer_thread();
97:
98:         // Consumer reads its own index: relaxed on read per spec §5.2
99:         const uint64_t tail = tail_.load(std::memory_order_relaxed);
100:
101:         // Consumer reads producer index: acquire to see slot writes
102:         if (head_cache_ == tail) {
103:             head_cache_ = head_.load(std::memory_order_acquire);
104:             if (head_cache_ == tail) {
105:                 return false; // Empty
106:             }
107:         }
108:
109:         // Read slot
110:         item = slots_[tail & kMask];
111:
112:         // Update consumer index: release to ensure slot read completes before
113:         // producer can overwrite it.
114:         tail_.store(tail + 1, std::memory_order_release);
115:
116:         return true;
117:     }
```

---

## `engine/core/pool_allocator.h`

### 3a — All `#include` directives (lines 26–31)

```cpp
26: #pragma once
27:
28: #include <cassert>
29: #include <cstddef>
30: #include <cstdint>   // std::uintptr_t (uintptr_t bounds check in slot_index)
31: #include <type_traits>
```

### 3b — Full `acquire()` function (lines 118–152)

```cpp
118:     // ── acquire() ─────────────────────────────────────────────────────────────
119:     //
120:     // Returns a pointer to one uninitialized slot.
121:     //
122:     // The caller MUST:
123:     //   1. Check the return value — nullptr means pool exhausted (fatal).
124:     //   2. Initialise the object before use.
125:     //   3. Call release() when finished.
126:     //
127:     // The caller MUST NOT:
128:     //   - Call delete on the returned pointer.
129:     //   - Call free() on the returned pointer.
130:     //   - Use the pointer after calling release().
131:     //
132:     // Time complexity: O(N) worst-case; O(1) amortised for sequential patterns.
133:     // Does not call malloc, new, or any allocator (GC-E-010, CLAIM-L3-POOL-001).
134:     //
135:     // Returns: pointer to an unused slot, or nullptr if all N slots are live.
136:     [[nodiscard]] T* acquire() noexcept {
137:         // Linear scan starting from next_, with wraparound.
138:         // Using % N (not bitmask) because N is not required to be power of 2.
139:         for (std::size_t i = 0; i < N; ++i) {
140:             const std::size_t idx = (next_ + i) % N;
141:             if (!in_use_[idx]) {
142:                 in_use_[idx] = true;
143:                 // Advance hint so the next acquire() skips this (now-used) slot.
144:                 next_ = (idx + 1) % N;
145:                 return slot_ptr(idx);
146:             }
147:         }
148:         // All N slots are live. Caller must treat this as a fatal startup error.
149:         // Spec §7.2: "Pool exhaustion is a fatal startup error — not a runtime
150:         // condition."
151:         return nullptr;
152:     }
```

### 3c — Full `release()` function (lines 154–206)

```cpp
154:     // ── release() ─────────────────────────────────────────────────────────────
155:     //
156:     // Returns a slot to the pool, making it available for the next acquire().
157:     //
158:     // Preconditions (enforced by assertion):
159:     //   - ptr must not be nullptr.
160:     //   - ptr must have been returned by a prior acquire() on THIS pool instance.
161:     //   - ptr must not have been released already (no double-release).
162:     //
163:     // The destructor of T is NOT called. T must be trivially destructible.
164:     // The slot's bytes are NOT zeroed — the next acquirer must initialise.
165:     //
166:     // Does not call free, delete, or any deallocator (GC-E-010).
167:     void release(T* ptr) noexcept {
168:         // Null-pointer guard — always a programming error.
169:         assert(ptr != nullptr &&
170:             "FixedPool::release: null pointer — always a programming error");
171:
172:         // Compute slot index. Returns N (sentinel) if ptr is out of range.
173:         const std::size_t idx = slot_index(ptr);
174:
175:         // Bounds check — ptr must originate from this pool's storage_.
176:         // Catches use of a foreign pointer or pointer into a different pool.
177:         assert(idx < N &&
178:             "FixedPool::release: pointer not within pool storage "
179:             "(foreign pointer, use-after-free, or wrong pool instance)");
180:
181:         // Slot-alignment check — ptr must be exactly slot-aligned.
182:         // A mid-slot pointer indicates internal corruption.
183:         // Slot-alignment check via uintptr_t arithmetic — avoids UB from
184:         // subtracting unrelated pointers ([expr.add]/5 C++17). The bounds
185:         // check above guarantees ptr is numerically within [base, end), so
186:         // the difference is non-negative and fits in std::size_t.
187:         assert((reinterpret_cast<std::uintptr_t>(ptr)
188:                     - reinterpret_cast<std::uintptr_t>(&storage_[0]))
189:                    % sizeof(T) == 0 &&
190:             "FixedPool::release: pointer is not slot-aligned "
191:             "(internal corruption)");
192:
193:         // Double-release detection — slot must currently be in-use.
194:         // Under ASAN this fires as an assertion abort, making it detectable
195:         // without an explicit shadow map (pool_no_double_free test, spec §15.3).
196:         assert(in_use_[idx] &&
197:             "FixedPool::release: double-release detected — "
198:             "slot is already free");
199:
200:         in_use_[idx] = false;
201:
202:         // Update hint to point at the newly freed slot. The next acquire()
203:         // call will find it immediately, giving O(1) behaviour for
204:         // depth-1 acquire/release patterns.
205:         next_ = idx;
206:     }
```

---

## `engine/core/cross_plane.h`

### 4a — Full `CROSS_PLANE_READ` macro definition (lines 129–148)

```cpp
129: #define CROSS_PLANE_READ(atomic_var, layer_from, layer_to)                    \
130:     ([&]() -> int64_t {                                                        \
131:         /* Enforce int64_t-only rule (spec §6.3, HTML §C3.3 verbatim).       \
132:            GC-E-045 additionally forbids direct field reads of x_l_ /        \
133:            order_tag_ from L4/L5; this static_assert is the compile-time     \
134:            half of that enforcement. */                                        \
135:         static_assert(                                                         \
136:             sizeof(atomic_var) == sizeof(int64_t),                            \
137:             "CROSS_PLANE_READ: only std::atomic<int64_t> (8-byte) atomics "  \
138:             "are permitted across layer boundaries (spec §6.3). "             \
139:             "Wider or narrower types require a new claim at V1 minimum.");    \
140:         /* Evaluate layer constants — fold to nothing at compile time.        \
141:            Their presence in the source is what static analysis tools read.  \
142:            (void) suppresses -Wunused warnings at -O0. */                     \
143:         (void)(layer_from);                                                    \
144:         (void)(layer_to);                                                      \
145:         /* memory_order_acquire: see rationale in header comment above.       \
146:            GC-E-044 blocks relaxed substitution at Phase 4. */                \
147:         return (atomic_var).load(std::memory_order_acquire);                  \
148:     }())
```

---

### SCALE ASSESSMENT (per file + aggregate)

---

#### `wal_writer.h` — 846 lines

**S1 deliverables served:** WAL writer (O_DSYNC, io_uring, CRC32C, recovery scan), WAL shadow mirror, WAL record schema, WAL pool allocator integration point.

**Material beyond S1 simple forward-declared constants:**

The `WalRecordType` enum (lines 90–103) contains entries spanning S1 through S7:

```cpp
90: enum class WalRecordType : uint32_t {
91:     ORDER_NEW                     = 1,   // S3
92:     FILL_APPLIED                  = 2,   // S3
93:     POSITION_SYNC                 = 3,   // S3
94:     MASS_CANCEL                   = 4,   // S3
95:     REGIME_CHANGE                 = 5,   // S3+
96:     SESSION_START                 = 6,   // S1 — clean-startup marker
97:     HITL_HALT_CONFIRM             = 7,   // S4+
98:     HITL_RESTART_AUTH              = 8,   // S4+
99:     HITL_RISK_LIMIT_CHANGE        = 9,   // S4+
100:    HITL_SIGNAL_WEIGHT_CHANGE     = 10,  // S4+
101:    HITL_REGISTRY_EMERGENCY_AMEND = 11,  // S4+
102:    CAPITAL_GATE_CLOCK_START      = 12,  // S7
103: };
```

Per Master Blueprint §I4 (Claim-to-Slice mapping): ORDER_NEW, FILL_APPLIED, POSITION_SYNC, MASS_CANCEL belong to S3 (CLAIM-L3-001 scope). HITL_* variants belong to S4+ (CLAIM-HITL-* scope, not yet registered). CAPITAL_GATE_CLOCK_START belongs to S7 (capital gate timing).

These are **enum value forward-declarations only** — no parsing logic, no handler dispatch, no struct definitions, and no implementations for these record types exist in this file. The only S1-active record type used by `start_session()` is `SESSION_START = 6`. The other 11 values are dead constants from the perspective of S1 runtime execution.

**Assessment:** The enum forward-declaration is the entire extent of S2+ material in this file. The remaining 830+ lines implement S1-scoped WAL mechanics. Whether registering 11 future enum values in S1's wire-format header is appropriate governance practice is a **founder decision** — the code comment at line 87–88 states "Additional types in later slices require YAML enum registration — they may not be added ad hoc." This suggests intentional forward-reservation of the wire format.

Beyond the enum: the `io_uring` infrastructure (lines 215–436), CRC32C table (lines 178–208), and `scan_wal_fd()` recovery scanner (lines 728–809) are all S1-scoped (CLAIM-WAL-001 requires kill-9 survival, which requires WAL recovery).

**Bulk driver:** 847 lines for a WAL writer is not surprising for a raw-syscall `io_uring` implementation. `liburing` alone is ~2,000 lines; the file essentially re-implements the subset it needs (SQ/CQ ring mapping, WRITE_FIXED submission, CQE harvesting) without the library dependency. The CRC32C software table (lines 178–208) and the full recovery scanner are the two largest non-trivial additions.

---

#### `spsc_ring.h` — 165 lines

**S1 deliverables served:** SPSC ring buffer (CLAIM-SPSC-001).

**Material beyond S1:**

None identified. The file is a single template class with `push()`, `pop()`, and thread-identity assertions. No future-slice record types, no layer references, no S2+ struct fields or dispatch logic. The `kSpscSpinYieldThreshold` constant (line 30) references `timing_constants.json` in a comment but is a compile-time `inline constexpr` with no runtime dependency on that file.

**Assessment:** 165 lines is a minimal, well-bounded implementation for a cache-line-isolated SPSC ring with debug-mode thread identity enforcement. No scope concerns.

---

#### `pool_allocator.h` — 288 lines

**S1 deliverables served:** Fixed-size pool allocator (CLAIM-L3-POOL-001).

**Material beyond S1:**

None identified. The file contains a single template class (`FixedPool<T, N>`). The comment at line 52 mentions "WAL writer fills all WalRecord fields before submitting to io_uring" and at line 280: "Production instantiations (e.g. `FixedPool<WalRecord, 16>`) are in wal_writer.h." These are comments pointing forward to the WAL writer; no S2+ logic is implemented here. The `_SelfCheckPool` type alias (lines 282–286) is a compile-time self-check (`FixedPool<uint64_t, 1>`) that emits no code.

**Assessment:** 288 lines for a pool allocator with documented double-release detection, bounds checking, and alignment assertions is consistent with a correct, well-defended S1 implementation. No scope concerns.

---

#### `cross_plane.h` — 193 lines

**S1 deliverables served:** Pre-registration of GC-E-044 and GC-E-045 violation fixtures (required by S1 Spec §8). The macro itself serves S3 (CLAIM-L3-004, CLAIM-L3-006), but its pre-registration in S1 is explicitly documented.

**Material beyond S1 beyond simple forward-declaration:**

The file defines layer constants for L0–L7 (lines 71–78) and the `CROSS_PLANE_READ` macro. L3/L4 are S3 deliverables. However:

- No L3 or L4 structs, classes, or runtime logic exist in this file.
- The macro is a compile-time expression (`sizeof` static_assert + `.load(acquire)`); it emits no S3-specific code.
- The constants `LAYER_L0` through `LAYER_L7` are `inline constexpr int` values — no behaviour.
- The `_self_check_cross_plane_read()` function (lines 183–191) is a compile-time-only inline, never called from production code.

The file comment at lines 39–41 explicitly justifies its S1 presence: *"This header is defined in S1 because GC-E-044 and GC-E-045 require pre-registered violation fixtures in S1 (spec §8 pre-registration table). The fixture tests exercise this header."*

Per Master Blueprint §I4 / S1 Spec §8, pre-registration of future-slice govcheck rules in S1 is a required governance activity, not scope creep.

**Assessment:** 193 lines, of which the bulk is documentation (comments). The functional code (macro + constants) is ~30 lines. No scope concern beyond the explicitly-justified S3 pre-registration.

---

#### Aggregate Scale Assessment

| File | Lines (`wc -l`) | S2+ material | Nature of S2+ material |
|------|----------------|--------------|------------------------|
| `wal_writer.h` | 846 | **YES** — 12 enum values | Wire-format forward-declarations (enum only, no logic) |
| `spsc_ring.h` | 165 | None | — |
| `pool_allocator.h` | 288 | None | — |
| `cross_plane.h` | 193 | Pre-registers S3 rules | Justified by S1 Spec §8 pre-registration requirement |
| **Total** | **1,492** | | |

**Plausible minimal S1 estimate:**

A minimal S1 implementation of the same four deliverables with no forward-declarations would require approximately:

| Component | Minimum estimate | Rationale |
|-----------|-----------------|-----------|
| WAL writer (raw io_uring, no liburing) | ~600 lines | io_uring setup/teardown (~150), write_sync (~60), CRC32C (~40), recovery scanner (~120), WalRecord struct (~40), constants/assertions (~80), plumbing (~110) |
| SPSC ring | ~100 lines | Template class; the current 165 lines already includes only necessary code |
| Pool allocator | ~150 lines | The current 288 lines includes substantial inline documentation; logic alone is ~120 lines |
| Cross-plane header | ~40 lines | Macro + 8 constants + static_assert |
| **Estimated minimum** | **~890 lines** | |

**Gap analysis:** 1,492 actual vs ~890 estimated minimum = ~602 lines difference. This gap is attributable to:

1. **Inline documentation** — `wal_writer.h` and `pool_allocator.h` have extensive step-by-step comments explaining each operation. These are not logic; they are auditable rationale.
2. **`io_uring` infrastructure** — The raw syscall io_uring wrapper (UringState struct + `uring_init`, `uring_register_buffers`, `uring_write_fixed_sync`, `uring_destroy`) is ~155 lines. This is unavoidable for a liburing-free implementation.
3. **WAL recovery scanner** (`scan_wal_fd`, `scan_wal` — ~80 lines) — Required by CLAIM-WAL-001 V4 kill-9 survival property.
4. **`WalRecordType` enum** with 11 future-slice entries — ~14 lines of forward-declarations.

The 14 enum lines are the only material that could be characterised as exceeding pure S1 scope. Whether this constitutes a governance issue is a **founder decision**.

---

### LINE COUNT SANITY CHECK (wc -l outputs)

Raw output of `wsl wc -l` (run this session):

```
  1799 /mnt/c/EGBOS/documentation/S0.html
 10889 /mnt/c/EGBOS/documentation/EGBOS_MASTER_augmented.html
 12688 total
```

```
  846 /mnt/c/EGBOS/egbos/engine/core/wal_writer.h
  165 /mnt/c/EGBOS/egbos/engine/core/spsc_ring.h
  288 /mnt/c/EGBOS/egbos/engine/core/pool_allocator.h
  193 /mnt/c/EGBOS/egbos/engine/core/cross_plane.h
 1492 total
```

**Reconciliation against ADD-01 claims:**

| File | ADD-01 claimed | `wc -l` actual | Delta | Explanation |
|------|---------------|---------------|-------|-------------|
| `S0.html` | "1800 lines (full file)" | 1799 | −1 | Viewer tool counts last line even without trailing `\n`; `wc -l` counts newlines only |
| `EGBOS_MASTER_augmented.html` | "10890 total lines" (stated in viewer output) | 10889 | −1 | Same trailing-newline artifact |
| `wal_writer.h` | "847 lines" | 846 | −1 | Same |
| `spsc_ring.h` | "166 lines" | 165 | −1 | Same |
| `pool_allocator.h` | "289 lines" | 288 | −1 | Same |
| `cross_plane.h` | "194 lines" | 193 | −1 | Same |

All six files show exactly −1 from the viewer-tool counts. This is a systematic artifact of the viewer tool's line-numbering convention (it numbers the last partial line if the file ends without `\n`), not a content discrepancy. No stop condition applies.

---

### FILE CREATED

`docs/S1_RECONCILIATION_AUDIT_ADDENDUM_02.md` — this document.

---

### GIT STATUS AT END

```
 M .github/workflows/build.yml
 M .github/workflows/sanitizers.yml
 M CMakeLists.txt
 M engine/CMakeLists.txt
 M engine/core/shm_layout.h
 M govcheck/CMakeLists.txt
 M govcheck/src/commands/cmd_validate.cpp
 M govcheck/src/main.cpp
 M tests/governance/fixtures/verify_all_fixtures.sh
?? .github/workflows/p3_unit_integration.yml
?? .github/workflows/p4_tsan.yml
?? .github/workflows/p5_latency_regression.yml
?? build2/
?? docs/S1_COMPLIANCE_REPORT.md
?? docs/S1_INSTITUTIONAL_SPEC.md
?? docs/S1_RECONCILIATION_AUDIT.md
?? docs/S1_RECONCILIATION_AUDIT_ADDENDUM.md
?? docs/S1_RECONCILIATION_AUDIT_ADDENDUM_02.md    ← THIS SESSION (new)
?? engine/core/cross_plane.h
?? engine/core/pool_allocator.h
?? engine/core/spsc_ring.h
?? engine/core/wal_writer.h
[... remainder unchanged ...]
```

`docs/S1_RECONCILIATION_AUDIT_ADDENDUM_02.md` is the **only new file** created this session. No existing files were modified.

---

*Addendum ID: EGBOS-RECON-S1-001-ADD-02 | Date: 2026-07-11*
