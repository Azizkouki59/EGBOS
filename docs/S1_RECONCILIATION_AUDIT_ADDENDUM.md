# S1 RECONCILIATION AUDIT — CITATION SOURCE ADDENDUM

## Addendum to EGBOS-RECON-S1-001

---

| Field                  | Value                                                                    |
|------------------------|--------------------------------------------------------------------------|
| **Addendum ID**        | EGBOS-RECON-S1-001-ADD-01                                                |
| **Parent Report**      | `docs/S1_RECONCILIATION_AUDIT.md` (EGBOS-RECON-S1-001)                  |
| **Classification**     | Verification Only — No Resolution Authority                              |
| **Addendum Date**      | 2026-07-11                                                               |
| **Trigger**            | Prior audit sourced Blueprint citations from `.txt` proxy, not HTML auth |

---

### FILES OPENED THIS SESSION (with line counts)

| File | Lines Read | Notes |
|------|-----------|-------|
| `documentation/EGBOS_MASTER_augmented.html` | Lines 6315–6410, 7130–7250, 4020–4140, 3155–3280 (total ~430 lines read directly) | HTML authority source — direct line viewing |
| `documentation/S0.html` | Lines 1–1800 (full file, 1800 total) | End-to-end read completed this session |
| `docs/S1_INSTITUTIONAL_SPEC.md` | Lines 800–982 (182 lines — the previously unread remainder) | Full file now complete |
| `engine/core/wal_writer.h` | Lines 1–847 (full file) | First direct read this session |
| `engine/core/spsc_ring.h` | Lines 1–166 (full file) | First direct read this session |
| `engine/core/pool_allocator.h` | Lines 1–289 (full file) | First direct read this session |
| `engine/core/cross_plane.h` | Lines 1–194 (full file) | First direct read this session |

---

### CITATION RECONCILIATION (5 items: CONFIRMED / CORRECTED / NEW FINDING)

---

#### Action 2a — Item 2 Citation: Master Blueprint §I2, S1 Exit Command

**Original report claimed:**
> Master Blueprint §I2 (line ~4450) requires for S1 exit: `govcheck validate --scope C0,P3,P4,P5 --strict`

**HTML snippet read** (`EGBOS_MASTER_augmented.html`, lines 6344–6357):

```html
<!-- S1 -->
<div class="card" id="slice-S1">
  <div class="slice-card-header">
    <span class="slice-id">S1 — WAL + SPSC + SHM Skeleton</span>
    <span class="slice-capital-gate none">Capital Gate: None</span>
  </div>
  <p><strong>Entry Condition:</strong> S0 acceptance gate passed. govcheck C0 clean. CLAIM-000 at V1.</p>
  <p><strong>Exit Condition (Acceptance Gate):</strong></p>
  <pre>govcheck validate --scope C0,P3,P4,P5 --strict</pre>
  <p><strong>Pass Criteria:</strong> CLAIM-WAL-001 at V4 ... CLAIM-SPSC-001 at V4 ... CLAIM-L3-POOL-001 at V2 ...</p>
```

**Verdict:** **CONFIRMED** — the exit command `govcheck validate --scope C0,P3,P4,P5 --strict` is verbatim in the HTML at `id="slice-S1"` (line 6352). The original report's claimed line number (~4450) was wrong (actual HTML line is 6352) but this was a proxy-line artifact — the text and command quoted in the report are correct.

**Correction to line number:** Original report cited "line ~4450". Actual HTML line: **6352**. The report's quoted command text is accurate; only the approximate line number was wrong.

---

#### Action 2b — Item 3 Citation: §I7.2 GC-F-004 "no other gate is valid without this"

**Original report claimed:**
> Per Master Blueprint §I7.2, GC-F-004 is "immediate blocking" from S0 start. "no other gate is valid without this."

**HTML snippet read** (`EGBOS_MASTER_augmented.html`, lines 7200–7207):

```html
<tr>
  <td style="font-family:var(--font-mono);font-size:0.75rem;color:var(--accent)">GC-F-004</td>
  <td>CLAIM-GOVCHECK-001 not at V4 on current hardware_id</td>
  <td>S0 start</td>
  <td>S0 (immediate blocking)</td>
  <td>C0</td>
  <td>Root trust chain — no other gate is valid without this</td>
</tr>
```

