#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "parser.h"

/* -------------------------------------------------------------------------
 * Minimal test framework
 * ------------------------------------------------------------------------- */

static int tests_run     = 0;
static int tests_passed  = 0;
static int checks_failed = 0;

static void test_begin(void) {
    checks_failed = 0;
}

static void test_end(const char *name) {
    tests_run++;
    if (checks_failed == 0) {
        tests_passed++;
        printf("  [PASS] %s\n", name);
    } else {
        printf("  [FAIL] %s  (%d check(s) failed)\n", name, checks_failed);
    }
}

#define RUN(fn) do { test_begin(); fn(); test_end(#fn); } while (0)

#define CHECK(cond, msg) do {                           \
    if (!(cond)) {                                      \
        printf("         >> %s\n", (msg));              \
        checks_failed++;                                \
    }                                                   \
} while (0)

/* -------------------------------------------------------------------------
 * Callback capture — primary (used for temperature in multi-type tests)
 * ------------------------------------------------------------------------- */

static struct {
    int     call_count;
    uint8_t last_type;
    uint8_t last_payload[MAX_PAYLOAD_LEN];
    uint8_t last_len;
} cb_state;

static void reset_cb(void) {
    memset(&cb_state, 0, sizeof(cb_state));
}

static void capture_cb(uint8_t type, const uint8_t *payload, uint8_t len) {
    cb_state.call_count++;
    cb_state.last_type = type;
    cb_state.last_len  = len;
    memcpy(cb_state.last_payload, payload, len);
}

/* Secondary capture state — used for humidity in multi-type dispatch tests */

static struct {
    int     call_count;
    uint8_t last_type;
    uint8_t last_payload[MAX_PAYLOAD_LEN];
    uint8_t last_len;
} cb_state2;

static void reset_cb2(void) {
    memset(&cb_state2, 0, sizeof(cb_state2));
}

static void capture_cb2(uint8_t type, const uint8_t *payload, uint8_t len) {
    cb_state2.call_count++;
    cb_state2.last_type = type;
    cb_state2.last_len  = len;
    memcpy(cb_state2.last_payload, payload, len);
}

/* -------------------------------------------------------------------------
 * Packet construction helpers
 * ------------------------------------------------------------------------- */

/* checksum = XOR of TYPE, LEN, and all payload bytes */
static uint8_t compute_checksum(uint8_t type, uint8_t len, const uint8_t *payload) {
    uint8_t cs = type ^ len;
    for (uint8_t i = 0; i < len; i++) {
        cs ^= payload[i];
    }
    return cs;
}

/* Writes a complete framed packet into buf; returns total byte count. */
static size_t make_packet(uint8_t *buf, uint8_t type,
                          const uint8_t *payload, uint8_t plen) {
    buf[0] = START_BYTE;
    buf[1] = type;
    buf[2] = plen;
    memcpy(buf + 3, payload, plen);
    buf[3 + plen] = compute_checksum(type, plen, payload);
    return (size_t)(4 + plen);
}

/* TYPE=0x01 temperature packet (int16_t, LE, units: 0.1 °C). */
static size_t make_temp_packet(uint8_t *buf, int16_t temp) {
    uint8_t payload[2];
    uint16_t raw = (uint16_t)temp;
    payload[0] = (uint8_t)(raw & 0xFFu);
    payload[1] = (uint8_t)(raw >> 8);
    return make_packet(buf, PACKET_TYPE_TEMPERATURE, payload, 2);
}

/* TYPE=0x02 humidity packet (uint16_t, LE, units: 0.1% RH). */
static size_t make_humidity_packet(uint8_t *buf, uint16_t rh) {
    uint8_t payload[2];
    payload[0] = (uint8_t)(rh & 0xFFu);
    payload[1] = (uint8_t)(rh >> 8);
    return make_packet(buf, PACKET_TYPE_HUMIDITY, payload, 2);
}

/* Decodes a 2-byte LE int16_t from a payload buffer. */
static int16_t decode_temp(const uint8_t *payload) {
    return (int16_t)((uint16_t)payload[0] | ((uint16_t)payload[1] << 8));
}

/* Decodes a 2-byte LE uint16_t from a payload buffer. */
static uint16_t decode_humidity(const uint8_t *payload) {
    return (uint16_t)((uint16_t)payload[0] | ((uint16_t)payload[1] << 8));
}

static void feed(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        parser_feed(data[i]);
    }
}

/* -------------------------------------------------------------------------
 * Stage 1 tests
 * ------------------------------------------------------------------------- */

static void test_valid_temp_positive(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, 250);   /* 250 = 25.0 °C */
    feed(pkt, n);

    CHECK(cb_state.call_count == 1,        "callback called exactly once");
    CHECK(cb_state.last_type  == 0x01,     "type field is 0x01");
    CHECK(cb_state.last_len   == 2,        "payload length is 2");
    CHECK(decode_temp(cb_state.last_payload) == 250, "decoded value is 250");
}

