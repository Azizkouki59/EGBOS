# GOVERNANCE DECISION PACKAGE — S5 AC-001 WI-FI QUALIFICATION

## BACKGROUND

During S5-AC-001 preflight on 2026-09-09, a governance gap was identified: the authoritative documents (`SLICE-S5-exit-v1.yaml`, `EGBOS_MASTER.md`, `S5_INSTITUTIONAL_SPEC.md`, and `MARKET-DATA-001.yaml`) require "empirical measurement on live data" for the startup buffer sizing, but do not codify an acceptable network medium (e.g., Ethernet vs. Wi-Fi) for this specific measurement.

The physical execution environment is currently operating over a Wi-Fi interface (`wlo1`).

D5 mandates Linux Tier-0 (satisfied via bare-metal Ubuntu and `isolcpus=2-11`), and D9 requires explicit production feed provenance. Neither architectural decision prohibits Wi-Fi.

This decision package formally requests authorization to use Wi-Fi for the **S5-AC-001** qualification run only, unblocking live operations.

## PROPOSED POLICY BOUNDARY

* **EXPLICITLY ALLOWED:** Wi-Fi (`wlo1`) may be used for the S5-AC-001 empirical live-feed jitter measurement, provided the live Binance stream (`wss://stream.binance.com:9443`) is genuinely reachable and the artifact is collected against live production data.
* **EXPLICITLY PROHIBITED:** Wi-Fi does NOT qualify as the network medium for the S5-AC-003 336-hour continuous operational trial or S5-AC-004 nightly replays. These require separate authorization or Ethernet connectivity.

## BOARD DECISIONS REQUIRED

### GOV-S5-36-A — S5-AC-001 Network Medium Authorization
* `A1 = AUTHORIZE_WIFI_FOR_AC001_ONLY`
* `A2 = REQUIRE_ETHERNET_FOR_AC001`

### GOV-S5-36-B — Specification Amendment Authorization
* `B1 = AUTHORIZE_INSTITUTIONAL_SPEC_AND_MARKET_DATA_AMENDMENT`
* `B2 = DEFER_AMENDMENT`

---

## REQUIRED BOARD INPUT

```text
GOV-S5-36-A: <A1 or A2>
GOV-S5-36-B: <B1 or B2>

Decision Authority: Governance Board
Decision Reference: <BOARD INPUT REQUIRED>
Decision Timestamp: <BOARD INPUT REQUIRED>
```
