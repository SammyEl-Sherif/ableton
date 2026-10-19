#include <animalaudio/testing/NativeWindowSupport.h>

#include <cstdlib>
#include <cstring>

#if defined(__linux__)
#include <X11/Xlib.h>

#include <initializer_list>
#elif defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace animalaudio::testing
{
bool prepareNativeWindows()
{
    // These tests move the real OS pointer, which hijacks the mouse of whoever is working on the machine.
    const char* optIn = std::getenv("AA_REAL_MOUSE_TESTS");
    if (optIn == nullptr || std::strcmp(optIn, "1") != 0)
        return false;

#if defined(__linux__)
    // Kept open on purpose: a bare X server forgets every atom when its last client disconnects.
    static Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
        return false;

    // A bare X server (Xvfb without a window manager) has never seen the ICCCM atoms; JUCE looks them up
    // without creating them and then passes atom 0 to XChangeProperty, which aborts the process.
    for (const char* name : {"WM_PROTOCOLS",
                             "WM_DELETE_WINDOW",
                             "WM_TAKE_FOCUS",
                             "_NET_WM_PING",
                             "WM_CHANGE_STATE",
                             "WM_STATE",
                             "_NET_WM_PID",
                             "_NET_WM_WINDOW_TYPE",
                             "_NET_WM_STATE",
                             "_NET_WM_STATE_HIDDEN",
                             "_NET_WM_STATE_MAXIMIZED_HORZ",
                             "_NET_WM_STATE_MAXIMIZED_VERT",
                             "_NET_WM_STATE_FULLSCREEN",
                             "_NET_FRAME_EXTENTS",
                             "_NET_WORKAREA",
                             "_NET_WM_MOVERESIZE",
                             "_MOTIF_WM_HINTS",
                             "_NET_WM_ALLOWED_ACTIONS",
                             "_NET_WM_WINDOW_TYPE_NORMAL",
                             "_NET_WM_WINDOW_TYPE_COMBO"})
        XInternAtom(display, name, False);

    XSync(display, False);
#elif defined(__APPLE__)
    // JUCE's macOS peers only take events where the window server says their window is topmost; with no GUI
    // session (ssh) or a locked screen that is never true, and every injected click would be dropped.
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (session == nullptr)
        return false;
    const auto locked = CFDictionaryGetValue(session, CFSTR("CGSSessionScreenIsLocked"));
    const bool isLocked = locked != nullptr && CFGetTypeID(locked) == CFBooleanGetTypeID() &&
                          CFBooleanGetValue(static_cast<CFBooleanRef>(locked));
    CFRelease(session);
    if (isLocked)
        return false;
#endif
    return true;
}

} // namespace animalaudio::testing
