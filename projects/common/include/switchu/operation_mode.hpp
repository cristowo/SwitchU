#pragma once

#include <switch.h>

namespace switchu {

// Asks AM directly: appletGetOperationMode() is a value libnx only refreshes
// from appletMainLoop, which neither the daemon nor the menu run.
inline bool consoleIsDocked() {
    u8 mode = AppletOperationMode_Handheld;
    Service* session = appletGetServiceSession_CommonStateGetter();
    if (!session || R_FAILED(serviceDispatchOut(session, 5, mode))) // GetOperationMode
        return false;
    return mode == AppletOperationMode_Console;
}

} // namespace switchu
