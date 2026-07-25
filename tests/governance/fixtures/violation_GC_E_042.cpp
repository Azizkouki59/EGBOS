// violation_GC_E_042.cpp
// Contains business logic in a FIX consumer context.
// Should trigger GC-E-042.

void handle_fix_message() {
    if (true) {
        // Business logic detected
    }
}
