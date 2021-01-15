#ifndef BRIDGEPLUGIN_H
#define BRIDGEPLUGIN_H

#include <QObject>
#include <QVariant>

struct BridgeCallableObject
{
    void *data;
    BridgeCallableObject():data(nullptr){}
};
Q_DECLARE_METATYPE(BridgeCallableObject)

class BridgePlugin : public QObject
{
    Q_OBJECT
public:
    explicit BridgePlugin(const QVariantMap &cfg, QObject *parent = nullptr);

    friend class QuikMultiBridgeApplication;
    friend void invokePlugin(QString name, const QVariantList &args, QVariant &vres);
    friend void fastInvokePlugin(BridgeCallableObject cobj, const QVariantList &args, QVariant &vres);
protected:
    static void invokeQuik(QString method, const QVariantList &args, QVariantList &res);
    static void registerCallback(QString name);
    virtual void callbackRequest(QString name, const QVariantList &args, QVariant &vres) = 0;
    virtual void fastCallbackRequest(BridgeCallableObject cobj, const QVariantList &args, QVariant &res) = 0;
    virtual void processEventsInBridge() = 0;
signals:
    void exitBridgeSignal();
};

#endif // BRIDGEPLUGIN_H
