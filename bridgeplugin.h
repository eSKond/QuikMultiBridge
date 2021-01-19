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

    friend class BridgeEventProcessor;
    friend void invokePlugin(QString name, const QVariantList &args, QVariant &vres);
    friend void fastInvokePlugin(BridgeCallableObject cobj, const QVariantList &args, QVariant &vres);
    virtual void start() = 0;
protected:
    static void invokeQuik(QString method, const QVariantList &args, QVariantList &res);
    static void invokeQuikObject(int objid, QString method, const QVariantList &args, QVariantList &res);
    static void deleteQuikObject(int objid);
    static void registerCallback(QString name);
    static void getQuikVariable(QString varname, QVariant &res);
    virtual void callbackRequest(QString name, const QVariantList &args, QVariant &vres) = 0;
    virtual void fastCallbackRequest(BridgeCallableObject cobj, const QVariantList &args, QVariant &res) = 0;
private slots:
    virtual void processEventsInBridge();
signals:
    void exitBridgeSignal();
};

#endif // BRIDGEPLUGIN_H
