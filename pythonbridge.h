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
    ~PythonBridge();
    static PythonBridge *getBridge(const QVariantMap &cfg, QObject *parent = nullptr);
    static PythonBridge *getBridge();
    size_t sendToStdout(char *msg);
    void stdoutFlush();
    size_t sendToStderr(char *msg);
    void stderrFlush();
    virtual void start();
protected:
    virtual void callbackRequest(QString name, const QVariantList &args, QVariant &vres);
    virtual void fastCallbackRequest(BridgeCallableObject cobj, const QVariantList &args, QVariant &vres);
    static PythonBridge *singletonPythonBridge;
private slots:
    virtual void processEventsInBridge();
private:
    PythonBridge(const QVariantMap &cfg, QObject *parent = nullptr);
    static PyObject *b_invokeQuikObject(PyObject *self, PyObject *args);
    static PyObject *b_deleteQuikObject(PyObject *self, PyObject *args);
    static PyObject *b_invokeQuik(PyObject *self, PyObject *args);
    static PyObject *b_registerCallback(PyObject *self, PyObject *args);
    static PyObject *b_registerProcessEventsCallback(PyObject *self, PyObject *args);
    static PyObject *b_quitBridge(PyObject *self, PyObject *args);
    static PyObject *b_getQuikVariable(PyObject *self, PyObject *args);
    QMap<QString, PyObject *> registeredCallbacks;
    QByteArray pycode;
    bool isReady;
    PyObject *procEventCallback;
    bool exitRequested;
    QString stdoutBuffer;
    QString stderrBuffer;
};

#endif // PYTHONBRIDGE_H
