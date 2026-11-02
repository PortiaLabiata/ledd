#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef enum {
    COMMAND_ON,
    COMMAND_OFF,
    COMMAND_BLINK,
} Command;

typedef union {
    struct __attribute__((packed)) Value {
        unsigned char command;  
        uint32_t argument;
    } value;

    uint8_t buffer[sizeof(struct Value)];
} Datagram;

#define DGRAM_SIZE sizeof(Datagram)
#define SOCK_NAME "/run/ledd.sock"

#endif