**Verdict:** **CONFIRMED** — the phrase "no other gate is valid without this" is verbatim in the HTML at line 7206, in the Notes column for GC-F-004. Section anchor is `id="i7-2-rule-schedule"` at line 7184. The original report's citation is accurate.

---

#### Action 2c — Item 5 Citation: §C3.1 8-Region SHM Table

**Original report claimed:**
> Master Blueprint §C3.1 defines 8 canonical regions with specific paths, directions, and sizes.

**HTML snippet read** (`EGBOS_MASTER_augmented.html`, lines 3160–3176):

```html
<h3 class="subsection-title" id="c3-1-shm-topology">C3.1 — SHM Region Topology</h3>
<p>All inter-layer communication in the execution plane (L0–L5) passes through
named POSIX shared memory regions...</p>

<div class="arch-overview">
<pre>SHM REGION MAP — ENGINE_V3 EXECUTION PLANE
════════════════════════════════════════════════════════════════════════
/dev/shm/egbos_market_events     [L0 → L1]   SPSC ring, 65536 slots × 128B
/dev/shm/egbos_lob_snapshots     [L1 → L2]   Double-buffered LOB, per instrument
/dev/shm/egbos_feature_vector    [L2 → L3]   256B FeatureVector, seqlock protected
/dev/shm/egbos_position_state    [L3 → L4]   Read-only L4 view (gross_exposure_ only)
/dev/shm/egbos_risk_limits       [L4 → L3]   Monotone latches: kill_switch, pos_limit
/dev/shm/egbos_exec_commands     [L3 → L5]   SPSC ring, 4096 slots × 64B
/dev/shm/egbos_regime_signal     [L6/L7 → L3] Redis Streams bridge output, 1KB
/dev/shm/egbos_governance_flags  [All layers]  Read-only: feed_stale, halt_required
════════════════════════════════════════════════════════════════════════
All regions: mlock(2) mandatory — no page faults on hot path
All regions: NUMA-local to execution CPU socket</pre>
```

**Verdict:** **CONFIRMED** — all 8 regions, paths, layer directions, slot counts, and sizes quoted in the original report's Item 5 table match the HTML verbatim at lines 3160–3176 (`id="c3-1-shm-topology"`). The `.txt` proxy used by the original audit did not introduce any distortion for this section.

---

#### Action 2d — Item 6 Citation: §I7.2 GC-E-010/GC-E-011 Activation Schedule

**Original report claimed:**
> §I7.2: GC-E-010 Warning from S0 start, Blocking at S3 (gate P2→P3). GC-E-011 Warning from S0 start, Blocking at S2 (gate P2).

**HTML snippet read** (`EGBOS_MASTER_augmented.html`, lines 7208–7223):

```html
<tr>
  <td ...>GC-E-010</td>
  <td>malloc/new/free/delete in engine/ hot path</td>
  <td>S0 start</td>
  <td>S3</td>
  <td>P2→P3</td>
  <td>ADR-006 (compile-time SHM)</td>
</tr>
<tr>
  <td ...>GC-E-011</td>
  <td>clock_gettime(CLOCK_REALTIME) in engine/</td>
  <td>S0 start</td>
  <td>S2</td>
  <td>P2</td>
  <td>Determinism (P-003)</td>
</tr>
```

**Verdict:** **CONFIRMED** — GC-E-010 Warning from S0, Blocking at S3 (P2→P3); GC-E-011 Warning from S0, Blocking at S2 (P2). Both match the original report's Item 6 table exactly.

**Incidental note:** The HTML `id` for this subsection is `i7-2-rule-schedule` (line 7184), not `I7.2` — the section is correctly referenced in the report as §I7.2.

---

#### Action 3 — S0.html End-to-End Read: Item 1 and Item 2 Citation Verification

S0.html was read in full (1,800 lines) this session.

**Item 1 citation — S0.html §4.7 FetchContent CMakeLists.txt:**

