#ifndef PYTHONBRIDGE_H
#define PYTHONBRIDGE_H

#include "bridgeplugin.h"
#include <QObject>
#include <QFile>
#include <QTextStream>
#undef slots
#include <Python.h>
#define slots Q_SLOTS

class PythonBridge : public BridgePlugin
{
    Q_OBJECT
public:
    ~PythonBridge();
    static PythonBridge *getBridge(const QVariantMap &cfg, QObject *parent = nullptr);
    //we need implement bridge to fllowing methods only:
    //virtual void invokeQuik(QString method, const QVariantList &args, QVariantList &res);
    //virtual void registerCallback(QString name);
protected:
    virtual void callbackRequest(QString name, const QVariantList &args, QVariant &vres);
    virtual void fastCallbackRequest(BridgeCallableObject cobj, const QVariantList &args, QVariant &vres);
    virtual void processEventsInBridge();
    static PythonBridge *singletonPythonBridge;
private:
    PythonBridge(const QVariantMap &cfg, QObject *parent = nullptr);
    static PyObject *b_invokeQuik(PyObject *self, PyObject *args);
    static PyObject *b_registerCallback(PyObject *self, PyObject *args);
    static PyObject *b_registerProcessEventsCallback(PyObject *self, PyObject *args);
    static PyObject *b_quitBridge(PyObject *self, PyObject *args);
    QMap<QString, PyObject *> registeredCallbacks;
    QByteArray pycode;
    PyObject *procEventCallback;
};

#endif // PYTHONBRIDGE_H
