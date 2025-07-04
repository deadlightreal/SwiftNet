#include "swift_net.h"
#include "internal/internal.h"

void swiftnet_add_debug_flags(const uint32_t flags) {
    debugger.flags |= flags;
}
