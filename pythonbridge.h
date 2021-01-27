#ifndef PYTHONBRIDGE_H
#define PYTHONBRIDGE_H

#include "bridgeplugin.h"
#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#undef slots
#include <Python.h>
#define slots Q_SLOTS

class PythonBridge : public BridgePlugin
{
    Q_OBJECT
public:
    explicit PythonBridge(const QVariantMap &cfg, QObject *parent = nullptr);
    ~PythonBridge();

    void registerPythonCallback(PyObject *cb, QString name);

    size_t sendToStdout(char *msg);
    void stdoutFlush();
    size_t sendToStderr(char *msg);
    void stderrFlush();
    virtual void start();
protected:
    virtual void callbackRequest(QString name, const QVariantList &args, QVariant &vres);
    virtual void fastCallbackRequest(BridgeCallableObject cobj, const QVariantList &args, QVariant &vres);
private:
    bool isReady;
    QByteArray pycode;
    PyObject *moduleObject;
    QMap<QString, PyObject *> registeredCallbacks;
    QString stdoutBuffer;
    QString stderrBuffer;
    QString modObjName;
};

#endif // PYTHONBRIDGE_H
