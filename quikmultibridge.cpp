#include "quikmultibridge.h"
#include <QDebug>
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <thread>

#define JUMP_TABLE_SIZE 100

static std::thread *mainThread = nullptr;

static void stackDump(lua_State *l)
{
    int i;
    int top = lua_gettop(l);
    qDebug() << "-- Stack Dump, Size=" << top << " --";
    for(i = 1; i <= top; i++)
    {
        int t = lua_type(l, i);
        switch(t)
        {
        case LUA_TSTRING:
            qDebug() << "String";
            break;
        case LUA_TBOOLEAN:
            qDebug() << "Bool";
            break;
        case LUA_TNUMBER:
            qDebug() << "Number";
            break;
        default:
            qDebug() << QString::fromLocal8Bit(lua_typename(l, t));
            break;
        }
    }
    qDebug() << "-----------";
}

lua_State *getRecentStackForThreadId(BridgePlugin *plug, Qt::HANDLE ctid)
{
    if(plug)
        return plug->getRecentStackForThreadId(ctid);
    return nullptr;
}

lua_State *getRecentStack(BridgePlugin *plug, Qt::HANDLE *p_ctid=nullptr)
{
    Qt::HANDLE ctid = QThread::currentThreadId();
    if(p_ctid)
        *p_ctid = ctid;
    return getRecentStackForThreadId(plug, ctid);
}

void setRecentStack(BridgePlugin *plug, lua_State *l)
{
    if(plug)
    {
        Qt::HANDLE ctid = QThread::currentThreadId();
        plug->setRecentStack(ctid, l);
    }
}

struct JumpTableItem
{
    BridgePlugin *ownerPlugin;
    Qt::HANDLE threadId;
    QString fName;
    void *customData;
    lua_CFunction callback;
    QString callerName;
    JumpTableItem():
        ownerPlugin(nullptr),
        threadId(nullptr),
        fName(QString()),
        customData(nullptr),
        callback(nullptr),
        callerName(QString())
    {}
};

JumpTableItem jumpTable[JUMP_TABLE_SIZE];


int findFreeJumpTableSlot()
{
    int i;
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(!jumpTable[i].customData && jumpTable[i].fName.isEmpty())
            return i;
    }
    return -1;
}

int findJumpTableSlotForNamedCallback(BridgePlugin *plug, QString cbName)
{
    int i;
    //search slot for replacement
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].ownerPlugin==plug && !jumpTable[i].customData && jumpTable[i].fName==cbName)
            return i;
    }
    return findFreeJumpTableSlot();
}

int findFreeOrExpiredJumpTableSlot(BridgePlugin *plug, QString caller)
{
    //find expired
    Qt::HANDLE ctid = QThread::currentThreadId();
    int i;
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].ownerPlugin==plug && jumpTable[i].customData && jumpTable[i].threadId==ctid && jumpTable[i].callerName==caller)
            return i;
    }
    //else find free
    return findFreeJumpTableSlot();
}

int findNamedJumpTableSlot(BridgePlugin *plug, QString cbName)
{
    int i;
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].ownerPlugin==plug && jumpTable[i].fName==cbName)
            return i;
    }
    return -1;
}

bool registerNamedCallback(BridgePlugin *plug, QString cbName)
{
    Qt::HANDLE ctid;
    lua_State *recentStack = getRecentStack(plug, &ctid);
    //qDebug() << "register named callback:" << cbName;
    if(recentStack)
    {
        int i=findJumpTableSlotForNamedCallback(plug, cbName);
        //qDebug() << "jumptable slot found:" << i;
        if(i<0)
        {
            qDebug() << "No more free callback slots. Can't register" << cbName;
            return false;
        }
        jumpTable[i].ownerPlugin = plug;
        jumpTable[i].threadId = ctid;
        jumpTable[i].fName = cbName;
        lua_register(recentStack, cbName.toLocal8Bit().data(), jumpTable[i].callback);
    }
    return true;
}

int registerFastCallback(BridgePlugin *plug, QString caller, void *data)
{
    Qt::HANDLE ctid = QThread::currentThreadId();
    int i=findFreeOrExpiredJumpTableSlot(plug, caller);
    if(i<0)
    {
        qDebug() << "No more free callback slots. Can't register fast callback";
        return -1;
    }
    jumpTable[i].ownerPlugin = plug;
    jumpTable[i].threadId = ctid;
    jumpTable[i].callerName = caller;
    jumpTable[i].customData = data;
    return i;
}

