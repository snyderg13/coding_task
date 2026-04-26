#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>

#define START_BYTE          0xAAu
#define MAX_PAYLOAD_LEN     64u
#define MAX_HANDLERS        8u

#define PACKET_TYPE_TEMPERATURE  0x01u
#define PACKET_TYPE_HUMIDITY     0x02u

typedef void (*packet_cb_t)(uint8_t type, const uint8_t *payload, uint8_t len);

void parser_init(void);
int  parser_register_handler(uint8_t type, packet_cb_t cb);
void parser_feed(uint8_t byte);

#endif /* PARSER_H */
