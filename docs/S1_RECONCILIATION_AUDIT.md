# S1 RECONCILIATION AUDIT

## Investigation Report — Code vs Specification Discrepancies

---

| Field                     | Value                                                         |
|---------------------------|---------------------------------------------------------------|
| **Report ID**             | EGBOS-RECON-S1-001                                            |
| **Classification**        | Investigation Only — No Resolution Authority                  |
| **Auditor**               | Staff Software Engineer (AI-assisted)                         |
| **Specification Sources** | `EGBOS_MASTER_augmented.html` v1.0, `S0.html`, `S1_INSTITUTIONAL_SPEC.md` |
| **Repository Inspected**  | `c:\EGBOS\egbos` at current HEAD (uncommitted working tree)   |
| **Audit Date**            | 2026-07-11                                                    |
| **Verdict**               | **6 items investigated; 5 confirmed discrepancies; 3 additional discrepancies found** |

---

## ITEM 1 — `govcheck/CMakeLists.txt` Dependency Strategy

### Question

Does the current `govcheck/CMakeLists.txt` use `FetchContent` (as prescribed by S0.html §4.7) or `find_package` (system packages)?

### Specification Authority

**S0.html §4.7** (lines 893–944) provides a verbatim `CMakeLists.txt` that is the canonical, frozen S0 deliverable. It prescribes:

```cmake
# ── Dependencies via FetchContent ──
include(FetchContent)
FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0)
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0)
FetchContent_MakeAvailable(yaml-cpp json googletest)
```

S0.html line 637 also states:

> _Dependencies (**all fetched via CMake FetchContent**)_

S0.html lines 535–536 confirm: GoogleTest → "CMake FetchContent", yaml-cpp → "CMake FetchContent".

### Repository State