void unregisterAllNamedCallbacks(BridgePlugin *plug)
{
    int i;
    lua_State *recentStack = getRecentStack(plug);
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].ownerPlugin==plug && !jumpTable[i].fName.isEmpty())
        {
            lua_pushnil(recentStack);
            lua_setglobal(recentStack, jumpTable[i].fName.toLocal8Bit().data());
            jumpTable[i].ownerPlugin = nullptr;
            jumpTable[i].threadId = nullptr;
            jumpTable[i].customData = nullptr;
            jumpTable[i].fName = QString();
            jumpTable[i].callerName = QString();
        }
    }
}

void unregisterAllCallbacksForCaller(BridgePlugin *plug, QString caller)
{
    int i;
    Qt::HANDLE ctid = QThread::currentThreadId();
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].ownerPlugin==plug && jumpTable[i].customData && jumpTable[i].threadId==ctid && jumpTable[i].callerName==caller)
        {
            jumpTable[i].ownerPlugin = nullptr;
            jumpTable[i].threadId = nullptr;
            jumpTable[i].customData = nullptr;
            jumpTable[i].fName = QString();
            jumpTable[i].callerName = QString();
        }
    }
}

void unregisterAllObjectCallbacks(BridgePlugin *plug, int objid)
{
    QString prefix = QString("obj%1.").arg(objid);
    int i;
    Qt::HANDLE ctid = QThread::currentThreadId();
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].ownerPlugin==plug && jumpTable[i].customData && jumpTable[i].threadId==ctid && jumpTable[i].callerName.startsWith(prefix))
        {
            jumpTable[i].ownerPlugin = nullptr;
            jumpTable[i].threadId = nullptr;
            jumpTable[i].customData = nullptr;
            jumpTable[i].fName = QString();
            jumpTable[i].callerName = QString();
        }
    }
}

static int extractValueFromLuaStack(lua_State *l, int sid, QVariant &sVal, QVariantList &lVal, QVariantMap &mVal, int *dtype=nullptr)
{
    int resType; //0 - simple value, 1 - list, 2 - map
    sid = lua_absindex(l, sid);
    int t = lua_type(l, sid);
    sVal.clear();
    lVal.clear();
    mVal.clear();
    if(dtype)
        *dtype = t;
    switch(t)
    {
    case LUA_TBOOLEAN:
    {
        //qDebug() << "bool";
        bool v = (bool)lua_toboolean(l, sid);
        resType = 0;
        sVal = QVariant(v);
        break;
    }
    case LUA_TSTRING:
    {
        //qDebug() << "string";
        QString v = QString::fromLocal8Bit(lua_tostring(l, sid));
        resType = 0;
        sVal = QVariant(v);
        break;
    }
    case LUA_TNUMBER:
    {
        double v = lua_tonumber(l, sid);
        resType = 0;
        if(v==(int)v)
        {
            //qDebug() << "int";
            sVal = QVariant((int)v);
        }
        else
        {
            //qDebug() << "double";
            sVal = QVariant(v);
        }
        break;
    }
    case LUA_TTABLE:
    {
        lua_pushnil(l);
        int lidx=1;
        bool islist=true;
        int dtype;
        bool hasFunction=false;
        while(lua_next(l, sid) != 0)
        {
            int kt=lua_type(l, -2);
            QString k;
            int ridx=0;
            if(kt == LUA_TSTRING)
            {
                k = QString::fromLocal8Bit(lua_tostring(l, -2));
                islist=false;
            }
            else
            {
                ridx = (int)lua_tonumber(l, -2);
                k = QString("[%1]").arg(ridx);
                if(ridx != lidx)
                    islist=false;
            }
            QVariant sv;
            QVariantList lv;
            QVariantMap mv;
            int vtp = extractValueFromLuaStack(l, lua_gettop(l), sv, lv, mv, &dtype);
            if(!vtp)
            {
                lVal.append(sv);
                mVal.insert(k, sv);
                if(dtype == LUA_TFUNCTION)
                    hasFunction = true;
            }
            else
            {
                if(vtp==1)
                {
                    lVal.insert(lVal.length(), lv);
                    mVal.insert(k, lv);
                }
                else
                {
                    lVal.insert(lVal.length(), mv);
                    mVal.insert(k, mv);
                }
            }
            lidx++;
            lua_pop(l, 1);
        }
        if(islist)
        {
            //qDebug() << "list";
            mVal.clear();
            resType = 1;
        }
        else
        {
            if(hasFunction)
            {
                //qDebug() << "object";
                lua_pushvalue(l, sid); //копируем таблицу
                int objid = luaL_ref(l, LUA_REGISTRYINDEX); //сохраняем в реестр и возвращаем индекс в реестре
                lVal.clear();
                mVal.clear();
                QuikCallableObject qcobj;
                qcobj.objid = objid;
                sVal = QVariant::fromValue(qcobj);
                resType = 0;
            }
            else
            {
                //qDebug() << "dict";
                lVal.clear();
                resType = 2;
            }
        }
        break;
    }
    case LUA_TNIL:
    case LUA_TNONE:
        //qDebug() << "none";
        resType = 0;
        sVal = QVariant();
        break;
    case LUA_TFUNCTION:
        //qDebug() << "function";
        resType = 0;
        sVal = QVariant();
        break;
    default:
        //qDebug() << "Unknown type:" << QString::fromLocal8Bit(lua_typename(l, t));
        resType = 0;
        sVal = QVariant();
        break;
    }
    return resType;
}

