# Stage 1: Single-Type Packet Parser

## What Stage 1 Provides

Stage 1 is a complete, working implementation of a byte-stream packet parser.
Read through `parser.c` and `parser.h` to understand the design before
starting Stage 2.

---

## State Machine

The parser runs a 5-state machine. Each call to `parser_feed(byte)` advances it
by one byte:

```
WAIT_START
    │  byte == 0xAA
    ▼
READ_TYPE
    │  any byte (saved as type; starts checksum)
    ▼
READ_LEN
    │  byte <= MAX_PAYLOAD_LEN (saved as len; XOR'd into checksum)
    │  byte > MAX_PAYLOAD_LEN → reset to WAIT_START
    ▼
READ_PAYLOAD  (skipped if len == 0)
    │  consumes len bytes (each XOR'd into checksum)
    ▼
READ_CHECKSUM
    │  byte == running checksum → fire callback, reset
    │  byte != running checksum → reset (no callback)
    ▼
WAIT_START
```

Any unexpected byte resets the machine to `WAIT_START`, so the parser
recovers automatically from noise, truncated packets, or a bad checksum.

---

## API

```c
void parser_init(packet_cb_t callback);
void parser_feed(uint8_t byte);

typedef void (*packet_cb_t)(uint8_t type, const uint8_t *payload, uint8_t len);
```

**`parser_init(callback)`**
Zeroes all internal state and stores `callback` as the single receive handler.
Call once at startup (or after a fault reset) before feeding any bytes.

**`parser_feed(byte)`**
Processes one byte. Safe to call from an ISR or a tight polling loop; it does
no allocation and has O(1) runtime. The callback fires synchronously from inside
`parser_feed` when a complete, valid packet is assembled.

**`packet_cb_t`**
The callback receives the packet type, a pointer to the internal payload buffer,
and the payload length. The buffer is only valid for the duration of the
callback — copy the data if you need it beyond that.

---

## Packet Type: Temperature (`0x01`)

| Field   | Details |
|---------|---------|
| Type    | `0x01` (`PACKET_TYPE_TEMPERATURE`) |
| Payload | 2 bytes, signed `int16_t`, little-endian |
| Units   | 0.1 °C (e.g. `250` → 25.0 °C, `-100` → −10.0 °C) |

Decoding example:
```c
int16_t temp = (int16_t)((uint16_t)payload[0] | ((uint16_t)payload[1] << 8));
```

---

## Checksum

```
checksum = TYPE ^ LEN ^ payload[0] ^ payload[1] ^ ... ^ payload[LEN-1]
```

The checksum byte follows immediately after the last payload byte. A mismatch
resets the parser without invoking the callback.

---

## Behavioral Requirements (Stage 1)

| ID | Requirement |
|----|-------------|
| S1-01 | A valid packet with a registered callback fires the callback exactly once |
| S1-02 | A packet with a bad checksum does not fire the callback |
| S1-03 | A corrupted payload byte invalidates the checksum (S1-02 applies) |
| S1-04 | Noise before a valid packet is ignored; the valid packet still fires |
| S1-05 | Two sequential valid packets fire the callback twice |
| S1-06 | The parser resets cleanly after a bad checksum and accepts the next valid packet |
| S1-07 | A `LEN` byte greater than `MAX_PAYLOAD_LEN` (64) resets the parser immediately |
| S1-08 | A partial packet (missing checksum byte) does not fire the callback |
| S1-09 | `0xAA` (the start byte) is legal as a payload data value |
| S1-10 | Noise between two valid packets does not affect either delivery |
| S1-11 | No callback registered → valid packet is silently discarded (no crash) |

---

## Stage 1 Test Coverage

| Test | Requirement(s) |
|------|----------------|
| `test_valid_temp_positive` | S1-01 |
| `test_valid_temp_negative` | S1-01 (negative encoding) |
| `test_valid_temp_zero` | S1-01 (zero value) |
| `test_bad_checksum_no_callback` | S1-02 |
| `test_corrupted_payload_byte` | S1-03 |
| `test_noise_before_valid_packet` | S1-04 |
| `test_two_sequential_packets` | S1-05 |
| `test_recovery_after_bad_checksum` | S1-06 |
| `test_oversized_length_resets_parser` | S1-07 |
| `test_partial_packet_no_callback` | S1-08 |
| `test_start_byte_value_in_payload` | S1-09 |
| `test_noise_between_packets` | S1-10 |
| `test_no_handler_safe` | S1-11 |
