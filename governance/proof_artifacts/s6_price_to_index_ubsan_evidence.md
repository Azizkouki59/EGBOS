# S6 evidence: price_to_index overflow safety (UBSan / libFuzzer)

- Date: 2026-09-21
- Commit: 92f220e01baea135b0034289a6819ab4937ac89a
- Compiler: Ubuntu clang version 21.1.8 (6ubuntu1)

## Commands
clang++ -std=c++20 -g -O1 -fsanitize=fuzzer,undefined,address -fno-sanitize-recover=undefined \
  -I. -I/tmp/egbos_build/schemas/generated -I/tmp/egbos_build/_deps/flatbuffers-src/include engine/tests/test_lob_fuzz.cpp engine/l1/lob_engine.cpp \
  engine/l0/ingestor.cpp engine/l0/binance_json_normalizer.cpp -o /tmp/lob_fuzz_ubsan
/tmp/lob_fuzz_ubsan engine/tests/fuzz_corpus/price_int64min_anchor_underflow.bin
/tmp/lob_fuzz_ubsan /tmp/fuzz_work -max_total_time=600 -max_len=64

## Results (real code)
- Seed replay (price_int64min_anchor_underflow.bin): exit 0, no sanitizer report.
- 600 s run: 1,772,999 executions in 601 s (~2,950 exec/s), cov 56 / ft 86 / corpus 23; log shared by two concurrent runs, so the provenance of this DONE line is unconfirmed. Re-run result: see below.
- Earlier harness (engine rebuilt per input): ~78 exec/s; 120 s run, 9,463 executions, no report.
- Full ctest on tree: 220 of 220 pass, 2 credential-gated skips.

## Mutation checks (each mutation applied with sed, change confirmed via git diff, then restored)
| Guard | Mutation | Observed |
|---|---|---|
| Overflow-safe subtraction | naive price - anchor_price_ | UBSan "signed integer overflow" at lob_engine.cpp:40 on the seed, exit 1 |
| 64-bit clamp | upper clamp removed | SEGFAULT: LobPriceIndexTest.ExtremePricesClampToEdges, LargeNonOverflowingDiffClampsNotWraps |
| tick_size guard | tick_size_ <= 0 block deleted | LobPriceIndexDeathTest.ConstructorRejectsNonPositiveTickSize fails |
| static_assert | kMaxPriceLevels = 131072 | build stops (static assertion failed, lob_engine.h:21) |
| bid/ask split | link_node(..., !is_bid) | CanonicalSequenceSemanticAssertions and ThreeXByteIdenticalReplayAgainstGolden fail |

## Limitations
- Regression evidence for the tested inputs, not a proof for all inputs.
- Coverage plateaued at 56 of 984 counters; most counters belong to ingestor/JSON code this harness never calls.
- The harness fuzzes one fixed anchor (100000000) and tick (100); other anchors are covered only by unit tests.
