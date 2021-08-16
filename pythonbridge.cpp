#include "pythonbridge.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QStack>
#include <QMutex>
#include <stdio.h>

class PyBridgeThreadContext
{
public:
    PyBridgeThreadContext(Qt::HANDLE tid) : threadId(tid){}
    Qt::HANDLE getThreadId(){return threadId;}
    void push(PythonBridge *b, QString name=QString())
    {
        frames.push(b);
        names.push(name);
    }
    PythonBridge * top(){return frames.top();}
    QString topName(){return names.top();}
    PythonBridge * pop(QString *nsave=nullptr)
    {
        QString name = names.pop();
        if(nsave)
            *nsave = name;
        return frames.pop();
    }
    bool isEmpty(){return frames.isEmpty();}
private:
    Qt::HANDLE threadId;
    QStack<PythonBridge *> frames;
    QStack<QString> names;
};

class PyBridgeContextMap
{
public:
    PyBridgeContextMap()
    {
        //mainThreadId = QThread::currentThreadId();
    }
    void restoreFrame(PythonBridge *b, QString name = QString())
    {
        /*
        if(name.isEmpty())
            qDebug() << "restoreFrame";
        else
            qDebug() << "restoreFrame:" << name;
        */
        Qt::HANDLE ctid = QThread::currentThreadId();
        PyBridgeThreadContext * tctx;
        if(ctxByThread.contains(ctid))
            tctx = ctxByThread.value(ctid);
        else
        {
            tctx = new PyBridgeThreadContext(ctid);
            ctxByThread.insert(ctid, tctx);
        }
        tctx->push(b, name);
        pythonLocker.lock();
        //qDebug() << "python locked";
    }
    void beginFrame(PythonBridge *b, QString name = QString())
    {
        /*
        if(name.isEmpty())
            qDebug() << "beginFrame";
        else
            qDebug() << "beginFrame:" << name;
        */
        restoreFrame(b, name);
    }
    PythonBridge * currentFrame()
    {
        Qt::HANDLE ctid = QThread::currentThreadId();
        PyBridgeThreadContext * tctx;
        if(ctxByThread.contains(ctid))
            tctx = ctxByThread.value(ctid);
        else
            return nullptr;
        return tctx->top();
    }
    PythonBridge * saveFrame(QString &name)
    {
        pythonLocker.unlock();
        Qt::HANDLE ctid = QThread::currentThreadId();
        PyBridgeThreadContext * tctx;
        if(ctxByThread.contains(ctid))
            tctx = ctxByThread.value(ctid);
        else
            return nullptr;
        PythonBridge * b = tctx->pop(&name);
        if(tctx->isEmpty())
        {
            ctxByThread.remove(ctid);
            delete tctx;
        }
        /*
        if(name.isEmpty())
            qDebug() << "saveFrame";
        else
            qDebug() << "saveFrame:" << name;
        */
        return b;
    }
    PythonBridge * endFrame()
    {
        QString name;
        PythonBridge * b = saveFrame(name);
        /*
        if(name.isEmpty())
            qDebug() << "endFrame";
        else
            qDebug() << "endFrame:" << name;
        */
        return b;
    }
private:
    QMap<Qt::HANDLE, PyBridgeThreadContext *> ctxByThread;
    //Qt::HANDLE mainThreadId;
    QMutex pythonLocker;
};

Q_GLOBAL_STATIC(PyBridgeContextMap, contextMap)

struct BridgeStdout
{
    PyObject_HEAD
};

struct BridgeStderr
{
    PyObject_HEAD
};

PyObject* BridgeStdout_write(PyObject* self, PyObject* args)
{
    Q_UNUSED(self)
    std::size_t written = 0;
    char* data;
    if (!PyArg_ParseTuple(args, "s", &data))
        return nullptr;
    PythonBridge *b = contextMap()->currentFrame();
    if(!b)
    {
        qDebug() << "stdout write without context";
        return PyLong_FromSize_t(0);
    }
    //Py_BEGIN_ALLOW_THREADS
    QString fname;
    b = contextMap()->saveFrame(fname);
    written = b->sendToStdout(data);
    //Py_END_ALLOW_THREADS
    contextMap()->restoreFrame(b, fname);
    return PyLong_FromSize_t(written);
}