void invokePlugin(BridgePlugin *plug, QString name, const QVariantList &args, QVariant &vres)
{
    if(plug)
        plug->callbackRequest(name, args, vres);
}

void fastInvokePlugin(BridgePlugin *plug, BridgeCallableObject cobj, const QVariantList &args, QVariant &vres)
{
    if(plug)
        plug->fastCallbackRequest(cobj, args, vres);
}

void pushVariantToLuaStack(lua_State *l, QVariant val, QString caller)
{
    switch(val.type())
    {
    case QVariant::Bool:
    {
        bool v = val.toBool();
        lua_pushboolean(l, v);
        break;
    }
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
    {
        qlonglong v = val.toLongLong();
        lua_pushinteger(l, v);
        break;
    }
    case QVariant::String:
    {
        QString v = val.toString();
        lua_pushstring(l, v.toLocal8Bit().data());
        break;
    }
    case QVariant::Double:
    {
        double v = val.toDouble();
        lua_pushnumber(l, v);
        break;
    }
    case QVariant::Map:
    {
        QVariantMap tbl = val.toMap();
        lua_newtable(l);
        QMapIterator<QString, QVariant> ti(tbl);
        while(ti.hasNext())
        {
            ti.next();
            QString k;
            QVariant v;
            k=ti.key();
            v=ti.value();
            lua_pushstring(l, k.toLocal8Bit().data());
            pushVariantToLuaStack(l, v, caller);
            lua_settable(l, -3);
        }
        break;
    }
    case QVariant::List:
    {
        QVariantList lst = val.toList();
        lua_newtable(l);
        for(int li=0;li<lst.count();li++)
        {
            QVariant v = lst.at(li);
            lua_pushinteger(l, li+1);
            pushVariantToLuaStack(l, v, caller);
            lua_settable(l, -3);
        }
        break;
    }
    default:
        if(val.canConvert<BridgeCallableObject>())
        {
            if(caller.isEmpty())
                qDebug() << "Callable object sent from lua?!";
            else
            {
                BridgeCallableObject fcb = val.value<BridgeCallableObject>();
                int cbi=registerFastCallback(fcb.ownerPlugin, caller, fcb.data);
                if(cbi<0)
                    lua_pushnil(l);
                else
                {
                    lua_pushcfunction(l, jumpTable[cbi].callback);
                }
            }
        }
        else
            lua_pushnil(l);
        break;
    }
}

QVariant popVariantFromLuaStack(lua_State *l)
{
    QVariant sv;
    QVariantList lv;
    QVariantMap mv;
    int vtp = extractValueFromLuaStack(l, -1, sv, lv, mv);
    lua_pop(l, 1);
    if(vtp == 1)
        return QVariant(lv);
    if(vtp == 2)
        return QVariant(mv);
    return sv;
}

bool getQuikVariable(BridgePlugin *plug, QString varname, QVariant &res)
{
    lua_State *recentStack = getRecentStack(plug);
    if(!recentStack)
    {
        qDebug() << "No stack?!";
        return false;
    }
    lua_getglobal(recentStack, varname.toLocal8Bit().data());
    res = popVariantFromLuaStack(recentStack);
    return true;
}

