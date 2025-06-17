#include "internal.h"
#include "stdlib.h"
#include "stdio.h"
#include <string.h>
#include <unistd.h>

const uint32_t get_mtu(const char* const restrict interface) {
    char command[256];

    snprintf(command, sizeof(command), "ifconfig %s | grep mtu | awk '{print $4}'", interface);

    FILE* const restrict fp = popen(command, "r");
    if (unlikely(fp == NULL)) {
        fprintf(stderr, "failed to run command\n");
        return 0;
    }

    char size[64];
   
    fread(size, 1, sizeof(size), fp);

    pclose(fp);

    return (uint32_t)atoi(size);
}