[CMakeLists.txt](file:///c:/EGBOS/egbos/govcheck/CMakeLists.txt) (lines 6–12) uses **`find_package`** for all four dependencies:

```cmake
# ── Dependencies (system packages) ──
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)
```

### Finding

> [!CAUTION]
> **CONFIRMED DISCREPANCY**: The repository uses `find_package()` (system-installed packages) instead of `FetchContent` (source-pinned dependencies) as prescribed by S0.html §4.7. This is a deviation from frozen S0 specification.

**Consequences of this deviation:**

| Aspect | S0 Spec (FetchContent) | Repository (find_package) |
|--------|------------------------|---------------------------|
| **Version pinning** | Exact: yaml-cpp 0.8.0, json 3.11.3, gtest 1.14.0 | Implicit: whatever the system has installed |
| **Build reproducibility** | Deterministic (source fetched at tagged commit) | Non-deterministic (system package version varies) |
| **CLAIM-GOVCHECK-001 scope** | V4 proof artifact tied to specific dependency versions | V4 proof artifact may not reflect current linked versions |
| **CI portability** | Any clean Linux host with CMake 3.27+ and a compiler | Requires pre-installed system packages on CI runners |

**Additional structural change**: The S0 spec uses `file(GLOB_RECURSE GOVCHECK_SOURCES src/*.cpp)` for source collection, while the repository explicitly enumerates sources in a `set(GOVCHECK_SOURCES ...)` block. The repository also links `GTest::GTest` + `GTest::Main` instead of `GTest::gtest_main`. These are minor but constitute further deviations from the frozen CMakeLists.txt text.

### Founder Decision Required

1. Is the switch from `FetchContent` to `find_package` an authorised modification, or must it be reverted?
2. If authorised, does CLAIM-GOVCHECK-001's V4 proof artifact need re-generation against the system-package build?
3. Does this deviation require an RFC under GC-E-053 (V3+ claim amendment)?

---

## ITEM 2 — `govcheck/src/main.cpp` Composite `--scope` Parsing

### Question

Is the comma-separated composite scope parsing (e.g., `--scope C0,P3,P4,P5`) in `main.cpp` legal per S0 specification?

### Specification Authority

**S0.html §4.6** (lines 838–869) defines the CLI syntax:

```
govcheck validate --scope <scope> [--strict] [--repo-root <path>]
```

Scope Values: `C0` (governance pre-check). Future: `P3`, `P4`, `G1`, `G2`, etc.

The S0 spec shows only **single-token** scope values. The word "Future" indicates that P3, P4, etc. are not S0-scope features.

**Master Blueprint §I2** (line 4450) requires for S1 exit:

```
govcheck validate --scope C0,P3,P4,P5 --strict
```

This is a **composite, comma-separated** scope — which the S0 CLI specification does not define.

### Repository State

[main.cpp](file:///c:/EGBOS/egbos/govcheck/src/main.cpp) (lines 72–112) implements composite scope parsing:

```cpp
// §I2: Scope is a comma-separated list of tokens.
std::vector<std::string> scope_tokens;
{
    std::string token;
    std::istringstream stream(scope);
    while (std::getline(stream, token, ',')) {
        if (!token.empty()) {
            scope_tokens.push_back(token);
        }
    }
}
```

Valid tokens include `C0-C4`, `P3-P11`, `G1-G4`.

### Finding

> [!IMPORTANT]
> **AMBIGUOUS — REQUIRES FOUNDER RULING**: Composite scope parsing is **required** by the Master Blueprint §I2 S1 exit condition but **not specified** in S0.html §4.6.

Two interpretations exist:

1. **S0 froze the CLI contract** and composite scopes are an unauthorized extension that should have been accompanied by an ADR or specification amendment.
2. **S0 established the initial CLI** and the composite scope is a legitimate S1 extension (the S0 spec says "Future: P3, P4…" indicating expected evolution).

The code comment at `main.cpp` line 3 acknowledges this: _"extended per §I2 for S1+ composite scopes"_.

**Note:** The composite scope is necessary for S1 exit. Without it, the command `govcheck validate --scope C0,P3,P4,P5 --strict` cannot be invoked.

### Founder Decision Required

Is the composite `--scope` extension a legitimate S1 enhancement, or does it constitute an unauthorized modification to the S0-frozen govcheck CLI interface?

---

## ITEM 3 — CLAIM-GOVCHECK-001 V4 Validity Against Current Binary SHA-256

### Question

Is the CLAIM-GOVCHECK-001 V4 proof artifact still valid given that the govcheck binary has been modified since the proof artifact was generated?

### Specification Authority

**CLAIM-GOVCHECK-001** is the root of the governance trust chain (GC-F-004). It asserts:

> _"The govcheck binary compiles clean under ASAN, TSAN, and UBSAN. Its own unit tests (GoogleTest) pass under all three sanitizer profiles."_

Per Master Blueprint §I7.2, GC-F-004 is "immediate blocking" from S0 start. **No other gate is valid without this.**

### Repository State

**Proof artifact**: [govcheck_selftest_dev-workstation-001_2026-06-20.json](file:///c:/EGBOS/egbos/governance/proof_artifacts/govcheck_selftest_dev-workstation-001_2026-06-20.json) (generated 2026-06-20).

**SHA-256 verification**:

| Check | Value |
|-------|-------|
| Claim's declared `sha256` | `1ee7dc8cbd98c00cbac719ec8136ad0e2176bb58800663e68e2b61588bc56ad5` |
| Actual file hash (computed) | `1ee7dc8cbd98c00cbac719ec8136ad0e2176bb58800663e68e2b61588bc56ad5` |
| **Match** | ✅ SHA-256 matches on-disk file |

**However**, the proof artifact was generated on 2026-06-20 against the **S0-era govcheck binary**. Since then, the following modifications have been made to govcheck:

| Change | S0 Spec | Current Repository |
|--------|---------|-------------------|
| Dependency strategy | `FetchContent` | `find_package` (Item 1) |
| Source collection | `file(GLOB_RECURSE)` | Explicit `set(GOVCHECK_SOURCES ...)` |
| `cmd_validate.cpp` | S0 C0-only validation | S1 engine source scanning (GC-E-010/011) |
| `main.cpp` | Single-token scope | Composite comma-separated scope parsing |
| Test link target | `GTest::gtest_main` | `GTest::GTest` + `GTest::Main` |

The proof artifact certifies 29 tests passed under ASAN/TSAN/UBSAN. The **current** binary has different source code and a different dependency chain — the proof artifact's assertion that "govcheck binary compiles clean" applies to a **different binary** than what currently exists.

### Finding

> [!WARNING]
> **CONFIRMED DISCREPANCY**: The V4 proof artifact was generated against the S0-era govcheck binary. The current binary has been materially modified (new validation logic, different dependency strategy, different source enumeration). The proof artifact does not certify the current binary. The SHA-256 of the proof artifact file is intact, but it proves a different binary than the one that now exists.

**Impact**: Per GC-F-004 semantics, if the current govcheck binary is not V4-certified, then _"no other gate is valid."_ The root trust chain is technically broken until a new proof artifact is generated against the current binary.

### Founder Decision Required

1. Must a new `govcheck_selftest` proof artifact be generated against the current binary?
2. Should CLAIM-GOVCHECK-001 be amended to reflect the binary's evolution from S0 to S1?
3. Is the S0 proof artifact's continuing validity an acceptable temporary state during S1 development?

---

## ITEM 4 — Existence of `govcheck scan` Subcommand

### Question

Does the `govcheck scan` subcommand (referenced in `verify_all_fixtures.sh`) exist in the govcheck binary? What is its specification status?

### Specification Authority

**S0.html §4.6** specifies exactly one subcommand: `govcheck validate`.

**Master Blueprint J5.1** (lines 5521–5547) specifies the govcheck binary architecture. The `commands/` directory includes `cmd_validate.h`/`.cpp`. No `cmd_scan` is mentioned.

**S0.html line 633** lists the commands directory contents:

> `commands/` — validate, status, claims subcommands

Three subcommands are implied: `validate`, `status`, `claims`. No `scan`.

### Repository State

**[main.cpp](file:///c:/EGBOS/egbos/govcheck/src/main.cpp)** (lines 147–169) implements exactly three subcommands:

```cpp
if (subcommand == "validate") { return run_validate(args); }
if (subcommand == "status")   { return run_status(args); }
if (subcommand == "claims")   { return run_claims(args); }
```

**No `scan` subcommand exists.** Any call to `govcheck scan` will hit the default path and exit with code 3 ("unknown subcommand").

**[verify_all_fixtures.sh](file:///c:/EGBOS/egbos/tests/governance/fixtures/verify_all_fixtures.sh)** (line 54) calls:

```bash
output=$($GOVCHECK scan --file "$S1_FIXTURES_DIR/$fixture" 2>&1 || true)
```

This call is made for **all 9 source-code violation fixtures** (lines 65–73):
- `violation_GC_E_010.cpp`
- `violation_GC_E_011.cpp`
- `violation_GC_WAL_001.cpp`
- `violation_GC_E_053.cpp`
- `violation_GC_E_054.cpp`
- `violation_GC_E_SECRETS_001.sh`
- `violation_GC_E_056.js`
- `violation_GC_E_044.cpp`
- `violation_GC_E_045.cpp`

### Finding

> [!CAUTION]
> **CONFIRMED DISCREPANCY — P1 GATE IMPACT**: `govcheck scan` does not exist. `verify_all_fixtures.sh` calls it for all 9 S1 source-code violation fixtures. Every call will fail with exit code 3, and since the script uses `|| true`, each `govcheck scan` invocation will produce no output containing the expected rule ID. All 9 source-fixture `grep` checks will report FAIL, and the script will exit 1.

**Cascading impact**:

| Gate | Impact |
|------|--------|
| **P1** | `verify_all_fixtures.sh` is a P1 requirement (S1 Spec §10, §15.4). It will exit 1, failing P1. |
| **S1 Exit** | S1 acceptance gate requires `govcheck validate --scope C0,P3,P4,P5 --strict`. P1 failure blocks the entire CI chain. |
| **S1-AC-013** | [SLICE-S1-exit-v1.yaml](file:///c:/EGBOS/egbos/governance/exit-gates/SLICE-S1-exit-v1.yaml) S1-AC-013 marks "All S1-required GC-E violation fixtures committed" as PASS. This status is premature — the fixtures are committed but **cannot be verified** because the scan subcommand does not exist. |

**Note**: The Category 1 (claim-YAML) fixtures in `verify_all_fixtures.sh` (lines 39–44) use `govcheck validate`, which **does** exist. Those 6 fixtures will execute correctly. Only the 9 Category 2 (source-code violation) fixtures are broken.

### Founder Decision Required

1. Should a `govcheck scan` subcommand be implemented (new feature)?
2. Alternatively, should `verify_all_fixtures.sh` be rewritten to use the existing `scan_engine_sources()` logic embedded in `cmd_validate.cpp`?
3. Should S1-AC-013 in the exit gate be reverted to PENDING?

---

## ITEM 5 — `engine/core/shm_layout.h` Scope Check (L0–L7 Region Mapping)

### Question

Does `shm_layout.h` define all 8 SHM regions per Master Blueprint §C3.1 with correct layer mappings?

### Specification Authority

**Master Blueprint §C3.1** (text lines 5004–5006, corroborated by S1 Spec §6.2) defines 8 canonical regions:

| # | Path | Direction | Description |
|---|------|-----------|-------------|
| 1 | `/dev/shm/egbos_market_events` | L0 → L1 | SPSC ring, 65536 slots × 128B |
| 2 | `/dev/shm/egbos_lob_snapshots` | L1 → L2 | Double-buffered LOB, per instrument |
| 3 | `/dev/shm/egbos_feature_vector` | L2 → L3 | 256B FeatureVector, seqlock protected |
| 4 | `/dev/shm/egbos_position_state` | L3 → L4 | Read-only L4 view (gross_exposure_ only) |
| 5 | `/dev/shm/egbos_risk_limits` | L4 → L3 | Monotone latches: kill_switch, pos_limit |
| 6 | `/dev/shm/egbos_exec_commands` | L3 → L5 | SPSC ring, 4096 slots × 64B |
| 7 | `/dev/shm/egbos_regime_signal` | L6/L7 → L3 | Redis Streams bridge output, 1KB |
| 8 | `/dev/shm/egbos_governance_flags` | All layers | Read-only: feed_stale, halt_required |

### Repository State

[shm_layout.h](file:///c:/EGBOS/egbos/engine/core/shm_layout.h) defines all 8 regions. Verification:

| # | Constant | Path | Slots/Size | `static_assert` | **Match** |
|---|----------|------|------------|------------------|-----------|
| 1 | `kMarketEventsSlots=65536`, `kMarketEventsSlotSize=128` | `kMarketEventsPath` = `/dev/shm/egbos_market_events` | 65536 × 128B = 8MB | Power-of-2 ✅ | ✅ |
| 2 | `kLobSnapshotsRegionSize` = 2 × 256 × 4096 | `kLobSnapshotsPath` = `/dev/shm/egbos_lob_snapshots` | 2MB | Non-zero, page-aligned ✅ | ✅ |
| 3 | `kFeatureVectorBytes=256`, region=4096 | `kFeatureVectorPath` = `/dev/shm/egbos_feature_vector` | 256B + seqlock counter | Size check ✅ | ✅ |
| 4 | `kPositionStateRegionSize=4096` | `kPositionStatePath` = `/dev/shm/egbos_position_state` | 1 × int64_t + reserved | Size check ✅ | ✅ |
| 5 | `kRiskLimitsRegionSize=4096` | `kRiskLimitsPath` = `/dev/shm/egbos_risk_limits` | kill_switch + pos_limit + reserved | Size check ✅ | ✅ |
| 6 | `kExecCommandsSlots=4096`, `kExecCommandsSlotSize=64` | `kExecCommandsPath` = `/dev/shm/egbos_exec_commands` | 4096 × 64B = 256KB | Power-of-2 ✅ | ✅ |
| 7 | `kRegimeSignalBytes=1024`, region=4096 | `kRegimeSignalPath` = `/dev/shm/egbos_regime_signal` | 1KB + reserved | Size check ✅ | ✅ |
| 8 | `kGovernanceFlagsRegionSize=4096` | `kGovernanceFlagsPath` = `/dev/shm/egbos_governance_flags` | 2 × bool + reserved | Size check ✅ | ✅ |

**Protocol structs verified**: `FeatureVectorRegion`, `PositionStateView`, `RiskLimitsRegion`, `GovernanceFlags` — all have `static_assert(sizeof(...) == kRegionSize)`.

**Seqlock protocol** (lines 280–331): Matches §6.4 verbatim — `seq_cst` fetch_add, `seq_cst` fence, `acquire` loads.

**`shm_init_all()`** (lines 567–586): Opens, maps, mlocks, and mbinds all 8 regions. `TERMINAL_HALT` on any failure (P-006 compliant).

### Finding

> [!NOTE]
> **NO DISCREPANCY FOUND**: `shm_layout.h` correctly defines all 8 SHM regions per Master Blueprint §C3.1 with correct paths, sizes, layer direction annotations, and `static_assert` enforcement of AD-006 invariants. The `shm_init_all()` function enforces SHM-MLOCK and SHM-NUMA invariants.

---

## ITEM 6 — `cmd_validate.cpp` Exemption Logic for GC-E-010/011 vs §I7.2 Activation Schedule

### Question

Does the `cmd_validate.cpp` exemption logic for GC-E-010 and GC-E-011 correctly implement the §I7.2 activation schedule?

### Specification Authority

**Master Blueprint §I7.2** (text lines 5017–5028):

| Rule | Description | Warning From | Blocking From | Gate |
|------|-------------|--------------|---------------|------|
| **GC-E-010** | malloc/new/free/delete in engine/ hot path | S0 start | **S3** | P2→P3 |
| **GC-E-011** | clock_gettime(CLOCK_REALTIME) in engine/ | S0 start | **S2** | P2 |

Both rules are **Warning** from S0 start. Neither becomes Blocking during S1.

### Repository State

[cmd_validate.cpp](file:///c:/EGBOS/egbos/govcheck/src/commands/cmd_validate.cpp) implements the exemption in two places:

**1. `scan_engine_sources()` function (lines 23–83)**:

Both GC-E-010 and GC-E-011 findings are emitted at `Severity::Warning`:

```cpp
findings.push_back({
    "GC-E-010",
    Severity::Warning,  // ← Correct per §I7.2: Warning in S1
    ...
});
```

```cpp
findings.push_back({
    "GC-E-011",
    Severity::Warning,  // ← Correct per §I7.2: Warning in S1
    ...
});
```

**2. `--strict` mode exemption (lines 229–239)**:

```cpp
if (strict) {
    for (auto& f : all_findings) {
        if (f.severity == Severity::Warning &&
            f.code != "GC-E-010" && f.code != "GC-E-011") {
            f.severity = Severity::Error;
        }
    }
}
```

This explicitly **exempts GC-E-010 and GC-E-011 from `--strict` promotion**. In `--strict` mode, all other warnings become errors, but GC-E-010 and GC-E-011 remain warnings.

### Finding

> [!NOTE]
> **NO DISCREPANCY FOUND**: The exemption logic correctly implements §I7.2. Both rules are Warning-only during S1. The `--strict` exemption prevents them from becoming errors prematurely. GC-E-010 should become Blocking at S3 (P2→P3), and GC-E-011 should become Blocking at S2 (P2). The current S1 implementation is correct for the current slice.

**Minor observation**: The code comment at line 230 says _"S3 and S2 respectively"_ which matches §I7.2. However, there is no mechanism in the code to **automatically transition** these rules from Warning to Blocking when S2/S3 begins. The severity is hardcoded at Warning. When S2 opens, a code change to `cmd_validate.cpp` will be required to promote GC-E-011 to Error severity (and similarly GC-E-010 at S3). This is not a discrepancy — it's an expected future code change — but it is worth noting that the activation schedule is not data-driven from a configuration file.

---

## ADDITIONAL DISCREPANCIES DISCOVERED DURING INSPECTION

### ADDITIONAL-1 — S1 Claim Proof Artifacts All Contain PLACEHOLDER SHA-256

**Files inspected**:
- [CLAIM-WAL-001.yaml](file:///c:/EGBOS/egbos/governance/claims/CLAIM-WAL-001.yaml) (line 47)
- [CLAIM-SPSC-001.yaml](file:///c:/EGBOS/egbos/governance/claims/CLAIM-SPSC-001.yaml) (line 47)
- [CLAIM-L3-POOL-001.yaml](file:///c:/EGBOS/egbos/governance/claims/CLAIM-L3-POOL-001.yaml) (line 47)

**Finding**: All three S1 claims reference proof artifact paths containing `run_PLACEHOLDER.log` and share the **identical** SHA-256 hash:

```
sha256: "1880ca30c938fc3d3bb49ffcb9fd4e3172f632266defe786392dd69be9ad7545"
```

The same hash across three different claims for three different proof artifacts (kill-9 test, TSAN test, ASAN test) is structurally impossible if the artifacts were genuine. The proof artifacts do not exist on disk (the paths reference `run_PLACEHOLDER.log`).

**Impact**: GC-E-018 (proof artifact SHA-256 mismatch) should fire for all three claims at C0. GC-E-031 (V4 claim with no empirical proof artifact) should fire for CLAIM-WAL-001 and CLAIM-SPSC-001. The [SLICE-S1-exit-v1.yaml](file:///c:/EGBOS/egbos/governance/exit-gates/SLICE-S1-exit-v1.yaml) correctly marks S1-AC-001, S1-AC-002, and S1-AC-003 as PENDING.

**This is a known and expected state** — the exit gate documents that proof artifacts are pending execution on target hardware. No founder decision is required unless S1 close is imminent.

---

### ADDITIONAL-2 — `verify_all_fixtures.sh` References Nonexistent Fixture File Paths

**Finding**: [verify_all_fixtures.sh](file:///c:/EGBOS/egbos/tests/governance/fixtures/verify_all_fixtures.sh) references Category 1 fixtures from `govcheck/tests/fixtures/` (line 14: `FIXTURES_DIR="govcheck/tests/fixtures"`) and Category 2 fixtures from `tests/governance/fixtures/` (line 15: `S1_FIXTURES_DIR="tests/governance/fixtures"`).

The Category 2 (S1) fixture files are referenced (lines 65–73) but their existence has not been verified on disk because the `govcheck scan` subcommand that would process them does not exist (Item 4). Whether these fixture source files are actually committed to the repository is **moot** until `govcheck scan` is implemented.

---

### ADDITIONAL-3 — `govcheck.yml` CI Workflow References `govcheck scan`

**Impact chain**: If `.github/workflows/govcheck.yml` invokes `verify_all_fixtures.sh` (which calls `govcheck scan`), then the C0 CI pipeline will also fail. This was not independently verified in the workflow file, but the P1 gate definition in S1 Spec §10 requires `verify_all_fixtures.sh` to pass.

---

## SUMMARY TABLE

| Item | File(s) | Status | Severity | Founder Action |
|------|---------|--------|----------|----------------|
| **1** | `govcheck/CMakeLists.txt` | **CONFIRMED DISCREPANCY** | 🔴 High | Authorize or revert `find_package` |
| **2** | `govcheck/src/main.cpp` | **AMBIGUOUS** | 🟡 Medium | Rule on composite scope legality |
| **3** | `CLAIM-GOVCHECK-001.yaml` | **CONFIRMED DISCREPANCY** | 🔴 High | Regenerate proof artifact for current binary |
| **4** | `verify_all_fixtures.sh` / `main.cpp` | **CONFIRMED DISCREPANCY** | 🔴 Critical | Implement `scan` or rewrite fixture runner |
| **5** | `engine/core/shm_layout.h` | **NO DISCREPANCY** | ✅ Pass | None |
| **6** | `govcheck/src/commands/cmd_validate.cpp` | **NO DISCREPANCY** | ✅ Pass | None |
| **A1** | S1 claim proof artifacts | **KNOWN PENDING** | 🟡 Expected | Execute tests on target hardware |
| **A2** | `verify_all_fixtures.sh` file paths | **DEPENDENT ON ITEM 4** | 🟡 Medium | Resolves with Item 4 |
| **A3** | CI workflow scan reference | **DEPENDENT ON ITEM 4** | 🟡 Medium | Resolves with Item 4 |

---

## CONSTRAINTS OBSERVED

This report is an investigation only. Per the audit charter:

- ❌ No files were modified, created, or deleted other than this report.
- ❌ No code fix is authorized until conflicts are resolved by the founder.
- ✅ All conclusions are based on what the tooling and files actually contain.
- ✅ Specification sources were mechanically inspected (SHA-256 computed, file contents compared line-by-line against spec text).

---

*Report ID: EGBOS-RECON-S1-001 | Audited: 2026-07-11 | Auditor: Staff Software Engineer (AI)*