PyObject* BridgeStderr_write(PyObject* self, PyObject* args)
{
    Q_UNUSED(self)
    std::size_t written = 0;
    char* data;
    if (!PyArg_ParseTuple(args, "s", &data))
        return nullptr;
    PythonBridge *b = contextMap()->currentFrame();
    if(!b)
    {
        qDebug() << "stderr write without context";
        return PyLong_FromSize_t(0);
    }
    //Py_BEGIN_ALLOW_THREADS
    QString fname;
    b = contextMap()->saveFrame(fname);
    written = b->sendToStderr(data);
    //Py_END_ALLOW_THREADS
    contextMap()->restoreFrame(b, fname);
    return PyLong_FromSize_t(written);
}

PyObject* BridgeStdout_flush(PyObject* self, PyObject* args)
{
    Q_UNUSED(self)
    Q_UNUSED(args)
    //Py_BEGIN_ALLOW_THREADS
    QString fname;
    PythonBridge *b = contextMap()->saveFrame(fname);
    if(b)
    {
        b->stdoutFlush();
    }
    else
    {
        qDebug() << "stdout flush without context";
    }
    //Py_END_ALLOW_THREADS
    contextMap()->restoreFrame(b, fname);
    return Py_BuildValue("");
}

PyObject* BridgeStderr_flush(PyObject* self, PyObject* args)
{
    Q_UNUSED(self)
    Q_UNUSED(args)
    //Py_BEGIN_ALLOW_THREADS
    QString fname;
    PythonBridge *b = contextMap()->saveFrame(fname);
    if(b)
    {
        b->stderrFlush();
    }
    else
    {
        qDebug() << "stderr flush without context";
    }
    //Py_END_ALLOW_THREADS
    contextMap()->restoreFrame(b, fname);
    return Py_BuildValue("");
}

PyMethodDef BridgeStdout_methods[] =
{
    {"write", BridgeStdout_write, METH_VARARGS, "sys.stdout.write"},
    {"flush", BridgeStdout_flush, METH_VARARGS, "sys.stdout.flush"},
    {0, 0, 0, 0}
};

PyMethodDef BridgeStderr_methods[] =
{
    {"write", BridgeStderr_write, METH_VARARGS, "sys.stderr.write"},
    {"flush", BridgeStderr_flush, METH_VARARGS, "sys.stderr.flush"},
    {0, 0, 0, 0}
};

static PyTypeObject BridgeStdoutType = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "PyQuikBridgeStdoutType",               /* tp_name */
    sizeof(BridgeStdout),                   /* tp_basicsize */
    0,										/* tp_itemsize */
    0,                                      /* tp_dealloc */
    0,										/* tp_print */
    0,										/* tp_getattr */
    0,										/* tp_setattr */
    0,										/* tp_reserved */
    0,										/* tp_repr */
    0,										/* tp_as_number */
    0,										/* tp_as_sequence */
    0,										/* tp_as_mapping */
    0,										/* tp_hash  */
    0,										/* tp_call */
    0,										/* tp_str */
    0,										/* tp_getattro */
    0,										/* tp_setattro */
    0,										/* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,						/* tp_flags */
    "Bridge Stdout type",                   /* tp_doc */
    0,										/* tp_traverse */
    0,										/* tp_clear */
    0,										/* tp_richcompare */
    0,										/* tp_weaklistoffset */
    0,										/* tp_iter */
    0,										/* tp_iternext */
    BridgeStdout_methods,                   /* tp_methods */
    0,										/* tp_members */
    0,										/* tp_getset */
    0,										/* tp_base */
    0,										/* tp_dict */
    0,										/* tp_descr_get */
    0,										/* tp_descr_set */
    0,										/* tp_dictoffset */
    0,										/* tp_init */
    0,										/* tp_alloc */
    PyType_GenericNew,                      /* tp_new */
};

