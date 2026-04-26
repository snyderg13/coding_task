#include "parser.h"
#include <string.h>

typedef struct {
    uint8_t     type;
    packet_cb_t cb;
} handler_entry_t;

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
    uint8_t         checksum;  /* running XOR of TYPE, LEN, and all payload bytes */
} ctx;

static handler_entry_t handlers[MAX_HANDLERS];
static uint8_t         handler_count;

static void reset(void) {
    ctx.state = STATE_WAIT_START;
}

void parser_init(void) {
    memset(&ctx, 0, sizeof(ctx));
    memset(handlers, 0, sizeof(handlers));
    handler_count = 0;
    ctx.state = STATE_WAIT_START;
}

int parser_register_handler(uint8_t type, packet_cb_t cb) {
    for (uint8_t i = 0; i < handler_count; i++) {
        if (handlers[i].type == type) {
            handlers[i].cb = cb;
            return 0;
        }
    }
    if (handler_count >= MAX_HANDLERS) {
        return -1;
    }
    handlers[handler_count].type = type;
    handlers[handler_count].cb   = cb;
    handler_count++;
    return 0;
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
            if (byte == ctx.checksum) {
                for (uint8_t i = 0; i < handler_count; i++) {
                    if (handlers[i].type == ctx.type && handlers[i].cb != NULL) {
                        handlers[i].cb(ctx.type, ctx.payload, ctx.len);
                        break;
                    }
                }
            }
            reset();
            break;
    }
}
