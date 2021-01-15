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

bool registerNamedCallback(QString cbName);
void BridgePlugin::registerCallback(QString name)
{
    ::registerNamedCallback(name);
}
