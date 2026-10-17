# Feature Contract: FEAT-0002 Level-1 Order Flow Imbalance (OFI)
**Status:** Ratified
**Feature ID:** 0x0002

## 1. Payload Layout
The output is a 32-bit float (`float`) occupying bytes 4–7 of the slot payload in the L2 FeatureVector.
- Slot bytes 0–3: FEAT-0001 Hawkes Intensity
- Slot bytes 4–7: FEAT-0002 Level-1 OFI
- Validity mask bit: bit 1 (mask |= 1u << 1)

## 2. Mathematical Definition
Computes the Level-1 Cont-Kukanov-Stoikov (CKS) Order Flow Imbalance:
`OFI = e_n^b - e_n^a`

Where top-of-book best bid is `(P_n^b, Q_n^b)` and best ask is `(P_n^a, Q_n^a)`.
Reconstructed absolute prices are used:
`absolute_price = anchor_price + level_offset * tick_size`

Bid side event contribution:
- If `P_n^b > P_{n-1}^b`: `e_n^b = +Q_n^b`
- If `P_n^b == P_{n-1}^b`: `e_n^b = Q_n^b - Q_{n-1}^b`
- If `P_n^b < P_{n-1}^b`: `e_n^b = -Q_{n-1}^b`

Ask side event contribution:
- If `P_n^a < P_{n-1}^a`: `e_n^a = +Q_n^a` (ask improved, increased selling pressure reduces OFI)
- If `P_n^a == P_{n-1}^a`: `e_n^a = Q_n^a - Q_{n-1}^a` (ask size increase reduces OFI)
- If `P_n^a > P_{n-1}^a`: `e_n^a = -Q_{n-1}^a` (ask moved away, reduced selling pressure increases OFI)

Cold Start & Gap Events:
- On cold start (first snapshot) or after gap event reset: record current state, return `OFI = 0.0f`.