S0.html lines 892–944 contain the verbatim canonical CMakeLists.txt. Key lines confirmed:
- Line 899: `# ── Dependencies via FetchContent ──`
- Line 900: `include(FetchContent)`
- Lines 901–909: `FetchContent_Declare` for `yaml-cpp` (GIT_TAG 0.8.0), `json` (GIT_TAG v3.11.3), `googletest` (GIT_TAG v1.14.0)
- Line 910: `FetchContent_MakeAvailable(yaml-cpp json googletest)`
- Line 913: `file(GLOB_RECURSE GOVCHECK_SOURCES src/*.cpp)`
- Line 940: `GTest::gtest_main` (not `GTest::GTest` + `GTest::Main`)

**Verdict:** **CONFIRMED** — all Item 1 citations in the original report match the actual S0.html text. The repository deviations (find_package, explicit source enumeration, different GTest link target) are confirmed against the canonical S0 spec.

**Additional finding from full S0.html read:**

> [!IMPORTANT]
> **NEW FINDING (S0-NF-01):** S0.html Section 4.1 (line 637) states: *"Dependencies (all fetched via CMake FetchContent)"* and lists OpenSSL as: `OpenSSL (libcrypto) — SHA-256 for artifact verification — Version: System`. OpenSSL is explicitly annotated as **System** in the dependency table (line 644), unlike yaml-cpp, nlohmann/json, and GoogleTest which are all annotated for FetchContent. The repository's `find_package(OpenSSL REQUIRED)` therefore **does not** constitute a deviation from S0 spec for OpenSSL specifically. The original report's Item 1 implicitly treated all four `find_package` calls as equal deviations. This is a partial correction: the OpenSSL `find_package` is spec-compliant; the yaml-cpp, nlohmann/json, and googletest `find_package` calls remain confirmed deviations.

**Item 2 citation — S0.html §4.6 CLI syntax:**

S0.html lines 838–869 define the `govcheck validate` CLI. Line 843 confirms:
- Scope Values: `C0` (governance pre-check). Future: `P3`, `P4`, `G1`, `G2`, etc.

**Verdict:** **CONFIRMED** — the original report's statement that S0 defines only single-token scope values and lists P3/P4/etc. as "Future" is accurate per the actual HTML text.

**Item 2 S0 fixture script finding:**

S0.html Section 7.3 (lines 1429–1470) provides the canonical `verify_all_fixtures.sh` script. The **S0-spec version** of this script:
- Uses **only** `govcheck validate` (not `govcheck scan`)
- Tests **6 fixtures** only: `invalid_schema.yaml`, `bad_artifact_sha.yaml`, `bad_owner.yaml`, `undeclared_ns_dep.yaml`, `superseded_dep.yaml`, `laundered_class.yaml`
- Contains **no Category 2 source-code violation fixtures** (those are S1 additions)

This further confirms the original report's finding that the `govcheck scan` references in the current `verify_all_fixtures.sh` are S1 additions that have no corresponding implementation.

---

#### Action 4 — S1_INSTITUTIONAL_SPEC.md Lines 800–982 (Previously Unread)

The remaining 182 lines cover: §17 Acceptance Criteria (WAL kill-9, SPSC TSAN, pool ASAN, P4/P5 gates), §18 Entry Conditions, §19 Failure Mode Specifications, §20 Exit Gates checklist, and Appendices A–C.

**Does anything contradict or supersede the original report's conclusions?**

| Section | Finding |
|---------|---------|
| §17 Acceptance Criteria | Consistent with Master Blueprint §I2 S1 exit criteria. No new conflicts. |
| §18 Entry Conditions (lines 823–836) | Confirms S1 requires S0 govcheck-clean + CLAIM-000 at V1. No conflict. |
| §19.1 WAL Failure Modes (lines 843–853) | Documents TERMINAL_HALT for WAL I/O errors, concurrent CQE consumer, short writes. No conflict with audit conclusions. |
| §19.3 SHM Failure Modes (lines 864–872) | "Runtime SHM region creation attempted → Static assertion at compile time." Confirms Item 5 (shm_layout.h) finding. |
| §20 Exit Gates (lines 875–914) | §20.1 line 881 confirms exit gate command `govcheck validate --scope C0,P3,P4,P5 --strict`. Consistent with Item 2 ambiguity finding. |
| §20.4 Forbidden Action Verification (lines 906–910) | Confirms GC-E-010 Warning, GC-E-011 pre-registered, AD-006 static assertion. Consistent with Items 5 and 6. |
| Appendix C — Governance Conflict Resolution (lines 969–977) | States explicitly: "This document is derived exclusively from `EGBOS_MASTER_augmented.html`... In the event of any conflict between this document and the HTML → **the HTML prevails unconditionally**." |