static void test_valid_temp_negative(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, -100);  /* -100 = -10.0 °C */
    feed(pkt, n);

    CHECK(cb_state.call_count == 1, "callback called exactly once");
    CHECK(decode_temp(cb_state.last_payload) == -100, "decoded value is -100");
}

static void test_valid_temp_zero(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, 0);
    feed(pkt, n);

    CHECK(cb_state.call_count == 1, "callback called exactly once");
    CHECK(decode_temp(cb_state.last_payload) == 0, "decoded value is 0");
}

static void test_bad_checksum_no_callback(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, 250);
    pkt[n - 1] ^= 0xFFu;                    /* corrupt checksum */
    feed(pkt, n);

    CHECK(cb_state.call_count == 0, "callback not called on bad checksum");
}

static void test_corrupted_payload_byte(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, 250);
    pkt[3] ^= 0x01u;                         /* flip a bit in payload, checksum now wrong */
    feed(pkt, n);

    CHECK(cb_state.call_count == 0, "callback not called for corrupted payload");
}

static void test_noise_before_valid_packet(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t noise[] = { 0x00, 0x11, 0x22, 0x55, 0x77, 0xBB, 0xDE, 0xAD };
    feed(noise, sizeof(noise));
    CHECK(cb_state.call_count == 0, "no callback during noise");

    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, 500);
    feed(pkt, n);

    CHECK(cb_state.call_count == 1,   "callback fires after valid packet");
    CHECK(decode_temp(cb_state.last_payload) == 500, "value is correct");
}

static void test_two_sequential_packets(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t pkt[64];
    size_t n;

    n = make_temp_packet(pkt, 100);
    feed(pkt, n);

    n = make_temp_packet(pkt, 200);
    feed(pkt, n);

    CHECK(cb_state.call_count == 2, "callback fired twice");
    CHECK(decode_temp(cb_state.last_payload) == 200, "last value is 200");
}

static void test_recovery_after_bad_checksum(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t pkt[64];
    size_t n;

    n = make_temp_packet(pkt, 999);
    pkt[n - 1] ^= 0xFFu;                    /* corrupt */
    feed(pkt, n);
    CHECK(cb_state.call_count == 0, "bad packet fires no callback");

    n = make_temp_packet(pkt, 42);
    feed(pkt, n);
    CHECK(cb_state.call_count == 1,  "parser recovered, good packet accepted");
    CHECK(decode_temp(cb_state.last_payload) == 42, "good packet value correct");
}

static void test_oversized_length_resets_parser(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    /* Manually craft a packet with LEN > MAX_PAYLOAD_LEN */
    uint8_t bad[] = { START_BYTE, 0x01, MAX_PAYLOAD_LEN + 1, 0x00, 0x00 };
    feed(bad, sizeof(bad));
    CHECK(cb_state.call_count == 0, "no callback for oversized LEN");

    /* Parser should now be reset and accept the next valid packet */
    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, 77);
    feed(pkt, n);
    CHECK(cb_state.call_count == 1, "parser recovered after oversized LEN");
}

static void test_partial_packet_no_callback(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, 123);
    feed(pkt, n - 1);                        /* omit checksum byte */

    CHECK(cb_state.call_count == 0, "no callback for incomplete packet");
}

/* 0xAA is START_BYTE; it must be legal as a payload data value. */
static void test_start_byte_value_in_payload(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    /* Temperature 0x00AA: payload bytes are [0xAA, 0x00] */
    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, (int16_t)0x00AA);
    feed(pkt, n);

    CHECK(cb_state.call_count == 1,               "callback fires exactly once");
    CHECK(cb_state.last_payload[0] == 0xAA,       "0xAA preserved in payload[0]");
    CHECK(cb_state.last_payload[1] == 0x00,       "payload[1] is 0x00");
}

static void test_noise_between_packets(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    uint8_t pkt[64];
    size_t n;

    n = make_temp_packet(pkt, 300);
    feed(pkt, n);

    uint8_t noise[] = { 0x00, 0x01, 0xFE, 0xFF };
    feed(noise, sizeof(noise));

    n = make_temp_packet(pkt, 400);
    feed(pkt, n);

    CHECK(cb_state.call_count == 2, "both packets received");
    CHECK(decode_temp(cb_state.last_payload) == 400, "second packet value correct");
}

/* The parser must not crash or fire when no handler is registered. */
static void test_no_handler_safe(void) {
    parser_init();   /* no handlers registered */
    reset_cb();

    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, 250);
    feed(pkt, n);

    CHECK(cb_state.call_count == 0, "no callback fires with no handler registered");
}

/* -------------------------------------------------------------------------
 * Stage 2 tests
 * ------------------------------------------------------------------------- */

