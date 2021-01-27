#ifndef BRIDGEPLUGIN_H
#define BRIDGEPLUGIN_H

#include <QObject>
#include <QVariant>
#include <QThread>
#include <lua.hpp>

class BridgePlugin;
struct BridgeCallableObject
{
    BridgePlugin *ownerPlugin;
    void *data;
    BridgeCallableObject():ownerPlugin(nullptr), data(nullptr){}
};
Q_DECLARE_METATYPE(BridgeCallableObject)

struct QuikCallableObject
{
    int objid;
    QuikCallableObject():objid(-1){}
};
Q_DECLARE_METATYPE(QuikCallableObject)

class BridgePlugin : public QObject
{
    Q_OBJECT
public:
    explicit BridgePlugin(const QVariantMap &cfg, QObject *parent = nullptr);
    ~BridgePlugin();

    virtual void invokeQuik(QString method, const QVariantList &args, QVariantList &res);
    virtual void invokeQuikObject(int objid, QString method, const QVariantList &args, QVariantList &res);
    virtual void deleteQuikObject(int objid);
    virtual void registerCallback(QString name);
    virtual void getQuikVariable(QString varname, QVariant &res);

    static int getPluginCount();

    friend void invokePlugin(BridgePlugin *plug, QString name, const QVariantList &args, QVariant &vres);
    friend void fastInvokePlugin(BridgePlugin *plug, BridgeCallableObject cobj, const QVariantList &args, QVariant &vres);
    virtual void start() = 0;

    lua_State *getRecentStackForThreadId(Qt::HANDLE ctid);
    void setRecentStack(Qt::HANDLE ctid, lua_State *l);
    void setEventLoopName(QString evlName){eventLoopName=evlName;}
    QString getEventLoopName(){return eventLoopName;}
protected:
    QString eventLoopName;
    QMap<Qt::HANDLE, lua_State *> recentStackMap;

    virtual void callbackRequest(QString name, const QVariantList &args, QVariant &vres) = 0;
    virtual void fastCallbackRequest(BridgeCallableObject cobj, const QVariantList &args, QVariant &res) = 0;

    void sendStdoutLine(QString line);
    void sendStderrLine(QString line);
private:
    static QList<BridgePlugin *> *pluginRegistry;
};

#endif // BRIDGEPLUGIN_H
