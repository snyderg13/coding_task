# Firmware Engineer Coding Task

A lightweight, state-machine-based byte-stream packet parser written in C11.

## Time Estimate

60 – 90 minutes

---

## Overview

This task is structured in two stages. Complete them in order — Stage 2 builds
directly on your Stage 1 implementation.

| Stage | Task | Tests |
|-------|------|-------|
| [Stage 1](docs/stage1_overview.md) | Implement the core byte-stream packet parser | 13 |
| [Stage 2](docs/stage2_requirements.md) | Extend it with per-type handler dispatch | +6 |

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

| Type   | Name        | Payload                                                            |
|--------|-------------|--------------------------------------------------------------------|
| `0x01` | Temperature | 2-byte signed `int16`, little-endian (units: 0.1 °C)              |
| `0x02` | Humidity    | 2-byte unsigned `uint16`, little-endian (units: 0.1 % RH) — Stage 2 |

---

## Stage 1 — Implement the Parser

See [`docs/stage1_overview.md`](docs/stage1_overview.md) for the full task
description, API contract, behavioral requirements, and test coverage.

### API to implement

```c
void parser_init(packet_cb_t callback);
void parser_feed(uint8_t byte);

typedef void (*packet_cb_t)(uint8_t type, const uint8_t *payload, uint8_t len);
```

The state machine skeleton and internal data structures are already in `parser.c`.
You only need to fill in the function bodies.

---

## Stage 2 — Per-Type Handler Dispatch

See [`docs/stage2_requirements.md`](docs/stage2_requirements.md) for the full
task description.

### API changes

Replace `parser_init(callback)` with:

```c
void parser_init(void);
int  parser_register_handler(uint8_t type, packet_cb_t cb);
```

---

## Building & Testing

```sh
make run
```

Builds with GCC (`-Wall -Wextra -Werror -std=c11`) and AddressSanitizer +
UBSan enabled, then runs the test suite.

### After Stage 1

```
=== Packet Parser — Test Suite ===

-- Stage 1 --
  [PASS] test_valid_temp_positive
  ...
  [PASS] test_no_handler_safe

  13 / 13 passed
```

### After Stage 2

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

| Document | Description |
|----------|-------------|
| [`docs/stage1_overview.md`](docs/stage1_overview.md) | Stage 1 task specification |
| [`docs/stage2_requirements.md`](docs/stage2_requirements.md) | Stage 2 task specification |
