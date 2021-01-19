#ifndef QUIKMULTIBRIDGE_H
#define QUIKMULTIBRIDGE_H

#include "QuikMultiBridge_global.h"
#include <lua.hpp>
#include "pythonbridge.h"

extern "C" {
QUIKMULTIBRIDGE_EXPORT int luaopen_QuikMultiBridge(lua_State *l);
}

class BridgeEventProcessor : public QObject
{
    Q_OBJECT
public:
    explicit BridgeEventProcessor(BridgePlugin *p);
    ~BridgeEventProcessor();
    void start();
    void stop();

private:
    BridgePlugin *plugin;
private slots:
    void appProcessEvents();
};

#endif // QUIKMULTIBRIDGE_H