static PyTypeObject BridgeStderrType = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "PyQuikBridgeStdoutType",               /* tp_name */
    sizeof(BridgeStderr),                   /* tp_basicsize */
    0,										/* tp_itemsize */
    0,                                      /* tp_dealloc */
    0,										/* tp_print */
    0,										/* tp_getattr */
    0,										/* tp_setattr */
    0,										/* tp_reserved */
    0,										/* tp_repr */
    0,										/* tp_as_number */
    0,										/* tp_as_sequence */
    0,										/* tp_as_mapping */
    0,										/* tp_hash  */
    0,										/* tp_call */
    0,										/* tp_str */
    0,										/* tp_getattro */
    0,										/* tp_setattro */
    0,										/* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,						/* tp_flags */
    "Bridge Stderr type",                   /* tp_doc */
    0,										/* tp_traverse */
    0,										/* tp_clear */
    0,										/* tp_richcompare */
    0,										/* tp_weaklistoffset */
    0,										/* tp_iter */
    0,										/* tp_iternext */
    BridgeStderr_methods,                   /* tp_methods */
    0,										/* tp_members */
    0,										/* tp_getset */
    0,										/* tp_base */
    0,										/* tp_dict */
    0,										/* tp_descr_get */
    0,										/* tp_descr_set */
    0,										/* tp_dictoffset */
    0,										/* tp_init */
    0,										/* tp_alloc */
    PyType_GenericNew,                      /* tp_new */
};

struct BridgeModuleObject
{
    PyObject_HEAD
    PythonBridge *bridge;
};

PyObject *variantToPyObject(QVariant val);
QVariant getVariantFromPyObject(PythonBridge *bridge, PyObject *obj);

PythonBridge *pyb_getSelfBridge(PyObject *self)
{
    BridgeModuleObject *bmo = reinterpret_cast<BridgeModuleObject *>(self);
    return bmo->bridge;
}

PyObject *pyb_registerCallback(PyObject *self, PyObject *args)
{
    PythonBridge *b = pyb_getSelfBridge(self);
    char *cbname;
    PyObject *cb = nullptr;
    if (!PyArg_ParseTuple(args,"sO", &cbname, &cb))
        return nullptr;
    QString scbname=QString::fromLocal8Bit(cbname);
    if(!scbname.isEmpty() && cb && PyCallable_Check(cb))
    {
        b->registerPythonCallback(cb, scbname);
    }
    Py_RETURN_NONE;
}

PyObject *pyb_invokeQuik(PyObject *self, PyObject *args)
{
    PythonBridge *b = pyb_getSelfBridge(self);
    char *mname;
    PyObject *listObj, *argItem;
    if(!PyArg_ParseTuple(args, "sO!", &mname, &PyList_Type, &listObj))
        return nullptr;
    int i, cnt = PyList_Size(listObj);
    QVariantList vl, rvl;
    for(i=0;i<cnt;i++)
    {
        argItem = PyList_GetItem(listObj, i);
        vl.append(getVariantFromPyObject(b, argItem));
    }
    //Py_BEGIN_ALLOW_THREADS
    QString fname;
    PythonBridge *ctxb = contextMap()->saveFrame(fname);
    if(ctxb != b)
        qDebug() << "something went wrong!";
    b->invokeQuik(QString::fromLocal8Bit(mname), vl, rvl);
    //Py_END_ALLOW_THREADS
    contextMap()->restoreFrame(ctxb, fname);
    if(rvl.count()>0)
    {
        if(rvl.count()==1)
        {
            return variantToPyObject(rvl.at(0));
        }
        return variantToPyObject(QVariant(rvl));
    }
    Py_RETURN_NONE;
}

PyObject *pyb_invokeQuikObject(PyObject *self, PyObject *args)
{
    PythonBridge *b = pyb_getSelfBridge(self);
    char *mname;
    int objid;
    PyObject *listObj, *argItem;
    if(!PyArg_ParseTuple(args, "isO!", &objid, &mname, &PyList_Type, &listObj))
        return nullptr;
    int i, cnt = PyList_Size(listObj);
    QVariantList vl, rvl;
    for(i=0;i<cnt;i++)
    {
        argItem = PyList_GetItem(listObj, i);
        vl.append(getVariantFromPyObject(b, argItem));
    }
    //Py_BEGIN_ALLOW_THREADS
    QString fname;
    PythonBridge *ctxb = contextMap()->saveFrame(fname);
    if(ctxb != b)
        qDebug() << "something went wrong!";
    b->invokeQuikObject(objid, QString::fromLocal8Bit(mname), vl, rvl);
    //Py_END_ALLOW_THREADS
    contextMap()->restoreFrame(ctxb, fname);
    if(rvl.count()>0)
    {
        if(rvl.count()==1)
            return variantToPyObject(rvl.at(0));
        return variantToPyObject(QVariant(rvl));
    }
    Py_RETURN_NONE;
}

