# Stage 1 Task: Byte-Stream Packet Parser

## Background

You are implementing a packet parser for a sensor device that transmits framed
binary packets over UART. The byte stream is unreliable — bytes can arrive with
noise, partial packets, or corruption. Your parser must extract each complete,
valid packet from the raw stream and deliver it to application code via a
callback.

---

## Your Task

Implement `parser_init` and `parser_feed` in `parser.c`.

The state machine skeleton and internal data structures are already provided.
You do not need to add any new types or fields — only the function bodies.

---

## API

```c
void parser_init(packet_cb_t callback);
void parser_feed(uint8_t byte);

typedef void (*packet_cb_t)(uint8_t type, const uint8_t *payload, uint8_t len);
```

### `parser_init(callback)`

Resets all parser state and registers `callback` as the receive handler.
Call once at startup (or after a fault reset) before feeding any bytes.

### `parser_feed(byte)`

Processes one byte of the incoming stream. Intended to be called once per
received byte — for example, directly from a UART receive interrupt or a
tight polling loop. It must do no dynamic allocation and return quickly.

When a complete, valid packet is assembled, `callback` fires **synchronously**
from within `parser_feed` with three arguments:
- `type` — the packet type byte
- `payload` — pointer to the internal payload buffer (valid only during the callback)
- `len` — number of payload bytes

---

## Packet Format

| Byte(s) | Field    | Description                              |
|---------|----------|------------------------------------------|
| 0       | START    | Fixed start byte `0xAA`                  |
| 1       | TYPE     | Packet type identifier                   |
| 2       | LEN      | Payload length in bytes (max 64)         |
| 3..N    | PAYLOAD  | `LEN` bytes of payload data              |
| N+1     | CHECKSUM | XOR of TYPE, LEN, and all payload bytes  |

**Checksum formula:**
```
checksum = TYPE ^ LEN ^ payload[0] ^ payload[1] ^ ... ^ payload[LEN-1]
```

---

## Packet Type: Temperature (`0x01`)

| Field   | Details |
|---------|---------|
| Constant | `PACKET_TYPE_TEMPERATURE` (`0x01`) |
| Payload | 2 bytes, signed `int16_t`, little-endian |
| Units   | 0.1 °C — e.g. `250` → 25.0 °C, `-100` → −10.0 °C |

Decoding example (for reference; done by the application, not the parser):
```c
int16_t temp = (int16_t)((uint16_t)payload[0] | ((uint16_t)payload[1] << 8));
```

---

## Behavioral Requirements

| ID | Requirement |
|----|-------------|
| S1-01 | A valid packet fires the callback exactly once |
| S1-02 | A packet with a bad checksum does not fire the callback |
| S1-03 | A corrupted payload byte invalidates the checksum (S1-02 applies) |
| S1-04 | Noise bytes before a valid packet are ignored; the valid packet still fires |
| S1-05 | Two sequential valid packets each fire the callback |
| S1-06 | The parser resets cleanly after a bad checksum and accepts the next valid packet |
| S1-07 | A `LEN` byte greater than `MAX_PAYLOAD_LEN` (64) resets the parser immediately |
| S1-08 | A partial packet (e.g. missing the checksum byte) does not fire the callback |
| S1-09 | `0xAA` (the start byte value) is legal as a payload data byte |
| S1-10 | Noise between two valid packets does not affect either delivery |
| S1-11 | Passing `NULL` as the callback is safe — a valid packet is silently discarded |

---

## Testing

```sh
make run
```

When Stage 1 is complete, all 13 tests should pass:

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

  13 / 13 passed
```

---

## Evaluation Criteria

| Area | What to look for |
|------|-----------------|
| Correctness | All 13 tests pass with no sanitizer errors |
| Robustness | Noise and bad checksums handled without crash or undefined behavior |
| Code clarity | State transitions are easy to follow; no unnecessary complexity |
| Edge cases | `LEN == 0`, `NULL` callback, `0xAA` in payload all handled correctly |
