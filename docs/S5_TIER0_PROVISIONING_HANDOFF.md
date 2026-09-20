# EGBOS S5 — Tier-0 Provisioning Handoff

## Required Host
* Bare-metal Linux
* Approved Tier-0 environment
* `isolcpus` configured (to isolate threads for polling)
* `nohz_full` configured (to avoid kernel tick interruptions)
* `rcu_nocbs` where required by governance
* Stable CPU topology
* Production network connectivity
* Persistent WAL storage (NVMe)
* Synchronized system environment (PTP / chrony)
* Required OpenSSL/TLS support (OpenSSL 1.1.1+ / 3.0+)

## Required Environment Variables
* `BINANCE_PROD_API_KEY`
* `BINANCE_PROD_API_SECRET`
*(Never commit these secrets or write their values to disk)*

## Required Validation
Before proceeding with any EGBOS execution, verify the environment:
```bash
uname -a
cat /proc/cmdline
lscpu
nproc
grep -E 'isolcpus|nohz_full|rcu_nocbs' /proc/cmdline
```
Ensure Binance WebSocket connectivity is available on port 9443:
```bash
nc -vz stream.binance.com 9443
```

## Required EGBOS Binaries/Scripts

The following scripts must be executed in order on the Tier-0 host to complete S5 qualification:

### 1. Preflight Checklist
```bash
./scripts/preflight.sh
```

### 2. S5-AC-001 (Market Data Jitter/Latency)
Execute the real feed qualification (must be built via CMake first):
```bash
./build_s5/s5_live_binance_qualification
```

### 3. S5-AC-003 (336-Hour Continuous Live Trial)
Start the 14-day uptime trial:
```bash
./scripts/s5_336h_runner.sh
```

### 4. S5-AC-004 (14-Night Live WAL Replay)
During and after the trial, run the nightly replay check against the live WAL:
```bash
./scripts/run_kill9_wal_test.sh
```

### 5. S5-AC-005 (Final Govcheck Deploy Gate)
After all evidence artifacts have been generated and committed:
```bash
./build_s5/govcheck/govcheck deploy-gate --target production
```