PyObject *pyb_deleteQuikObject(PyObject *self, PyObject *args)
{
    PythonBridge *b = pyb_getSelfBridge(self);
    int objid;
    if(!PyArg_ParseTuple(args, "i", &objid))
        return nullptr;
    //Py_BEGIN_ALLOW_THREADS
    QString fname;
    PythonBridge *ctxb = contextMap()->saveFrame(fname);
    if(ctxb != b)
        qDebug() << "something went wrong!";
    b->deleteQuikObject(objid);
    //Py_END_ALLOW_THREADS
    contextMap()->restoreFrame(ctxb, fname);
    Py_RETURN_NONE;
}

PyObject *pyb_getQuikVariable(PyObject *self, PyObject *args)
{
    PythonBridge *b = pyb_getSelfBridge(self);
    char *vname;
    if(!PyArg_ParseTuple(args, "s", &vname))
        return nullptr;
    QVariant vres;
    //Py_BEGIN_ALLOW_THREADS
    QString fname;
    PythonBridge *ctxb = contextMap()->saveFrame(fname);
    if(ctxb != b)
        qDebug() << "something went wrong!";
    b->getQuikVariable(QString::fromLocal8Bit(vname), vres);
    //Py_END_ALLOW_THREADS
    contextMap()->restoreFrame(ctxb, fname);
    return variantToPyObject(vres);
}

static PyMethodDef b_methods[] = {
    {"registerCallback", pyb_registerCallback, METH_VARARGS, "Registers quik callback function"},
    {"invokeQuik", pyb_invokeQuik, METH_VARARGS, "Invokes quik function"},
    {"invokeQuikObject", pyb_invokeQuikObject, METH_VARARGS, "Invokes quik object function (data source for example)"},
    {"deleteQuikObject", pyb_deleteQuikObject, METH_VARARGS, "deletes allocated before quik object"},
    {"getQuikVariable", pyb_getQuikVariable, METH_VARARGS, "reads global lua variable"},
    {nullptr, nullptr, 0, nullptr}
};

static PyTypeObject PyQuikBridgeModuleType = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "PyQuikBridgeModuleType",               /* tp_name */
    sizeof(BridgeModuleObject),             /* tp_basicsize */
    0,										/* tp_itemsize */
    0,                                      /* tp_dealloc */
    0,										/* tp_print */
    0,										/* tp_getattr */
    0,										/* tp_setattr */
    0,										/* tp_reserved */
    0,										/* tp_repr */
    0,										/* tp_as_number */
    0,										/* tp_as_sequence */
    0,										/* tp_as_mapping */
    0,										/* tp_hash  */
    0,										/* tp_call */
    0,										/* tp_str */
    0,										/* tp_getattro */
    0,										/* tp_setattro */
    0,										/* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,						/* tp_flags */
    "Bridge Module type",                   /* tp_doc */
    0,										/* tp_traverse */
    0,										/* tp_clear */
    0,										/* tp_richcompare */
    0,										/* tp_weaklistoffset */
    0,										/* tp_iter */
    0,										/* tp_iternext */
    b_methods,                              /* tp_methods */
    0,										/* tp_members */
    0,										/* tp_getset */
    0,										/* tp_base */
    0,										/* tp_dict */
    0,										/* tp_descr_get */
    0,										/* tp_descr_set */
    0,										/* tp_dictoffset */
    0,										/* tp_init */
    0,										/* tp_alloc */
    PyType_GenericNew,                      /* tp_new */
};

static PyModuleDef b_module = {
    PyModuleDef_HEAD_INIT,
    "PyQuikBridgeModule", "Python to QUIK bridge module", -1,
    NULL, NULL, NULL, NULL, NULL
};

