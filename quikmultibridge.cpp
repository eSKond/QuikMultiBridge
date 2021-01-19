#include "quikmultibridge.h"
#include <QDebug>
#include <QCoreApplication>
#include <QTimer>
#include <QThread>

#define JUMP_TABLE_SIZE 100

BridgePlugin *activePlugin=nullptr;
Q_GLOBAL_STATIC(QString, eventLoopName)
QMap<Qt::HANDLE, lua_State *> *recentStackMap=nullptr;
Q_GLOBAL_STATIC(QList<int>, objRegistry)
QCoreApplication *p_app=nullptr;

lua_State *getRecentStackForThreadId(Qt::HANDLE ctid)
{
    if(!recentStackMap)
        recentStackMap = new QMap<Qt::HANDLE, lua_State *>;
    if(recentStackMap->contains(ctid))
        return recentStackMap->value(ctid);
    return nullptr;
}

lua_State *getRecentStack(Qt::HANDLE *p_ctid=nullptr)
{
    Qt::HANDLE ctid = QThread::currentThreadId();
    if(p_ctid)
        *p_ctid = ctid;
    return getRecentStackForThreadId(ctid);
}

void setRecentStack(lua_State *l)
{
    if(!recentStackMap)
        recentStackMap = new QMap<Qt::HANDLE, lua_State *>;
    Qt::HANDLE ctid = QThread::currentThreadId();
    recentStackMap->insert(ctid, l);
}

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

struct JumpTableItem
{
    Qt::HANDLE threadId;
    QString fName;
    void *customData;
    lua_CFunction callback;
    QString callerName;
    JumpTableItem():
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

int findJumpTableSlotForNamedCallback(QString cbName)
{
    int i;
    //search slot for replacement
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(!jumpTable[i].customData && jumpTable[i].fName==cbName)
            return i;
    }
    return findFreeJumpTableSlot();
}

int findFreeOrExpiredJumpTableSlot(QString caller)
{
    //find expired
    Qt::HANDLE ctid = QThread::currentThreadId();
    int i;
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].customData && jumpTable[i].threadId==ctid && jumpTable[i].callerName==caller)
            return i;
    }
    //else find free
    return findFreeJumpTableSlot();
}

int findNamedJumpTableSlot(QString cbName)
{
    int i;
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].fName==cbName)
            return i;
    }
    return -1;
}

bool registerNamedCallback(QString cbName)
{
    Qt::HANDLE ctid;
    lua_State *recentStack = getRecentStack(&ctid);
    if(recentStack)
    {
        int i=findJumpTableSlotForNamedCallback(cbName);
        if(i<0)
        {
            qDebug() << "No more free callback slots. Can't register" << cbName;
            return false;
        }
        jumpTable[i].threadId = ctid;
        jumpTable[i].fName = cbName;
        lua_register(recentStack, cbName.toLocal8Bit().data(), jumpTable[i].callback);
    }
    return true;
}

int registerFastCallback(QString caller, void *data)
{
    Qt::HANDLE ctid = QThread::currentThreadId();
    int i=findFreeOrExpiredJumpTableSlot(caller);
    if(i<0)
    {
        qDebug() << "No more free callback slots. Can't register fast callback";
        return -1;
    }
    jumpTable[i].threadId = ctid;
    jumpTable[i].callerName = caller;
    jumpTable[i].customData = data;
    return i;
}

void unregisterCallback(int i)
{
    if(i>=0 && i<JUMP_TABLE_SIZE)
    {
        if(!jumpTable[i].fName.isEmpty())
        {
            lua_State *recentStack = getRecentStack();
            if(recentStack)
            {
                lua_pushnil(recentStack);
                lua_setglobal(recentStack, jumpTable[i].fName.toLocal8Bit().data());
            }
        }
        jumpTable[i].threadId = nullptr;
        jumpTable[i].customData = nullptr;
        jumpTable[i].fName = QString();
        jumpTable[i].callerName = QString();
    }
}

void unregisterAllNamedCallbacks()
{
    int i;
    lua_State *recentStack = getRecentStack();
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(!jumpTable[i].fName.isEmpty())
        {
            lua_pushnil(recentStack);
            lua_setglobal(recentStack, jumpTable[i].fName.toLocal8Bit().data());
            jumpTable[i].threadId = nullptr;
            jumpTable[i].customData = nullptr;
            jumpTable[i].fName = QString();
            jumpTable[i].callerName = QString();
        }
    }
}

