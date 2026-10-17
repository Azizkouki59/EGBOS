# Feature Contract: FEAT-0001 Hawkes Intensity
**Status:** Ratified
**Feature ID:** 0x0001

## 1. Payload Layout
The output is a 32-bit float (`float`) occupying bytes 0-3 of the slot payload in the L2 FeatureVector.

## 2. Mathematical Definition
The feature computes a univariate Hawkes process intensity using an exponential excitation kernel:
`λ(t) = μ + Σ α * exp(-β * (t - t_i))`
- `μ` (baseline): 0.1
- `α` (jump): 0.05
- `β` (decay): 0.01

## 3. Parity Reference Strategy
The C++ output will be tested against an offline Python reference model (Polars/NumPy) via DIRC byte-identical comparison to guarantee math equivalence.
