#include "bridgeplugin.h"
#include <QDebug>

BridgePlugin::BridgePlugin(const QVariantMap &cfg, QObject *parent)
    : QObject(parent)
{
    Q_UNUSED(cfg);
}

bool invokeQuik(QString method, const QVariantList &args, QVariantList &res, QString &errMsg);
void BridgePlugin::invokeQuik(QString method, const QVariantList &args, QVariantList &res)
{
    QString errMsg;
    if(!::invokeQuik(method, args, res, errMsg))
        qDebug() << errMsg;
}

bool invokeQuikObject(int objid, QString method, const QVariantList &args, QVariantList &res, QString &errMsg);
void BridgePlugin::invokeQuikObject(int objid, QString method, const QVariantList &args, QVariantList &res)
{
    QString errMsg;
    if(!::invokeQuikObject(objid, method, args, res, errMsg))
        qDebug() << errMsg;
}

void deleteQuikObject(int objid);
void BridgePlugin::deleteQuikObject(int objid)
{
    ::deleteQuikObject(objid);
}

bool registerNamedCallback(QString cbName);
void BridgePlugin::registerCallback(QString name)
{
    ::registerNamedCallback(name);
}

bool getQuikVariable(QString varname, QVariant &res);
void BridgePlugin::getQuikVariable(QString varname, QVariant &res)
{
    ::getQuikVariable(varname, res);
}

void BridgePlugin::processEventsInBridge()
{
}