void unregisterAllCallbacksForCaller(QString caller)
{
    int i;
    Qt::HANDLE ctid = QThread::currentThreadId();
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].customData && jumpTable[i].threadId==ctid && jumpTable[i].callerName==caller)
        {
            jumpTable[i].threadId = nullptr;
            jumpTable[i].customData = nullptr;
            jumpTable[i].fName = QString();
            jumpTable[i].callerName = QString();
        }
    }
}

void unregisterAllObjectCallbacks(int objid)
{
    QString prefix = QString("obj%1.").arg(objid);
    int i;
    Qt::HANDLE ctid = QThread::currentThreadId();
    for(i=0; i<JUMP_TABLE_SIZE; i++)
    {
        if(jumpTable[i].customData && jumpTable[i].threadId==ctid && jumpTable[i].callerName.startsWith(prefix))
        {
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
        bool v = (bool)lua_toboolean(l, sid);
        resType = 0;
        sVal = QVariant(v);
        break;
    }
    case LUA_TSTRING:
    {
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
            sVal = QVariant((int)v);
        }
        else
        {
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
            mVal.clear();
            resType = 1;
        }
        else
        {
            if(hasFunction)
            {
                lua_pushvalue(l, sid); //копируем таблицу
                int objid = luaL_ref(l, LUA_REGISTRYINDEX); //сохраняем в реестр и возвращаем индекс в реестре
                lVal.clear();
                mVal.clear();
                sVal = QVariant(objid);
                resType = 0;
                objRegistry()->append(objid);
            }
            else
            {
                lVal.clear();
                resType = 2;
            }
        }
        break;
    }
    case LUA_TNIL:
    case LUA_TNONE:
        resType = 0;
        sVal = QVariant();
        break;
    case LUA_TFUNCTION:
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

void invokePlugin(QString name, const QVariantList &args, QVariant &vres)
{
    if(name == *eventLoopName())
    {
        if(activePlugin)
        {
            static int argc=1;
            static const char * const progName="QuikMultiBridge";
            static char **argv = const_cast<char **>(&progName);
            p_app = new QCoreApplication(argc, argv);
            BridgeEventProcessor evprc(activePlugin);
            QObject::connect(activePlugin, SIGNAL(exitBridgeSignal()), p_app, SLOT(quit()));
            evprc.start();
            p_app->exec();
            evprc.stop();
            delete p_app;
            p_app = nullptr;
        }
        eventLoopName()->clear();
        unregisterAllNamedCallbacks();
    }
    else
    {
        if(activePlugin)
            activePlugin->callbackRequest(name, args, vres);
    }
}

void fastInvokePlugin(BridgeCallableObject cobj, const QVariantList &args, QVariant &vres)
{
    if(activePlugin)
        activePlugin->fastCallbackRequest(cobj, args, vres);
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
                int cbi=registerFastCallback(caller, fcb.data);
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

bool getQuikVariable(QString varname, QVariant &res)
{
    lua_State *recentStack = getRecentStack();
    if(!recentStack)
    {
        qDebug() << "No stack?!";
        return false;
    }
    lua_getglobal(recentStack, varname.toLocal8Bit().data());
    res = popVariantFromLuaStack(recentStack);
    return true;
}

bool invokeQuik(QString method, const QVariantList &args, QVariantList &res, QString &errMsg)
{
    lua_State *recentStack = getRecentStack();
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
    int pcres=lua_pcall(recentStack, li, LUA_MULTRET, 0);
    if(pcres)
    {
        errMsg = QString::fromLocal8Bit(lua_tostring(recentStack, -1));
        lua_pop(recentStack, 1);
        return false;
    }
    while(lua_gettop(recentStack) != top)
    {
        QVariant resItem = popVariantFromLuaStack(recentStack);
        //мы забираем результаты с конца, поэтому и складывать нужно в обратном порядке
        res.prepend(resItem);
    }
    return true;
}

bool invokeQuikObject(int objid, QString method, const QVariantList &args, QVariantList &res, QString &errMsg)
{
    if(!objRegistry()->contains(objid))
        return false;
    QString caller = QString("obj%1.%2").arg(objid).arg(method);
    lua_State *recentStack = getRecentStack();
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

void deleteQuikObject(int objid)
{
    if(!objRegistry()->contains(objid))
        return;
    lua_State *recentStack = getRecentStack();
    if(!recentStack)
        return;
    unregisterAllObjectCallbacks(objid);
    luaL_unref(recentStack, LUA_REGISTRYINDEX, objid);
    objRegistry()->removeAll(objid);
}

static int initBridge(lua_State *l)
{
    if(activePlugin)
    {
        qDebug() << "Bridge already initialized";
        return 1;
    }
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
    //плагина - это нужно по 2-м причинам: 1 - этот цикл используется для
    //обработки пользовательского интерфейса, 2 - завершение этого цикла
    //сигнализирует о завершении всего плагина и позволяет очистить память аккуратно
    //сам же плагин вместо этого метода должен зарегистрировать специальный колбэк через
    //registerProcessEventsCallback, который будет вызываться внутри eventLoop
    if(mv.contains("eventLoopName"))
    {
        QString loceln=mv.value("eventLoopName").toString();
        *eventLoopName()=loceln;
        registerNamedCallback(loceln);
    }
    //здесь нужно создать соответствующий плагин с заданной конфигурацией
    setRecentStack(l);
    if(bridgeType == "Python")
    {
        activePlugin = PythonBridge::getBridge(mv);
    }
    if(activePlugin)
    {
        activePlugin->start();
    }

    return 1;
}

int luaopen_QuikMultiBridge(lua_State *l)
{
    //Нам не нужно регистрировать библиотеку, нам просто нужно установить глобальные указатели функций
    //luaL_newlib(l, qmblib);
    //lua_pushvalue(l, -1);
    //lua_setglobal(l, "QuikMultiBridge");
    //Можно было бы прочитать некий конфиг по имени головного скрипта и по нему делать инициализацию,
    //но мы предпочтём явную инициализацию из скрипта lua вызовом метода initBridge
    //lua_Debug ar;
    //lua_getstack(l, 2, &ar);
    //lua_getinfo(l, "S", &ar);
    //QString scriptName = QString::fromLocal8Bit(ar.short_src);

    setRecentStack(l);
    lua_register(l, "initBridge", initBridge);

    return 0;
}
static int universalCallbackHandler(JumpTableItem *jitem, lua_State *l)
{
    QVariantList args;
    QVariant sv, vres;
    QVariantList lv;
    QVariantMap mv;
    int i;
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
    setRecentStack(l);
    bool finished = false;
    if(jitem->fName.isEmpty())
    {
        if(jitem->customData)
        {
            BridgeCallableObject cobj;
            cobj.data = jitem->customData;
            fastInvokePlugin(cobj, args, vres);
        }
    }
    else
    {
        if(jitem->fName == *eventLoopName())
            finished=true;
        invokePlugin(jitem->fName, args, vres);
    }
    if(!vres.isNull())
    {
        pushVariantToLuaStack(l, vres, QString());
    }
    if(finished)
    {
        delete activePlugin;
        activePlugin=nullptr;
    }
    return 1;
}

template<int i> int cbHandler(lua_State *l)
{
    return universalCallbackHandler(&jumpTable[i], l);
}

template<int i> bool JumpTable_init()
{
  jumpTable[i-1].callback = &cbHandler<i-1>;
  return JumpTable_init<i-1>();
}

template<> bool JumpTable_init<-1>(){ return true; }

const bool jt_initialized = JumpTable_init<JUMP_TABLE_SIZE>();


BridgeEventProcessor::BridgeEventProcessor(BridgePlugin *p)
    : QObject(), plugin(p)
{
}

BridgeEventProcessor::~BridgeEventProcessor()
{
    stop();
}

void BridgeEventProcessor::start()
{
    if(plugin)
        QTimer::singleShot(0, this, SLOT(appProcessEvents()));
}

void BridgeEventProcessor::stop()
{
    plugin=nullptr;
}

void BridgeEventProcessor::appProcessEvents()
{
    if(plugin)
    {
        plugin->processEventsInBridge();
        QTimer::singleShot(0, this, SLOT(appProcessEvents()));
    }
}
