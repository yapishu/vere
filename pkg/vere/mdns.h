#include "noun.h"

typedef void mdns_cb(c3_c* ship, c3_w s_addr, c3_s port, void* context);

void mdns_init(uint16_t port, char* our, mdns_cb* cb, void* context);
