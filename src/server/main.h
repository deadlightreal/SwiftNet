#pragma once

#include <stddef.h>
#include <stdint.h>

void (*SwiftNetMessageHandler)(uint8_t* data) = NULL;
unsigned int SwiftNetBufferSize = 1024;