PyObject* pyBridgeModule=nullptr;
static PyObject *PyInit_bridge(void)
{
    if(PyType_Ready(&BridgeStdoutType) < 0)
    {
        return nullptr;
    }
    if(PyType_Ready(&BridgeStderrType) < 0)
    {
        return nullptr;
    }
    if(PyType_Ready(&PyQuikBridgeModuleType) < 0)
    {
        return nullptr;
    }
    PyObject *mod = PyModule_Create(&b_module);
    if(mod)
    {
        Py_INCREF(&BridgeStdoutType);
        if(PyModule_AddObject(mod, "BridgeStdoutType", reinterpret_cast<PyObject*>(&BridgeStdoutType))<0)
        {
            qDebug() << "Error on adding BridgeStdoutType object";
            Py_XDECREF(mod);
            Py_XDECREF(reinterpret_cast<PyObject*>(&BridgeStdoutType));
        }
        Py_INCREF(&BridgeStderrType);
        if(PyModule_AddObject(mod, "BridgeStderrType", reinterpret_cast<PyObject*>(&BridgeStderrType))<0)
        {
            qDebug() << "Error on adding BridgeStderrType object";
            Py_XDECREF(mod);
            Py_XDECREF(reinterpret_cast<PyObject*>(&BridgeStderrType));
        }
        Py_INCREF(&PyQuikBridgeModuleType);
        if(PyModule_AddObject(mod, "PyQuikBridgeModuleType", reinterpret_cast<PyObject*>(&PyQuikBridgeModuleType))<0)
        {
            qDebug() << "Error on adding PyQuikBridgeModuleType object";
            Py_XDECREF(mod);
            Py_XDECREF(reinterpret_cast<PyObject*>(&PyQuikBridgeModuleType));
        }
        pyBridgeModule=mod;
    }
    return mod;
}

PyObject* b_stdout=nullptr;
PyObject* b_stdout_saved=nullptr;
PyObject* b_stderr=nullptr;
PyObject* b_stderr_saved=nullptr;

void setupStdOutErr()
{
    b_stdout_saved = PySys_GetObject("stdout");
    b_stdout = BridgeStdoutType.tp_new(&BridgeStdoutType, 0, 0);
    PySys_SetObject("stdout", b_stdout);
    b_stderr_saved = PySys_GetObject("stderr");
    b_stderr = BridgeStderrType.tp_new(&BridgeStderrType, 0, 0);
    PySys_SetObject("stderr", b_stderr);
}

PyObject * initModuleObject(PythonBridge *bridge, QString objname)
{
    PyObject *mo = nullptr;
    if(pyBridgeModule)
    {
        BridgeModuleObject *bmo = PyObject_New(BridgeModuleObject, &PyQuikBridgeModuleType);
        bmo->bridge = bridge;
        mo = reinterpret_cast<PyObject*>(bmo);
        PyModule_AddObject(pyBridgeModule, objname.toLocal8Bit().data(), mo);
        PyObject *modules = PyImport_GetModuleDict();
        PyDict_SetItemString(modules, objname.toLocal8Bit().data(), mo);
        Py_XDECREF(mo);
    }
    return mo;
}

