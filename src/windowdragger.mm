#include "windowdragger.h"

#ifdef Q_OS_MACOS
#import <AppKit/AppKit.h>
#endif

void WindowDragger::startDrag(QQuickWindow *window)
{
#ifdef Q_OS_MACOS
    NSView *nsView = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    NSWindow *nsWindow = [nsView window];
    NSEvent *currentEvent = [nsWindow currentEvent];
    if (currentEvent) {
        [nsWindow performWindowDragWithEvent:currentEvent];
    }
#else
    window->startSystemMove();
#endif
}
