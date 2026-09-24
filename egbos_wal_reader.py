#!/usr/bin/env python3
import struct
import sys
import os

# EGBOS WAL Record Header Format (Spec §4.3)
# 72 bytes total
# uint64_t sequence;          (8 bytes)
# uint64_t tsc_before_write;  (8 bytes)
# uint64_t tsc_after_write;   (8 bytes)
# uint32_t record_type;       (4 bytes)
# uint32_t instrument_id;     (4 bytes)
# uint64_t order_id;          (8 bytes)
# int64_t  quantity_delta;    (8 bytes)
# int64_t  price;             (8 bytes)
# uint32_t payload_length;    (4 bytes)
# uint32_t header_crc32c;     (4 bytes)
# uint8_t  _pad[8];           (8 bytes)
# Total = 8+8+8+4+4+8+8+8+4+4+8 = 72 bytes

WAL_HEADER_FMT = "<QQQIIqqqII8s"
WAL_HEADER_SIZE = struct.calcsize(WAL_HEADER_FMT)
PRICE_SCALE = 100_000.0

RECORD_TYPES = {
    1: "ORDER_NEW",
    2: "FILL_APPLIED",
    3: "POSITION_SYNC",
    4: "MASS_CANCEL",
    5: "REGIME_CHANGE",
    6: "SESSION_START",
    7: "HITL_HALT_CONFIRM",
    8: "HITL_RESTART_AUTH",
    9: "HITL_RISK_LIMIT_CHANGE",
    10: "HITL_SIGNAL_WEIGHT_CHANGE",
    11: "HITL_REGISTRY_EMERGENCY_AMEND",
    12: "CAPITAL_GATE_CLOCK_START",
}

def parse_wal(filepath):
    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}")
        sys.exit(1)

    print(f"Opening WAL file: {filepath}\n")
    print(f"{'SEQ':<8} | {'TYPE':<22} | {'INST':<5} | {'ORDER_ID':<12} | {'PRICE':<12} | {'QTY':<8} | {'PAYLOAD_LEN'}")
    print("-" * 90)

    try:
        with open(filepath, "rb") as f:
            while True:
                header_bytes = f.read(WAL_HEADER_SIZE)
                if not header_bytes or len(header_bytes) < WAL_HEADER_SIZE:
                    break

                fields = struct.unpack(WAL_HEADER_FMT, header_bytes)
                
                seq = fields[0]
                rec_type = fields[3]
                inst_id = fields[4]
                order_id = fields[5]
                qty_delta = fields[6]
                price = fields[7] / PRICE_SCALE
                payload_len = fields[8]
                
                # We skip payload bytes for display, but advance the file pointer
                if payload_len > 0:
                    f.read(payload_len)

                type_name = RECORD_TYPES.get(rec_type, f"UNKNOWN({rec_type})")
                
                print(f"{seq:<8} | {type_name:<22} | {inst_id:<5} | {order_id:<12} | {price:<12.5f} | {qty_delta:<8} | {payload_len}")
    except Exception as e:
        print(f"\nError reading WAL: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: ./egbos_wal_reader.py <path_to_wal_primary.bin>")
        sys.exit(1)
        
    parse_wal(sys.argv[1])