bool loadBridgeScript(const QVariantMap &cfg, QByteArray &pycode)
{
    //PyThreadState *mainThreadState = nullptr;
    bool isReady = false;
    PyObject *sys = nullptr;
    PyObject *path = nullptr;
    if(!pyBridgeModule)
    {
        QString venvPackagesPath;
        if(cfg.contains("venvPath"))
        {
            QString fullPythonPath=cfg.value("venvPath").toString();
            if(!fullPythonPath.endsWith(QDir::separator()))
                fullPythonPath+=QDir::separator();
            venvPackagesPath=QString("%1Lib%2site-packages").arg(fullPythonPath).arg(QDir::separator());
            fullPythonPath=QString("%1Scripts%2python.exe").arg(fullPythonPath).arg(QDir::separator());
            if(QFileInfo::exists(fullPythonPath))
            {
                qDebug().noquote().nospace() << "venv path: \"" << venvPackagesPath << "\"";
                qDebug().noquote().nospace() << "pythong path: \"" << fullPythonPath << "\"";
                wchar_t *program = Py_DecodeLocale(fullPythonPath.toLocal8Bit(), nullptr);
                Py_SetProgramName(program);
            }
        }
        PyImport_AppendInittab("PyQuikBridgeModule", &PyInit_bridge);
        Py_Initialize();
        //mainThreadState = PyThreadState_Get();
        //Нужно импортировать модуль до загрузки скрипта чтобы код настройки stdout/err сработал
        PyImport_ImportModule("PyQuikBridgeModule");
        sys = PyImport_ImportModule("sys");
        path = PyObject_GetAttrString(sys, "path");
        if(!venvPackagesPath.isEmpty())
        {
            PyList_Append(path, PyUnicode_FromString(venvPackagesPath.toLocal8Bit().data()));
        }
        setupStdOutErr();
    }
    if(!sys)
        sys = PyImport_ImportModule("sys");
    if(!path)
        path = PyObject_GetAttrString(sys, "path");
    if(cfg.contains("scriptPath"))
    {
        QString scriptPath = cfg.value("scriptPath").toString();
        if(QFileInfo::exists(scriptPath))
        {
            QFileInfo fi(scriptPath);
            //Добавим каталог где лежит сам скрипт в sys.path
            PyList_Append(path,
                          PyUnicode_FromString(
                              fi.absolutePath().toLocal8Bit().data()));
            qDebug().noquote().nospace() << "Script path: \"" << scriptPath << "\"";
            QFile f(scriptPath);
            if(f.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                //Вариант с запускам из файла плох тем, что блокирует файл скрипта
                //Поэтому читаем весь скрипт, закрываем файл и выполняем из памяти
                pycode = f.readAll();
                f.close();
                if(!pycode.isEmpty())
                {
                    isReady=true;
                }
            }
        }
    }
    return isReady;
}

PythonBridge::PythonBridge(const QVariantMap &cfg, QObject *parent)
    : BridgePlugin(cfg, parent), isReady(false)
{
    contextMap()->beginFrame(this, "PythonBridge.load");
    isReady = loadBridgeScript(cfg, pycode);
    contextMap()->endFrame();
    QString objName=QString("qb");
    if(cfg.contains("bridgeModule"))
    {
        objName=cfg.value("bridgeModule").toString();
        qDebug() << "bridge module name:" << objName;
    }
    if(objName.length()>256)
        objName=objName.left(256);
    modObjName=objName;
    contextMap()->beginFrame(this, "PythonBridge.initModule");
    moduleObject = initModuleObject(this, objName);
    contextMap()->endFrame();
}

PythonBridge::~PythonBridge()
{
    QStringList allCbs=registeredCallbacks.keys();
    int i;
    for(i=0;i<allCbs.count();i++)
    {
        PyObject *cb=registeredCallbacks.take(allCbs.at(i));
        contextMap()->beginFrame(this, "PythonBridge.deleteCallback");
        Py_XDECREF(cb);
        contextMap()->endFrame();
    }
}

void PythonBridge::registerPythonCallback(PyObject *cb, QString name)
{
    if(registeredCallbacks.contains(name))
    {
        PyObject *ocb=registeredCallbacks.take(name);
        Py_XDECREF(ocb);
    }
    Py_INCREF(cb);
    //Py_BEGIN_ALLOW_THREADS
    QString fname;
    PythonBridge *ctxb = contextMap()->saveFrame(fname);
    if(ctxb != this)
        qDebug() << "something went wrong!";
    registeredCallbacks.insert(name, cb);
    registerCallback(name);
    //Py_END_ALLOW_THREADS
    contextMap()->restoreFrame(ctxb, fname);
}

size_t PythonBridge::sendToStdout(char *msg)
{
    size_t ret;
    QString smsg = QString::fromLocal8Bit(msg);
    ret = smsg.length();
    stdoutBuffer+=smsg;
    while(!stdoutBuffer.isEmpty() && stdoutBuffer.contains('\n'))
    {
        int pos=stdoutBuffer.indexOf('\n');
        QString partial = stdoutBuffer.left(pos);
        stdoutBuffer.remove(0, pos+1);
        //здесь мы выводим готовую строку
        sendStdoutLine(QString("%1: %2").arg(modObjName).arg(partial));
        //emit stdoutLineOutputRequest(partial);
    }
    return ret;
}

