# Firmware Engineer Coding Task

A lightweight, state-machine-based byte-stream packet parser written in C11.

## Packet Format

| Byte(s)    | Field    | Description                              |
|------------|----------|------------------------------------------|
| 0          | START    | Fixed start byte `0xAA`                  |
| 1          | TYPE     | Packet type (e.g. `0x01` = temperature)  |
| 2          | LEN      | Payload length in bytes (max 64)         |
| 3..N       | PAYLOAD  | `LEN` bytes of payload data              |
| N+1        | CHECKSUM | XOR of TYPE, LEN, and all payload bytes  |

## API

```c
void parser_init(packet_cb_t callback);
void parser_feed(uint8_t byte);
```

Call `parser_init` once with a callback to receive decoded packets. Feed incoming bytes one at a time via `parser_feed`. The callback fires only when a complete, valid packet is received.

```c
typedef void (*packet_cb_t)(uint8_t type, const uint8_t *payload, uint8_t len);
```

## Packet Types

| Type   | Name        | Payload              |
|--------|-------------|----------------------|
| `0x01` | Temperature | 2-byte signed int16, little-endian (units: 0.1 °C) |

## Building & Testing

```sh
make run
```

Builds with GCC (`-Wall -Wextra -Werror -std=c11`) and AddressSanitizer + UBSan enabled.

```
=== Packet Parser — Test Suite ===

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
  [PASS] test_null_callback_safe

  13 / 13 passed
```
