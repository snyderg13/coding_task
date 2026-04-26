# Stage 2 Technical Specification: Per-Type Handler Dispatch

## Overview

Stage 2 replaces the single-callback model from Stage 1 with a **handler dispatch
table** keyed by packet type. The parser retains its existing state machine and
checksum validation; the only change is in what happens after a valid packet is
received.

---

## Public API

```c
/* parser.h */

#define MAX_HANDLERS  8u

#define PACKET_TYPE_TEMPERATURE  0x01u
#define PACKET_TYPE_HUMIDITY     0x02u

typedef void (*packet_cb_t)(uint8_t type, const uint8_t *payload, uint8_t len);

void parser_init(void);
int  parser_register_handler(uint8_t type, packet_cb_t cb);
void parser_feed(uint8_t byte);
```

### `parser_init(void)`
- `memset` the parser context to zero.
- `memset` the handler table to zero.
- Reset `handler_count` to `0`.
- Set `ctx.state = STATE_WAIT_START`.

### `parser_register_handler(uint8_t type, packet_cb_t cb)`
1. Linear scan `handlers[0 .. handler_count-1]` for an entry whose `.type == type`.
2. **Found:** set `.cb = cb`, return `0`.
3. **Not found, table not full** (`handler_count < MAX_HANDLERS`): append
   `{type, cb}` at index `handler_count`, increment `handler_count`, return `0`.
4. **Not found, table full:** return `-1`.

### `parser_feed(uint8_t byte)` — `STATE_READ_CHECKSUM` change
```
if (byte == ctx.checksum) {
    for i in 0 .. handler_count-1:
        if handlers[i].type == ctx.type AND handlers[i].cb != NULL:
            call handlers[i].cb(ctx.type, ctx.payload, ctx.len)
            break
}
reset()
```
All other states are unchanged from Stage 1.

---

## Internal Data Structures

```c
typedef struct {
    uint8_t     type;
    packet_cb_t cb;
} handler_entry_t;

static handler_entry_t handlers[MAX_HANDLERS];
static uint8_t         handler_count;
```

A linear array is chosen over a 256-entry lookup table to keep RAM usage bounded
and independent of the type-byte range — appropriate for microcontroller targets.
With `MAX_HANDLERS = 8` the worst-case scan is 8 comparisons per packet.

---

## Packet Type: Humidity (`0x02`)

| Byte offset | Field | Value |
|---|---|---|
| 0 | START | `0xAA` |
| 1 | TYPE | `0x02` |
| 2 | LEN | `0x02` |
| 3 | RH low byte | `rh & 0xFF` |
| 4 | RH high byte | `rh >> 8` |
| 5 | CHECKSUM | `0x02 ^ 0x02 ^ payload[0] ^ payload[1]` |

Encoding: unsigned 16-bit little-endian integer, units 0.1 % RH.  
Range: 0 – 1000 (0.0 % – 100.0 % RH).

---

## Test Coverage

| Test | Requirements covered |
|---|---|
| `test_humidity_valid` | Decode `0x02` packet end-to-end |
| `test_dispatch_to_correct_handler` | Per-type routing with two types live |
| `test_unregistered_type_ignored` | REQ-S2-02 |
| `test_handler_overwrite` | REQ-S2-05; confirms slot is not duplicated |
| `test_handler_table_full` | REQ-S2-03, REQ-S2-04, REQ-S2-05 |
| `test_mixed_type_sequence` | Interleaved multi-type stream integrity |
| Stage 1 tests (13, updated) | REQ-S2-01, REQ-S2-06 |

---

## Design Decisions

**Why linear scan instead of direct-indexed table?**  
A `handlers[256]` array indexed by type byte costs 256 × `sizeof(packet_cb_t)`
(1–2 KB on a 32/64-bit target). That is unacceptable on many MCUs with 8–64 KB
RAM. A linear scan over ≤ 8 entries adds negligible latency and costs only
`MAX_HANDLERS × (1 + sizeof(pointer))` bytes.

**Why does `parser_init` clear the handler table?**  
`parser_init` represents a full reset (e.g. after a fault or re-enumeration).
Preserving stale handlers across a reset could route packets to callbacks that
are no longer valid. Callers that want to retain handlers simply do not call
`parser_init`.

**Why return `int` from `parser_register_handler`?**  
The caller must know whether registration succeeded; a `void` return would
silently drop the registration when the table is full, which could cause
hard-to-debug missing-callback bugs in production firmware.