**Verdict:** **NO CONTRADICTIONS** — lines 800–982 of `S1_INSTITUTIONAL_SPEC.md` do not contradict or supersede any conclusion in the original report. The exit gate command, failure modes, and governance conflict rule are all consistent.

---

#### Action 5 — Engine Header Review

---

##### `engine/core/wal_writer.h` — 847 lines

**Scope conformance assessment:**

| Check | Finding |
|-------|---------|
| GC-E-010 (no malloc/new/free/delete) | **PASS** — no heap allocation on write path. Stack-local buffers only. All storage is member-resident (`write_buffer_[]`, `uring_{}`, `primary_fd_`, etc.). |
| GC-E-011 (no CLOCK_REALTIME) | **PASS** — line 275 uses `CLOCK_MONOTONIC_RAW` exclusively. CLOCK_REALTIME explicitly commented as forbidden at line 26 and 585. |
| AD-005 (io_uring single-CQE-consumer) | **PASS** — `ASSERT_WAL_THREAD()` on every `write_sync()` call (line 578). Thread identity registered on first call, assertion on all subsequent calls. |
| AD-012 (WAL-before-state ordering) | **PASS** — `write_sync()` blocks until O_DSYNC CQE returns (step 5–6) before any state mutation. Short write = TERMINAL_HALT (line 617–619). |
| GC-WAL-001 (fsync before state) | **PASS** — enforced by the single `uring_write_fixed_sync()` call with `min_complete=1` (line 397). Function only returns after CQE is received. |
| No out-of-scope features | The header includes S3+ `WalRecordType` enum entries (ORDER_NEW, FILL_APPLIED, etc., lines 91–102). These are **forward declarations for future slices** — the comment `// S3` annotates each. This is not a defect; it is standard practice to register enum values in the wire format before their slice. No S3 logic is implemented. |

**Verdict: NO SCOPE DEVIATION** — `wal_writer.h` conforms to CLAIM-WAL-001 requirements and all referenced spec sections.

---

##### `engine/core/spsc_ring.h` — 166 lines

**Scope conformance assessment:**

| Check | Finding |
|-------|---------|
| GC-E-010 (no malloc/new/free/delete) | **PASS** — slots array is member-resident (`alignas(kCacheLineSize) T slots_[Capacity]{}`). No heap allocation anywhere. |
| AD-006 (power-of-two capacity) | **PASS** — `static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0, "SpscRing capacity must be a power of 2 (AD-006)")` at line 35–36. |
| Cache-line isolation (§5.2, §13.2) | **PASS** — `head_` and `tail_` are on separate `alignas(64)` cache lines (lines 148, 155). |
| Producer memory ordering (spec §5.2) | **PASS** — `head_.store(head + 1, std::memory_order_seq_cst)` at line 90. Matches spec §5.2 "seq_cst on write". |
| Consumer memory ordering (spec §5.2) | **PASS** — `tail_.store(tail + 1, std::memory_order_release)` at line 114. Matches spec §5.2 "relaxed on read" / "release on advance". |
| SPSC-SINGLE assertion | **PASS** — `assert_producer_thread()` and `assert_consumer_thread()` enforce single-producer/single-consumer in debug builds (lines 121–143). |
| GC-E-011 (no CLOCK_REALTIME) | **PASS** — no time calls in this file. |

**Verdict: NO SCOPE DEVIATION** — `spsc_ring.h` conforms to CLAIM-SPSC-001 requirements.

---

##### `engine/core/pool_allocator.h` — 289 lines

**Scope conformance assessment:**

