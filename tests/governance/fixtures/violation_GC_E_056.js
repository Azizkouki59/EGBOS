// tests/governance/fixtures/violation_GC_E_056.js
// §15.4 Violation Fixture: GC-E-056 (SIM_STATE accesses production SHM)
//
// This JavaScript fixture intentionally accesses production shared memory
// paths from a SIM_STATE context. govcheck source scanning must detect
// this and fire GC-E-056.
// If this fixture passes CI without triggering GC-E-056, the rule is broken.
//
// Per HTML §B1.4.3: JavaScript fixture for SIM_STATE production-SHM access.
//
// Expected: govcheck reports GC-E-056 on this file.

// VIOLATION: SIM_STATE code accessing production SHM path
const SHM_PRODUCTION_PATH = "/dev/shm/egbos_production";

function readProductionShm() {
    // GC-E-056: SIM_STATE accesses production SHM
    const fs = require('fs');
    const data = fs.readFileSync(SHM_PRODUCTION_PATH);
    return data;
}

// This simulates a sim-state module importing production SHM directly
// instead of using the simulation SHM overlay.
const SIM_STATE = true;
if (SIM_STATE) {
    // Direct production SHM access from SIM_STATE — forbidden
    readProductionShm();  // GC-E-056 violation
}
