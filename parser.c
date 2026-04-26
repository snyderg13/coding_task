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
    parser_state_t state;
    packet_cb_t    callback;
    uint8_t        type;
    uint8_t        len;
    uint8_t        payload[MAX_PAYLOAD_LEN];
    uint8_t        payload_idx;
    uint8_t        checksum;  /* running XOR of TYPE, LEN, and all payload bytes */
} ctx;

static void reset(void) {
    ctx.state = STATE_WAIT_START;
}

void parser_init(packet_cb_t callback) {
    memset(&ctx, 0, sizeof(ctx));
    ctx.callback = callback;
    ctx.state    = STATE_WAIT_START;
}

void parser_feed(uint8_t byte) {
    switch (ctx.state) {

        case STATE_WAIT_START:
            if (byte == START_BYTE) {
                ctx.state = STATE_READ_TYPE;
            }
            break;

        case STATE_READ_TYPE:
            ctx.type     = byte;
            ctx.checksum = byte;   /* begin accumulating checksum */
            ctx.state    = STATE_READ_LEN;
            break;

        case STATE_READ_LEN:
            if (byte > MAX_PAYLOAD_LEN) {
                reset();
                break;
            }
            ctx.len         = byte;
            ctx.checksum   ^= byte;
            ctx.payload_idx = 0;
            ctx.state       = (byte > 0) ? STATE_READ_PAYLOAD : STATE_READ_CHECKSUM;
            break;

        case STATE_READ_PAYLOAD:
            ctx.payload[ctx.payload_idx++]  = byte;
            ctx.checksum                   ^= byte;
            if (ctx.payload_idx == ctx.len) {
                ctx.state = STATE_READ_CHECKSUM;
            }
            break;

        case STATE_READ_CHECKSUM:
            if (byte == ctx.checksum && ctx.callback != NULL) {
                ctx.callback(ctx.type, ctx.payload, ctx.len);
            }
            reset();
            break;
    }
}
