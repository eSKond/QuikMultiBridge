#include "bridgeplugin.h"
#include <QDebug>

QList<BridgePlugin *> *BridgePlugin::pluginRegistry = nullptr;

BridgePlugin::BridgePlugin(const QVariantMap &cfg, QObject *parent)
    : QObject(parent)
{
    Q_UNUSED(cfg);
    if(!pluginRegistry)
        pluginRegistry = new QList<BridgePlugin *>();
    pluginRegistry->append(this);
}

BridgePlugin::~BridgePlugin()
{
    pluginRegistry->removeAll(this);
}

int BridgePlugin::getPluginCount()
{
    if(!pluginRegistry)
        return 0;
    return pluginRegistry->count();
}

lua_State *BridgePlugin::getRecentStackForThreadId(Qt::HANDLE ctid)
{
    if(recentStackMap.contains(ctid))
        return recentStackMap.value(ctid);
    return nullptr;
}

void BridgePlugin::setRecentStack(Qt::HANDLE ctid, lua_State *l)
{
    recentStackMap.insert(ctid, l);
}

void BridgePlugin::sendStdoutLine(QString line)
{
    qDebug().noquote() << line;
}

void BridgePlugin::sendStderrLine(QString line)
{
    qDebug().noquote() << line;
}

bool invokeQuik(BridgePlugin *plug, QString method, const QVariantList &args, QVariantList &res, QString &errMsg);
void BridgePlugin::invokeQuik(QString method, const QVariantList &args, QVariantList &res)
{
    QString errMsg;
    if(!::invokeQuik(this, method, args, res, errMsg))
        qDebug() << errMsg;
}

bool invokeQuikObject(BridgePlugin *plug, int objid, QString method, const QVariantList &args, QVariantList &res, QString &errMsg);
void BridgePlugin::invokeQuikObject(int objid, QString method, const QVariantList &args, QVariantList &res)
{
    QString errMsg;
    if(!::invokeQuikObject(this, objid, method, args, res, errMsg))
        qDebug() << errMsg;
}

void deleteQuikObject(BridgePlugin *plug, int objid);
void BridgePlugin::deleteQuikObject(int objid)
{
    ::deleteQuikObject(this, objid);
}

bool registerNamedCallback(BridgePlugin *plug, QString cbName);
void BridgePlugin::registerCallback(QString name)
{
    ::registerNamedCallback(this, name);
}

bool getQuikVariable(BridgePlugin *plug, QString varname, QVariant &res);
void BridgePlugin::getQuikVariable(QString varname, QVariant &res)
{
    ::getQuikVariable(this, varname, res);
}
