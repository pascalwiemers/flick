#include "windowdragger.h"

void WindowDragger::startDrag(QQuickWindow *window)
{
    window->startSystemMove();
}

#include "moc_windowdragger.cpp"
