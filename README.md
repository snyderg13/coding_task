# Firmware Engineer Coding Task

A lightweight, state-machine-based byte-stream packet parser written in C11.

## Packet Format

| Byte(s) | Field    | Description                             |
|---------|----------|-----------------------------------------|
| 0       | START    | Fixed start byte `0xAA`                 |
| 1       | TYPE     | Packet type identifier                  |
| 2       | LEN      | Payload length in bytes (max 64)        |
| 3..N    | PAYLOAD  | `LEN` bytes of payload data             |
| N+1     | CHECKSUM | XOR of TYPE, LEN, and all payload bytes |

## API

```c
void parser_init(void);
int  parser_register_handler(uint8_t type, packet_cb_t cb);
void parser_feed(uint8_t byte);
```

Call `parser_init` to reset all parser state and clear the handler table. Register
a callback for each packet type of interest via `parser_register_handler`, then feed
incoming bytes one at a time through `parser_feed`. The registered callback fires
only when a complete, valid packet of the matching type is received.

```c
typedef void (*packet_cb_t)(uint8_t type, const uint8_t *payload, uint8_t len);
```

### `parser_register_handler`

| Return | Meaning |
|--------|---------|
| `0`    | Handler registered (or updated) successfully |
| `-1`   | Handler table full; type not already registered |

Up to `MAX_HANDLERS` (8) distinct types may be registered. Re-registering an
existing type replaces its handler and returns `0` regardless of table capacity.

## Packet Types

| Type   | Name        | Payload                                            |
|--------|-------------|----------------------------------------------------|
| `0x01` | Temperature | 2-byte signed int16, little-endian (units: 0.1 °C) |
| `0x02` | Humidity    | 2-byte unsigned uint16, little-endian (units: 0.1 % RH) |

## Building & Testing

```sh
make run
```

Builds with GCC (`-Wall -Wextra -Werror -std=c11`) and AddressSanitizer + UBSan enabled.

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
  [PASS] test_humidity_valid
  [PASS] test_dispatch_to_correct_handler
  [PASS] test_unregistered_type_ignored
  [PASS] test_handler_overwrite
  [PASS] test_handler_table_full
  [PASS] test_mixed_type_sequence

  19 / 19 passed
```

## Interview Task Docs

| Document | Description |
|----------|-------------|
| [`docs/stage2_requirements.md`](docs/stage2_requirements.md) | Candidate-facing task sheet for Stage 2 |
| [`docs/stage2_spec.md`](docs/stage2_spec.md) | Internal technical specification and design rationale |
