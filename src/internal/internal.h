#pragma once

#define MIN(one, two) (one > two ? two : one)

int GetDefaultInterface(char* interface_name);
unsigned int GetMtu(const char* interface);