bool invokeQuik(BridgePlugin *plug, QString method, const QVariantList &args, QVariantList &res, QString &errMsg)
{
    //qDebug() << "invokeQuik:" << method;
    lua_State *recentStack = getRecentStack(plug);
    int top = lua_gettop(recentStack);
    lua_getglobal(recentStack, method.toLocal8Bit().data());
    res.clear();
    errMsg.clear();
    int li;
    for(li=0;li<args.count();li++)
    {
        QVariant v = args.at(li);
        pushVariantToLuaStack(recentStack, v, method);
    }
    //qDebug() << "lua_pcall...";
    int pcres=lua_pcall(recentStack, li, LUA_MULTRET, 0);
    if(pcres)
    {
        errMsg = QString::fromLocal8Bit(lua_tostring(recentStack, -1));
        //qDebug() << "..." << errMsg;
        lua_pop(recentStack, 1);
        return false;
    }
    //qDebug() << "...finished";
    while(lua_gettop(recentStack) != top)
    {
        QVariant resItem = popVariantFromLuaStack(recentStack);
        //qDebug() << "return" << resItem.toString();
        //мы забираем результаты с конца, поэтому и складывать нужно в обратном порядке
        res.prepend(resItem);
    }
    //qDebug() << "return from invokeQuik";
    return true;
}

bool invokeQuikObject(BridgePlugin *plug, int objid, QString method, const QVariantList &args, QVariantList &res, QString &errMsg)
{
    QString caller = QString("obj%1.%2").arg(objid).arg(method);
    lua_State *recentStack = getRecentStack(plug);
    int top = lua_gettop(recentStack);
    lua_rawgeti(recentStack, LUA_REGISTRYINDEX, objid);
    lua_getfield(recentStack, -1,  method.toLocal8Bit().data());
    lua_pushvalue(recentStack, -2);
    res.clear();
    errMsg.clear();
    int li;
    for(li=0;li<args.count();li++)
    {
        QVariant v = args.at(li);
        pushVariantToLuaStack(recentStack, v, caller);
    }
    int pcres=lua_pcall(recentStack, li+1, LUA_MULTRET, 0);
    if(pcres)
    {
        errMsg = QString::fromLocal8Bit(lua_tostring(recentStack, -1));
        lua_pop(recentStack, 1);
        return false;
    }
    while(lua_gettop(recentStack) != top+1)
    {
        QVariant resItem = popVariantFromLuaStack(recentStack);
        //мы забираем результаты с конца, поэтому и складывать нужно в обратном порядке
        res.prepend(resItem);
    }
    lua_pop(recentStack, 1);
    return true;
}

void deleteQuikObject(BridgePlugin *plug, int objid)
{
    lua_State *recentStack = getRecentStack(plug);
    if(!recentStack)
        return;
    unregisterAllObjectCallbacks(plug, objid);
    luaL_unref(recentStack, LUA_REGISTRYINDEX, objid);
}

static int initBridge(lua_State *l)
{
    BridgePlugin *plug = nullptr;
    QVariant sv;
    QVariantList lv;
    QVariantMap mv;
    int vtp = extractValueFromLuaStack(l, 1, sv, lv, mv);
    if(vtp)
    {
        qDebug() << "First argument must specify bridge type('Python', 'QtRO', 'QtPlugin','R')";
        return 1;
    }
    QString bridgeType = sv.toString();
    if(bridgeType != "Python" &&
            bridgeType != "QtRO" &&
            bridgeType != "QtPlugin" &&
            bridgeType != "R")
    {
        qDebug() << "Unknown bridge type. Possible values: 'Python', 'QtRO', 'QtPlugin','R'";
        return 1;
    }
    vtp = extractValueFromLuaStack(l, 2, sv, lv, mv);
    if(vtp != 2)
    {
        qDebug() << "Second argument must be bridge configuration table";
        return 1;
    }
    //проверим специальный параметр "eventLoopName" и создадим главный цикл вне
    //плагина - это нужно поскольку завершение этого цикла
    //сигнализирует о завершении всего плагина и позволяет очистить память аккуратно
    //сам же плагин должен зарегистрировать этот колбэк через registerCallback
    QString loceln;
    if(mv.contains("eventLoopName"))
    {
        loceln=mv.value("eventLoopName").toString();
    }
    //здесь нужно создать соответствующий плагин с заданной конфигурацией
    if(bridgeType == "Python")
    {
        qDebug() << "Create PythonBridge...";
        plug = new PythonBridge(mv);
    }
    if(plug)
    {
        if(!loceln.isEmpty())
            plug->setEventLoopName(loceln);
        setRecentStack(plug, l);
        qDebug() << "Start plugin...";
        plug->start();
    }

    return 1;
}