void PythonBridge::stdoutFlush()
{
    sendStdoutLine(QString("%1: %2").arg(modObjName).arg(stdoutBuffer));
    //emit stdoutLineOutputRequest(stdoutBuffer);
    stdoutBuffer.clear();
}

size_t PythonBridge::sendToStderr(char *msg)
{
    size_t ret;
    QString smsg = QString::fromLocal8Bit(msg);
    ret = smsg.length();
    stderrBuffer+=smsg;
    while(!stderrBuffer.isEmpty() && stderrBuffer.contains('\n'))
    {
        int pos=stderrBuffer.indexOf('\n');
        QString partial = stderrBuffer.left(pos);
        stderrBuffer.remove(0, pos+1);
        //здесь мы выводим готовую строку
        sendStderrLine(QString("%1: %2").arg(modObjName).arg(partial));
        //emit stderrLineOutputRequest(partial);
    }
    return ret;
}

void PythonBridge::stderrFlush()
{
    sendStderrLine(QString("%1: %2").arg(modObjName).arg(stderrBuffer));
    //emit stderrLineOutputRequest(stderrBuffer);
    stderrBuffer.clear();
}

void PythonBridge::callbackRequest(QString name, const QVariantList &args, QVariant &vres)
{
    contextMap()->beginFrame(this, QString("callbackRequest(%1)").arg(name));
    //qDebug() << "frame started for " << name;
    if(registeredCallbacks.contains(name))
    {
        PyObject *cb=registeredCallbacks.value(name);
        PyObject *res = nullptr;
        //qDebug() << "callbackRequest" << name << args.count() << PyCallable_Check(cb);
        if(args.count())
        {
            PyObject *tuple = PyTuple_New(args.count());
            for(int li=0;li<args.count();li++)
            {
                QVariant v = args.at(li);
                PyObject *_value = variantToPyObject(v);
                PyTuple_SetItem(tuple, li, _value);
            }
            res = PyObject_CallObject(cb, tuple);
        }
        else
            res = PyObject_CallObject(cb, nullptr);
        if(!res)
            qDebug() << modObjName << ": Fault on callback" << name << "request";
        else
        {
            vres = getVariantFromPyObject(this, res);
            Py_XDECREF(res);
        }
    }
    if(contextMap()->endFrame() != this)
        qDebug() << "wrong restored frame!";
}

void PythonBridge::fastCallbackRequest(BridgeCallableObject cobj, const QVariantList &args, QVariant &vres)
{
    //qDebug() << eventLoopName << ":" << fastCallbackRequest
    contextMap()->beginFrame(this, QString("fastCallbackRequest"));
    PyObject *fcb = reinterpret_cast<PyObject *>(cobj.data);
    if(fcb)
    {
        PyObject *res = nullptr;
        if(args.count())
        {
            PyObject *tuple = PyTuple_New(args.count());
            for(int li=0;li<args.count();li++)
            {
                QVariant v = args.at(li);
                PyObject *_value = variantToPyObject(v);
                PyTuple_SetItem(tuple, li, _value);
            }
            res = PyObject_CallObject(fcb, tuple);
        }
        else
            res = PyObject_CallObject(fcb, nullptr);
        if(!res)
            qDebug() << eventLoopName << ":" << "Fault on fast callback request";
        else
        {
            vres = getVariantFromPyObject(this, res);
            Py_XDECREF(res);
        }
    }
    if(contextMap()->endFrame() != this)
        qDebug() << eventLoopName << ":" << "wrong restored frame!";
}

void PythonBridge::start()
{
    if(isReady)
    {
        contextMap()->beginFrame(this, QString("start"));
        PyRun_SimpleString(pycode.data());
        if(contextMap()->endFrame() != this)
            qDebug() << "wrong restored frame!";
    }
    else
    {
        qDebug() << "Bridge is not ready to start";
    }
}

