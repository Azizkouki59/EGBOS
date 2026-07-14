#!/usr/bin/env bash
# tests/governance/fixtures/violation_GC_E_SECRETS_001.sh
# §15.4 Violation Fixture: GC-E-SECRETS-001 (secret in tracked file)
#
# This file intentionally contains patterns that look like secrets
# (API keys, private keys, tokens). govcheck secret scanning must
# detect this and fire GC-E-SECRETS-001.
# If this fixture passes CI without triggering GC-E-SECRETS-001,
# the rule is broken.
#
# Expected: govcheck reports GC-E-SECRETS-001 on this file.

# VIOLATION: Secret material in a tracked file
export AWS_SECRET_ACCESS_KEY="AKIAIOSFODNN7EXAMPLE+wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
export API_TOKEN="ghp_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef012345"
export PRIVATE_KEY="-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEA0Z3VS5JJcds3xfn/ygWyF9PbnGEYPRz
-----END RSA PRIVATE KEY-----"

echo "This script intentionally contains secret-like patterns for GC-E-SECRETS-001 fixture testing."
