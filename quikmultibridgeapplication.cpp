#include "quikmultibridgeapplication.h"
#include <QTimer>

QuikMultiBridgeApplication::QuikMultiBridgeApplication(BridgePlugin **pp, QObject *parent)
    : QObject(parent), pluginPointer(pp)
{
    QTimer::singleShot(0, this, SLOT(appProcessEvents()));
}

void QuikMultiBridgeApplication::appProcessEvents()
{
    if((*pluginPointer))
        (*pluginPointer)->processEventsInBridge();
    QTimer::singleShot(0, this, SLOT(appProcessEvents()));
}
