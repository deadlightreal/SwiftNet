#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include "../swift_net.h"

#define MIN(one, two) (one > two ? two : one)

int GetDefaultInterface(char* restrict interface_name);
unsigned int GetMtu(const char* restrict interface);
void* process_packets(void* void_connection);

typedef struct PacketQueueNode PacketQueueNode;

struct PacketQueueNode {
    PacketQueueNode* next;
    uint8_t* data; 
    struct sockaddr_in sender_address;
    socklen_t sender_address_len;
};

typedef struct {
    PacketQueueNode* volatile first_node;
    PacketQueueNode* volatile last_node;
} PacketQueue;

extern PacketQueue packet_queue;
