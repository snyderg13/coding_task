# Firmware Engineer Coding Task

A lightweight, state-machine-based byte-stream packet parser written in C11.

## Time Estimate

45 – 60 minutes

---

## Your Task

This task has two stages. **Stage 1 is fully implemented** as your starting point — read through it before writing any code. Your job is to complete **Stage 2**.

Full requirements: [`docs/stage2_requirements.md`](docs/stage2_requirements.md)

---

## Packet Format

Every packet follows this fixed framing:

| Byte(s) | Field    | Description                              |
|---------|----------|------------------------------------------|
| 0       | START    | Fixed start byte `0xAA`                  |
| 1       | TYPE     | Packet type identifier                   |
| 2       | LEN      | Payload length in bytes (max 64)         |
| 3..N    | PAYLOAD  | `LEN` bytes of payload data              |
| N+1     | CHECKSUM | XOR of TYPE, LEN, and all payload bytes  |

## Packet Types

| Type   | Name        | Payload                                                         |
|--------|-------------|-----------------------------------------------------------------|
| `0x01` | Temperature | 2-byte signed `int16`, little-endian (units: 0.1 °C)           |
| `0x02` | Humidity    | 2-byte unsigned `uint16`, little-endian (units: 0.1 % RH) — **added in Stage 2** |

---

## Stage 1 — Provided

`parser.c` and `parser.h` implement a single-type packet parser. The core is a
5-state machine that consumes a raw byte stream and frames packets:

```
WAIT_START → READ_TYPE → READ_LEN → READ_PAYLOAD → READ_CHECKSUM
```

On a valid checksum, a single shared callback fires with the packet's type,
payload pointer, and length. Any byte that does not fit the current state — a
bad checksum, an oversized length field, or noise between packets — resets
the state machine to `WAIT_START`.

For a detailed walkthrough of the existing code, see
[`docs/stage1_overview.md`](docs/stage1_overview.md).

### Current API (Stage 1)

```c
#define START_BYTE          0xAAu
#define MAX_PAYLOAD_LEN     64u

#define PACKET_TYPE_TEMPERATURE  0x01u

typedef void (*packet_cb_t)(uint8_t type, const uint8_t *payload, uint8_t len);

void parser_init(packet_cb_t callback);
void parser_feed(uint8_t byte);
```

- **`parser_init(callback)`** — resets all parser state and registers a single
  callback that will fire for every valid packet, regardless of type.
- **`parser_feed(byte)`** — processes one byte of the incoming stream. Call
  repeatedly as bytes arrive (e.g. from a UART receive interrupt).

---

## Stage 2 — Your Task

The device streams multiple sensor readings over the same UART. Each reading has
its own type and its own consumer in application code. Routing all packets
through a single callback forces the application to `switch` on type — the
parser should own that dispatch instead.

Your task is to extend the parser with a **per-type handler table** and add
support for a second packet type (`0x02` — humidity).

### API Changes

Replace `parser_init(callback)` with:

```c
void parser_init(void);
int  parser_register_handler(uint8_t type, packet_cb_t cb);
```

See [`docs/stage2_requirements.md`](docs/stage2_requirements.md) for the
complete specification, behavioral requirements, and testing checklist.

---

## Building & Testing

```sh
make run
```

Builds with GCC (`-Wall -Wextra -Werror -std=c11`) and AddressSanitizer +
UBSan enabled, then runs the test suite.

### Starting state (Stage 1 passing)

```
=== Packet Parser — Test Suite ===

-- Stage 1 --
  [PASS] test_valid_temp_positive
  [PASS] test_valid_temp_negative
  [PASS] test_valid_temp_zero
  [PASS] test_bad_checksum_no_callback
  [PASS] test_corrupted_payload_byte
  [PASS] test_noise_before_valid_packet
  [PASS] test_two_sequential_packets
  [PASS] test_recovery_after_bad_checksum
  [PASS] test_oversized_length_resets_parser
  [PASS] test_partial_packet_no_callback
  [PASS] test_start_byte_value_in_payload
  [PASS] test_noise_between_packets
  [PASS] test_no_handler_safe

-- Stage 2 --
  (not yet implemented)

  13 / 13 passed
```

### Goal (Stage 2 complete)

```
=== Packet Parser — Test Suite ===

-- Stage 1 --
  [PASS] ...  (13 tests)

-- Stage 2 --
  [PASS] test_humidity_valid
  [PASS] test_dispatch_to_correct_handler
  [PASS] test_unregistered_type_ignored
  [PASS] test_handler_overwrite
  [PASS] test_handler_table_full
  [PASS] test_mixed_type_sequence

  19 / 19 passed
```

---

## Reference Documents

| Document | Audience | Description |
|----------|----------|-------------|
| [`docs/stage1_overview.md`](docs/stage1_overview.md) | Candidate | Walkthrough of the Stage 1 implementation |
| [`docs/stage2_requirements.md`](docs/stage2_requirements.md) | Candidate | Stage 2 task specification |
