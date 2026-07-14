// tests/governance/fixtures/violation_GC_E_053.cpp
// §15.4 Violation Fixture: GC-E-053 (V3+ claim amended without RFC)
//
// This file is a companion to the YAML fixture that tests whether
// govcheck detects a V3+ claim amendment without a corresponding RFC
// reference. The violation is in the claim YAML, not in C++ code.
//
// This .cpp fixture exists to satisfy the §15.4 naming convention.
// The actual test is a claim YAML that:
//   1. Has verification_class "V3" or higher
//   2. Has a non-empty amendments[] array
//   3. Has an empty rfc_references[] array
//
// Expected: govcheck reports GC-E-053 on the companion claim YAML.

// Intentionally empty C++ — the violation is in claim schema, not source code.
// This file exists as the fixture entry point for the verify script.
