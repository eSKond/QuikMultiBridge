#ifndef QUIKMULTIBRIDGE_H
#define QUIKMULTIBRIDGE_H

#include "QuikMultiBridge_global.h"
#include <lua.hpp>
#include "pythonbridge.h"

extern "C" {
QUIKMULTIBRIDGE_EXPORT int luaopen_QuikMultiBridge(lua_State *l);
}

#endif // QUIKMULTIBRIDGE_H
