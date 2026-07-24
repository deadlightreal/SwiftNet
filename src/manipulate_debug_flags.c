#include "internal/internal.h"
#include "swift_net.h"

#ifdef SWIFT_NET_DEBUG
void swiftnet_add_debug_flags(const SwiftNetDebugFlags flags) {
  debugger.flags |= flags;
}
void swiftnet_remove_debug_flags(const SwiftNetDebugFlags flags) {
  debugger.flags &= ~flags;
}
#endif
