#ifndef QUIKMULTIBRIDGEAPPLICATION_H
#define QUIKMULTIBRIDGEAPPLICATION_H

#include <QObject>
#include <bridgeplugin.h>

class QuikMultiBridgeApplication : public QObject
{
    Q_OBJECT
public:
    explicit QuikMultiBridgeApplication(BridgePlugin **pp, QObject *parent = nullptr);

private:
    BridgePlugin **pluginPointer;
private slots:
    void appProcessEvents();
};

#endif // QUIKMULTIBRIDGEAPPLICATION_H