static void test_humidity_valid(void) {
    reset_cb2();
    parser_init();
    parser_register_handler(PACKET_TYPE_HUMIDITY, capture_cb2);

    uint8_t pkt[64];
    size_t n = make_humidity_packet(pkt, 555);  /* 555 = 55.5% RH */
    feed(pkt, n);

    CHECK(cb_state2.call_count == 1,              "humidity callback called once");
    CHECK(cb_state2.last_type  == 0x02,           "type field is 0x02");
    CHECK(cb_state2.last_len   == 2,              "payload length is 2");
    CHECK(decode_humidity(cb_state2.last_payload) == 555, "decoded humidity is 555");
}

static void test_dispatch_to_correct_handler(void) {
    reset_cb();
    reset_cb2();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);
    parser_register_handler(PACKET_TYPE_HUMIDITY,    capture_cb2);

    uint8_t pkt[64];
    size_t n;

    n = make_temp_packet(pkt, 220);      /* 22.0 °C */
    feed(pkt, n);

    n = make_humidity_packet(pkt, 600);  /* 60.0% RH */
    feed(pkt, n);

    CHECK(cb_state.call_count  == 1, "temperature handler called once");
    CHECK(cb_state2.call_count == 1, "humidity handler called once");
    CHECK(decode_temp(cb_state.last_payload)      == 220, "temperature value correct");
    CHECK(decode_humidity(cb_state2.last_payload) == 600, "humidity value correct");
}

static void test_unregistered_type_ignored(void) {
    reset_cb();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);

    /* Send a humidity packet — no handler registered for 0x02 */
    uint8_t pkt[64];
    size_t n = make_humidity_packet(pkt, 500);
    feed(pkt, n);

    CHECK(cb_state.call_count == 0, "no callback for unregistered packet type");
}

static void test_handler_overwrite(void) {
    reset_cb();
    reset_cb2();
    parser_init();

    /* Register capture_cb, then overwrite with capture_cb2 for the same type */
    int r1 = parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);
    int r2 = parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb2);

    CHECK(r1 == 0, "first registration succeeds");
    CHECK(r2 == 0, "overwrite registration succeeds");

    uint8_t pkt[64];
    size_t n = make_temp_packet(pkt, 111);
    feed(pkt, n);

    CHECK(cb_state.call_count  == 0, "original handler not called after overwrite");
    CHECK(cb_state2.call_count == 1, "new handler called after overwrite");
}

static void test_handler_table_full(void) {
    parser_init();

    /* Fill the table to capacity with distinct types */
    for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
        int r = parser_register_handler((uint8_t)(i + 1u), capture_cb);
        CHECK(r == 0, "registration succeeds while table has space");
    }

    /* One more new type must fail */
    int r = parser_register_handler((uint8_t)(MAX_HANDLERS + 1u), capture_cb);
    CHECK(r == -1, "registration fails when table is full");

    /* Overwriting an already-registered type must still succeed */
    int r2 = parser_register_handler(0x01u, capture_cb);
    CHECK(r2 == 0, "overwrite of existing type succeeds even when table is full");
}

static void test_mixed_type_sequence(void) {
    reset_cb();
    reset_cb2();
    parser_init();
    parser_register_handler(PACKET_TYPE_TEMPERATURE, capture_cb);
    parser_register_handler(PACKET_TYPE_HUMIDITY,    capture_cb2);

    uint8_t pkt[64];
    size_t n;

    /* Interleave temperature and humidity packets */
    n = make_temp_packet(pkt,     150);  feed(pkt, n);
    n = make_humidity_packet(pkt, 400);  feed(pkt, n);
    n = make_temp_packet(pkt,     160);  feed(pkt, n);
    n = make_humidity_packet(pkt, 410);  feed(pkt, n);

    CHECK(cb_state.call_count  == 2, "temperature handler called twice");
    CHECK(cb_state2.call_count == 2, "humidity handler called twice");
    CHECK(decode_temp(cb_state.last_payload)      == 160, "last temperature correct");
    CHECK(decode_humidity(cb_state2.last_payload) == 410, "last humidity correct");
}

/* -------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Packet Parser — Test Suite ===\n\n");

    printf("-- Stage 1 --\n");
    RUN(test_valid_temp_positive);
    RUN(test_valid_temp_negative);
    RUN(test_valid_temp_zero);
    RUN(test_bad_checksum_no_callback);
    RUN(test_corrupted_payload_byte);
    RUN(test_noise_before_valid_packet);
    RUN(test_two_sequential_packets);
    RUN(test_recovery_after_bad_checksum);
    RUN(test_oversized_length_resets_parser);
    RUN(test_partial_packet_no_callback);
    RUN(test_start_byte_value_in_payload);
    RUN(test_noise_between_packets);
    RUN(test_no_handler_safe);

    printf("\n-- Stage 2 --\n");
    RUN(test_humidity_valid);
    RUN(test_dispatch_to_correct_handler);
    RUN(test_unregistered_type_ignored);
    RUN(test_handler_overwrite);
    RUN(test_handler_table_full);
    RUN(test_mixed_type_sequence);

    printf("\n  %d / %d passed\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
