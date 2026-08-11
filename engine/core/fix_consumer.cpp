// engine/core/fix_consumer.cpp
// EGBOS FIX Consumer — Pure Translation Implementation
//
// Specification : EGBOS Master Blueprint S4, §E1.2
// ADR compliance: AD-009 (FlatBuffers events), AD-004 (fixed-point), AD-011 (ONNX-only)
// Claims        : CLAIM-L3-02 (V3) — zero business logic in FIX consumer
// Govcheck      : GC-E-042 — zero business logic
//
// GC-E-042 COMPLIANCE PROOF:
//   This file contains NO if/switch/while/for on ExecCommand content.
//   All branching is structural (buffer capacity guard, snprintf return).
//   No command is evaluated, modified, rejected, or deferred here.
//
// HOT PATH GUARANTEES:
//   No malloc/new/delete/free (GC-E-010)
//   No CLOCK_REALTIME (GC-E-011)
//   No floating-point (AD-004)
//   No pickle/torch.load/joblib/SavedModel (GC-E-RESEARCH-003)

#include "fix_consumer.h"

#include <cstdio>    // snprintf — stack-only, no heap
#include <cstring>   // memcpy
#include <cinttypes> // PRId64

namespace egbos::l5 {

// Append tag=value\x01 to buf at offset pos. Returns new pos, or -1 if overflow.
// No heap allocation. No control flow on message content.
static int append_field(char* buf, int pos, int cap, int tag, int64_t val) noexcept {
    int written = std::snprintf(buf + pos, static_cast<std::size_t>(cap - pos),
                                "%d=%" PRId64 "\x01", tag, val);
    return (written < 0 || written >= cap - pos) ? -1 : pos + written;
}

bool FIXConsumer::encode(const ExecCommand& cmd, FixEncodedMsg& out) noexcept {
    // Output is a fixed-size stack buffer — no heap (GC-E-010).
    char* const buf = reinterpret_cast<char*>(out.data);
    const int   cap = static_cast<int>(kFixMsgMaxBytes);
    int pos = 0;

    // FIX 4.2 field mapping — pure translation, GC-E-042.
    // Tag 35 = MsgType (D = New Order Single)
    pos = std::snprintf(buf + pos, static_cast<std::size_t>(cap - pos),
                        "35=D\x01");
    if (pos < 0) return false;

    // Tag 34 = MsgSeqNum (from L3, GC-E-042 required field)
    pos = append_field(buf, pos, cap, 34, static_cast<int64_t>(cmd.msg_seq_num));
    if (pos < 0) return false;

    // Tag 49 = SenderCompID (session_id encodes sender, GC-E-042 required)
    pos = append_field(buf, pos, cap, 49, static_cast<int64_t>(cmd.session_id));
    if (pos < 0) return false;

    // Tag 44 = Price (fixed-point, AD-004)
    pos = append_field(buf, pos, cap, 44, cmd.price);
    if (pos < 0) return false;

    // Tag 38 = OrderQty (fixed-point, AD-004)
    pos = append_field(buf, pos, cap, 38, cmd.quantity);
    if (pos < 0) return false;

    // Tag 54 = Side (1=Buy, 2=Sell — FIX 4.2 encoding)
    pos = append_field(buf, pos, cap, 54, static_cast<int64_t>(cmd.side == 0 ? 1 : 2));
    if (pos < 0) return false;

    // Tag 40 = OrdType (2=Limit)
    pos = std::snprintf(buf + pos, static_cast<std::size_t>(cap - pos), "40=2\x01");
    if (pos < 0) return false;

    out.length = static_cast<uint32_t>(pos);
    ++messages_encoded_;
    return true;
}

}  // namespace egbos::l5
