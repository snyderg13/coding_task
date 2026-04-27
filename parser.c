#include "parser.h"
#include <string.h>

typedef enum {
    STATE_WAIT_START,
    STATE_READ_TYPE,
    STATE_READ_LEN,
    STATE_READ_PAYLOAD,
    STATE_READ_CHECKSUM,
} parser_state_t;

static struct {
    parser_state_t  state;
    uint8_t         type;
    uint8_t         len;
    uint8_t         payload[MAX_PAYLOAD_LEN];
    uint8_t         payload_idx;
    uint8_t         checksum;   /* running XOR of TYPE, LEN, and all payload bytes */
    packet_cb_t     callback;
} ctx;

static void reset(void) {
    ctx.state = STATE_WAIT_START;
}

void parser_init(packet_cb_t callback) {
    /* TODO: zero ctx, store callback */
    reset();
    (void)callback;
}

void parser_feed(uint8_t byte) {
    /* TODO */
    (void)byte;
}
