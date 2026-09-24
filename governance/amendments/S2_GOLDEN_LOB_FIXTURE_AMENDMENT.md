# Amendment: S2 golden_lob.bin fixture re-pin

- Amends: SLICE-S2-exit-v1.yaml (line 41), CLAIM-LOB-001.yaml (class_rationale)
- Fixture: governance/proof_artifacts/golden_lob.bin
- Old SHA-256 (signed, commit 2465359): c97b85f63d8dc5ec14d4e2c308e3596059d2304e4f535d88a4214c4c5ca4d007
- New SHA-256 (commit 8d3cffd):         c64ebd32be50fd29385f15f0fdb28115fd2388bedd6cd2288cf1fe25e4c17440

## Finding
The fixture signed at S2 encoded cross-book corruption. Decoded under the old Top5BidsAsks layout:
bid offsets (1,-1,-2) / ask offsets (-2,-1,1), i.e. ask levels in the bid array and a bid level
in the ask array. Expected result of the canonical sequence in test_lob_determinism.cpp:
bid [+1 x 10], ask [-2 x 10, -1 x 25]. The regenerated fixture matches exactly.

## Cause
- e5a7255: physical split of bid/ask price-level arrays (fixes the corruption).
- e5a7255 (same commit): Top5BidsAsks ABI change (qty uint16->uint32, offsets int32->int16, fields reordered).
- 8d3cffd: zero-init of levels; regenerated golden WITHOUT the governance decision that
  test_lob_determinism.cpp requires. This amendment is the retroactive record.

## Impact
- e5a7255 also changed snapshot_abi.h: SerializedOrder's uint32 _pad became uint8 is_bid + uint8 _pad[3]
  (size unchanged). kFormatVersion and kSchemaVersion were NOT bumped (both still 1). A pre-e5a7255
  snapshot would load with is_bid=0 (all orders as asks) if its padding was zero.
  No snapshot files were found on the dev host (only scratch WALs). Resolved in f10cc5a: kSchemaVersion bumped to 2
  and the deserializer rejects older snapshots (covered by RejectsOldSchemaVersionWithValidHash).
- Determinism (3x byte-identical replay) re-verified on 79bde99: 213/213 ctest pass.

## Required follow-up
1. Governance decision + signature: TODO_DECISION_ID / TODO_SIGNER / TODO_DATE
2. Re-pin SLICE-S2-exit-v1.yaml and CLAIM-LOB-001.yaml to the new hash via the re-sign process.
3. Add a semantic test asserting the expected bid/ask contents, not only byte equality.
