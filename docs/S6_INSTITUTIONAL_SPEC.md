# S6 Institutional Specification: Production Hardware Qualification

**Authority:** EGBOS Governance Board  
**Governs:** Phase III Entry / S6  
**Status:** ACTIVE

## 1. Overview
Slice 6 (S6) constitutes the transition from synthetic testing and paper simulations into the physical production environment. The EGBOS execution engine (ENGINE_V3) depends on strict hardware determinism, cache affinity, and kernel bypass to achieve microsecond-scale latencies. S6 enforces the procurement and benchmarking of this hardware.

## 2. Hardware and Environment Requirements

### 2.1 Colocation (PROC-001)
- **Facility:** Tier-3 or Tier-4 equivalent.
- **Latency Budget:** One-way latency to the primary Binance matching engine must not exceed 2ms.
- **Power & Security:** Minimum N+1 redundancy, SOC-2 Type II certified.

### 2.2 Execution CPU (PROC-002)
- **Architecture:** AMD EPYC 9004 series (Genoa/Bergamo) or Intel Xeon Sapphire Rapids.
- **Cores:** Minimum 32 physical cores.
- **Topology:** NUMA topology must be explicitly documented and mapped.
- **Affinity:** `ENGINE_V3` ring threads must be pinned to isolated cores (`config/cpu-affinity.yaml`) using `isolcpus`.

### 2.3 Storage (PROC-003)
- **Interface:** PCIe Gen 4.0 NVMe (dedicated controller, separate from OS).
- **Capacity:** Minimum 2TB usable.
- **Latency:** `fsync` latency must be strictly `< 100µs` at the 99th percentile (P99).

### 2.4 Network (PROC-004)
- **Hardware:** 10 GbE capable NIC with hardware bypass support (DPDK or RDMA).
- **Latency:** Ping-pong loopback latency across the kernel bypass stack must measure `< 5µs` at P99.
- **Configuration:** Interrupt coalescing MUST be disabled for latency-critical rings.

## 3. Exit Gates
To officially exit S6 and enter S7 (Paper Trading), the Founder must physically measure the hardware and commit the following to the governance repository:
1. All four `PROC` records countersigned.
2. `config/cpu-affinity.yaml` configured.
3. `timing_constants.json` populated with empirical measurements from the server.
4. `LIVE-CAPITAL-001.yaml` signed to formally declare staging capital commitment.