static struct luaL_Reg ls_lib[] = {
    {"initBridge", initBridge},
    {nullptr, nullptr}
};

static volatile int appStarted = 0;
static void qmbMain()
{
    int argc = 1;
    char appname[] = "QuikMultiBridge";
    char* argv[] = {appname, NULL};
    QCoreApplication app(argc, argv);
    qDebug() << "start application...";
    appStarted=1;
    app.exec();
    qDebug() << "Application finished";
}

int luaopen_QuikMultiBridge(lua_State *l)
{
    if(!appStarted)
    {
        mainThread = new std::thread(qmbMain);
        while(!appStarted);
    }
    luaL_newlib(l, ls_lib);
    qDebug() << "Register initBridge...";
    lua_register(l, "initBridge", initBridge);
    qDebug() << "initBridge registered";
    return 0;
}

static int universalCallbackHandler(JumpTableItem *jitem, lua_State *l)
{
    QVariantList args;
    QVariant sv, vres;
    QVariantList lv;
    QVariantMap mv;
    int i;
    //qDebug() << "universalCallbackHandler: start";
    int top = lua_gettop(l);
    for(i = 1; i <= top; i++)
    {
        int vtp = extractValueFromLuaStack(l, i, sv, lv, mv);
        if(!vtp)
        {
            args.append(sv);
        }
        else
        {
            if(vtp==1)
            {
                args.insert(args.length(), lv);
            }
            else
            {
                args.insert(args.length(), mv);
            }
        }
    }
    //qDebug() << "universalCallbackHandler: get plugin";
    BridgePlugin *plug = jitem->ownerPlugin;
    /*
    if(plug)
        qDebug() << "universalCallbackHandler: plugin found";
    else
        qDebug() << "universalCallbackHandler: plugin is not found!";
    */
    setRecentStack(plug, l);
    bool finished = false;
    if(jitem->fName.isEmpty())
    {
        //qDebug() << "universalCallbackHandler: fast callback";
        if(jitem->customData)
        {
            BridgeCallableObject cobj;
            cobj.ownerPlugin = plug;
            cobj.data = jitem->customData;
            fastInvokePlugin(plug, cobj, args, vres);
        }
    }
    else
    {
        //qDebug() << "universalCallbackHandler: named callback:" << jitem->fName;
        if(jitem->fName == plug->getEventLoopName())
            finished=true;
        invokePlugin(plug, jitem->fName, args, vres);
    }
    int rescnt = 0;
    if(!vres.isNull())
    {
        //qDebug() << "universalCallbackHandler: results received";
        if(vres.type() == QVariant::List)
        {
            //qDebug() << "universalCallbackHandler: return tuple";
            //special case: multiple results
            QVariantList lst = vres.toList();
            for(int li=0;li<lst.count();li++)
            {
                QVariant v = lst.at(li);
                //qDebug() << "item" << li << QString::fromLocal8Bit(v.typeName());
                pushVariantToLuaStack(l, v, QString());
                rescnt++;
            }
        }
        else
        {
            //qDebug() << "universalCallbackHandler: return single value";
            pushVariantToLuaStack(l, vres, QString());
            rescnt++;
        }
    }
    if(finished)
    {
        delete plug;
        if(!BridgePlugin::getPluginCount())
        {
            qApp->quit();
            //mainThread->join();
        }
    }
    //qDebug() << "rescnt=" << rescnt;
    return rescnt;
}

template<int i> int cbHandler(lua_State *l)
{
    //qDebug() << "cbHandler" << i;
    return universalCallbackHandler(&jumpTable[i], l);
}

template<int i> bool JumpTable_init()
{
  jumpTable[i-1].callback = &cbHandler<i-1>;
  return JumpTable_init<i-1>();
}

template<> bool JumpTable_init<-1>(){ return true; }

const bool jt_initialized = JumpTable_init<JUMP_TABLE_SIZE>();