PyObject *variantToPyObject(QVariant val)
{
    switch(val.type())
    {
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
        return PyLong_FromLongLong(val.toLongLong());
    case QVariant::Bool:
        if(val.toBool())
            return PyBool_FromLong(1);
        return PyBool_FromLong(0);
    case QVariant::String:
    {
        QString sval = val.toString();
        return PyUnicode_FromKindAndData(PyUnicode_2BYTE_KIND, sval.data(), sval.length());
    }
    case QVariant::Double:
        return PyFloat_FromDouble(val.toDouble());
    case QVariant::Map:
    {
        PyObject *dict = PyDict_New();
        QVariantMap tbl = val.toMap();
        QMapIterator<QString, QVariant> ti(tbl);
        while(ti.hasNext())
        {
            ti.next();
            QString k;
            QVariant v;
            k=ti.key();
            v=ti.value();
            PyObject *_value = variantToPyObject(v);
            PyDict_SetItemString(dict, k.toLocal8Bit().data(), _value);
        }
        return dict;
    }
    case QVariant::List:
    {
        QVariantList lst = val.toList();
        PyObject *tuple = PyTuple_New(lst.count());
        for(int li=0;li<lst.count();li++)
        {
            QVariant v = lst.at(li);
            PyObject *_value = variantToPyObject(v);
            PyTuple_SetItem(tuple, li, _value);
        }
        return tuple;
    }
    default:
        if(val.canConvert<QuikCallableObject>())
        {
            QuikCallableObject qco = val.value<QuikCallableObject>();
            return PyLong_FromLongLong(qco.objid);
        }
        break;
    }
    Py_RETURN_NONE;
}

QVariant getVariantFromPyObject(PythonBridge *bridge, PyObject *obj)
{
    if(PyBool_Check(obj))
    {
        if(Py_True == obj)
            return QVariant(true);
        return QVariant(false);
    }
    if(PyFloat_Check(obj))
    {
        return QVariant(PyFloat_AsDouble(obj));
    }
    if(PyLong_Check(obj))
    {
        return QVariant(PyLong_AsLongLong(obj));
    }
    if(PyUnicode_Check(obj))
    {
        PyObject *utf8_item;
        utf8_item = PyUnicode_AsUTF8String(obj);
        if(!utf8_item)
        {
            qDebug() << "Couldn't convert unicode to utf8";
            return QVariant();
        }
        if(strlen(PyBytes_AsString(utf8_item)) == PyUnicode_GET_SIZE(obj))
        {
            return QVariant(QString::fromUtf8(PyBytes_AsString(utf8_item)));
        }
        else
        {
            return QVariant(QString::fromUtf8(PyBytes_AsString(utf8_item), PyUnicode_GET_SIZE(obj)));
        }
    }
    if(PyTuple_Check(obj))
    {
        //qDebug() << "tuple";
        int i, cnt = PyTuple_Size(obj);
        QVariantList vl;
        for(i = 0; i < cnt; ++i)
        {
            PyObject *item = PyTuple_GetItem(obj, i);
            vl.append(getVariantFromPyObject(bridge, item));
        }
        return QVariant(vl);
    }
    if(PyList_Check(obj))
    {
        //qDebug() << "list";
        int i, cnt = PyList_Size(obj);
        QVariantList vl;
        for(i = 0; i < cnt; ++i)
        {
            PyObject *item = PyList_GetItem(obj, i);
            vl.append(getVariantFromPyObject(bridge, item));
        }
        return QVariant(vl);
    }
    if(PyDict_Check(obj))
    {
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        QVariantMap vm;
        while(PyDict_Next(obj, &pos, &key, &value))
        {
            PyObject *_size = PyLong_FromSsize_t(pos);
            long i = PyLong_AsLong(_size);
            Py_XDECREF(_size);
            if (i == -1) break;
            QString skey=getVariantFromPyObject(bridge, key).toString();
            QVariant vval=getVariantFromPyObject(bridge, value);
            vm.insert(skey, vval);
        }
        return QVariant(vm);
    }
    if(PyFunction_Check(obj))
    {
        BridgeCallableObject cobj;
        cobj.ownerPlugin = bridge;
        cobj.data = reinterpret_cast<void *>(obj);
        return QVariant::fromValue(cobj);
    }
    return QVariant();
}
