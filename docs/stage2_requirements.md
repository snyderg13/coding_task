# Stage 2 Task: Per-Type Handler Dispatch

## Background

You are extending the packet parser built in Stage 1. The parser already handles a
single packet type (temperature, `0x01`) and delivers every valid packet to one
shared callback registered at init time.

In a real firmware product the device streams multiple sensor readings over the same
UART. Each reading has its own packet type and its own consumer in application code.
Routing all packets through a single callback forces the application to `switch` on
type — the parser should own that dispatch instead.

## Your Task

Extend the parser to support **per-type handler registration** and add a second
sensor packet type.

---

## API Changes

### `parser_init`

Change the signature from:
```c
void parser_init(packet_cb_t callback);
```
to:
```c
void parser_init(void);
```

`parser_init` must reset all parser state **and** clear the handler table.

---

### `parser_register_handler` (new)

```c
int parser_register_handler(uint8_t type, packet_cb_t cb);
```

| Return value | Meaning |
|---|---|
| `0` | Handler registered (or updated) successfully |
| `-1` | Table is full and `type` is not already registered |

Rules:
- The table holds at most `MAX_HANDLERS` (8) distinct types.
- Registering the same type a second time **replaces** the existing handler
  (returns `0`; does not consume a new slot).
- Registering a new type when the table is already full returns `-1` and leaves
  the table unchanged.

---

## New Packet Type: Humidity (`0x02`)

| Field | Value |
|---|---|
| Type byte | `0x02` |
| Payload size | 2 bytes |
| Encoding | Unsigned 16-bit, little-endian |
| Units | 0.1 % relative humidity |
| Example | `0x022B` (555) → 55.5 % RH |

Add `PACKET_TYPE_HUMIDITY 0x02u` to `parser.h`.

---

## Behavioral Requirements

| ID | Requirement |
|---|---|
| REQ-S2-01 | `parser_init()` clears all registered handlers |
| REQ-S2-02 | A packet whose type has no registered handler is silently discarded |
| REQ-S2-03 | The handler table supports up to `MAX_HANDLERS` distinct types |
| REQ-S2-04 | `parser_register_handler` returns `-1` when the table is full and the type is new |
| REQ-S2-05 | Re-registering an existing type succeeds (returns `0`) even when the table is full |
| REQ-S2-06 | All Stage 1 behavioral requirements remain satisfied |

---

## Testing Requirements

- Update every existing Stage 1 test to use `parser_init()` +
  `parser_register_handler()` in place of `parser_init(callback)`.
- Add tests covering at minimum:
  - Successful humidity packet decode
  - Correct per-type dispatch when both handlers are registered
  - No callback for an unregistered type
  - Handler overwrite (second registration for same type)
  - Table-full rejection and overwrite-while-full success

---

## Evaluation Criteria

| Area | What to look for |
|---|---|
| Dispatch mechanism | Table-based lookup, not a `switch` on type |
| Boundary handling | `MAX_HANDLERS` enforced; overwrite path correct |
| API design | Clean separation between init, registration, and feed |
| Regression safety | All Stage 1 tests still pass with new API |
| Test quality | New tests are specific, minimal, and independently named |