| Check | Finding |
|-------|---------|
| GC-E-010 (no malloc/new/free/delete) | **PASS** — all storage in `storage_[]` BSS member array (line 86). No heap allocation in acquire(), release(), or any method. Comment at line 133 confirms: "Does not call malloc, new, or any allocator (GC-E-010, CLAIM-L3-POOL-001)." |
| AD-006 (compile-time capacity) | **PASS** — `N` is a compile-time template parameter with `static_assert(N > 0)` at line 68. No runtime sizing. |
| CLAIM-L3-POOL-001 V2 scope | **PASS** — V2 requires code review sign-off + ASAN run showing zero malloc/new calls. The file is structured to produce zero heap events (stack/BSS only). |
| Double-release detection | **PASS** — `assert(in_use_[idx])` in `release()` at line 196. Fires under ASAN as expected by spec §15.3. |
| Trivially destructible constraint | **PASS** — `static_assert(std::is_trivially_destructible_v<T>)` at line 74. |
| GC-E-011 (no CLOCK_REALTIME) | **PASS** — no time calls. |

**Verdict: NO SCOPE DEVIATION** — `pool_allocator.h` conforms to CLAIM-L3-POOL-001 requirements.

---

##### `engine/core/cross_plane.h` — 194 lines

**Scope conformance assessment:**

| Check | Finding |
|-------|---------|
| GC-E-010 (no malloc/new/free/delete) | **PASS** — no allocation. Header defines a macro and inline constants only. |
| GC-E-011 (no CLOCK_REALTIME) | **PASS** — no time calls. |
| HTML §C3.3 macro conformance | **PASS** — `CROSS_PLANE_READ` macro at lines 129–148 implements the spec verbatim: `sizeof(atomic_var) == sizeof(int64_t)` static_assert (line 136), `memory_order_acquire` load (line 147), layer parameters for static analysis annotation. |
| Layer constants L0–L7 | **PASS** — all 8 layer constants defined (lines 71–78) with monotonicity static_assert (lines 82–87). |
| GC-E-044/GC-E-045 pre-registration | **PASS** — rules documented in lines 11–14 and 151–172. Violation fixtures referenced: `violation_GC_E_044.cpp`, `violation_GC_E_045.cpp`. Both fixture files are confirmed present in the untracked list from git status. |
| S1 scope note | Header correctly annotates that L3/L4 are S3 deliverables (line 39–41): "L3 and L4 are implemented in S3. This header is defined in S1 because GC-E-044 and GC-E-045 require pre-registered violation fixtures in S1." This is correct governance practice — no scope violation. |

**Verdict: NO SCOPE DEVIATION** — `cross_plane.h` conforms to spec §C3.3 and §6.3.

---

### ENGINE HEADER REVIEW SUMMARY

| Header | Lines | Verdict | Key Findings |
|--------|-------|---------|-------------|
| `wal_writer.h` | 847 | **NO DEVIATION** | GC-E-010/011 clean; AD-005/012 enforced; S3 enum entries pre-declared (expected) |
| `spsc_ring.h` | 166 | **NO DEVIATION** | AD-006 power-of-2 asserted; cache-line isolation correct; seq_cst/release ordering per spec |
| `pool_allocator.h` | 289 | **NO DEVIATION** | Zero heap allocation; double-release detection; trivially-destructible constraint enforced |
| `cross_plane.h` | 194 | **NO DEVIATION** | §C3.3 macro verbatim; L0–L7 constants; GC-E-044/045 pre-registered correctly |

The original report's Item 5 conclusion ("NO DISCREPANCY" for `shm_layout.h`) was stated partly by extension from these four files never having been checked. That extension is now **supported by direct inspection** — all four headers are scope-conformant.

---

### FILE CREATED

`docs/S1_RECONCILIATION_AUDIT_ADDENDUM.md` — this document.

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
?? docs/S1_RECONCILIATION_AUDIT_ADDENDUM.md    ← THIS SESSION (new)
?? engine/core/cross_plane.h
?? engine/core/pool_allocator.h
?? engine/core/spsc_ring.h
?? engine/core/wal_writer.h
[... remainder unchanged from prior session ...]
```

`docs/S1_RECONCILIATION_AUDIT_ADDENDUM.md` is the **only new file** created this session. No existing files were modified.

---

### STOP — AWAITING FOUNDER REVIEW

*Addendum ID: EGBOS-RECON-S1-001-ADD-01 | Date: 2026-07-11*
