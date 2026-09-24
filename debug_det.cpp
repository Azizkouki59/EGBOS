#include "engine/tests/test_lob_determinism.cpp"
int main() {
    auto s1 = run_canonical_sequence();
    auto s2 = run_canonical_sequence();
    printf("memcmp(s1,s2): %d\n", std::memcmp(&s1, &s2, sizeof(s1)));
    return 0;
}
