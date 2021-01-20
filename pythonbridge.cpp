#include "pythonbridge.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <stdio.h>

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
    if(!PythonBridge::getBridge())
        return PyLong_FromSize_t(0);
    written = PythonBridge::getBridge()->sendToStdout(data);
    return PyLong_FromSize_t(written);
}

PyObject* BridgeStderr_write(PyObject* self, PyObject* args)
{
    Q_UNUSED(self)
    std::size_t written = 0;
    char* data;
    if (!PyArg_ParseTuple(args, "s", &data))
        return nullptr;
    if(!PythonBridge::getBridge())
        return PyLong_FromSize_t(0);
    written = PythonBridge::getBridge()->sendToStderr(data);
    return PyLong_FromSize_t(written);
}

PyObject* BridgeStdout_flush(PyObject* self, PyObject* args)
{
    Q_UNUSED(self)
    Q_UNUSED(args)
    if(PythonBridge::getBridge())
        PythonBridge::getBridge()->stdoutFlush();
    return Py_BuildValue("");
}

PyObject* BridgeStderr_flush(PyObject* self, PyObject* args)
{
    Q_UNUSED(self)
    Q_UNUSED(args)
    if(PythonBridge::getBridge())
        PythonBridge::getBridge()->stderrFlush();
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

static char b_stdout_type_name[277];
static char b_stderr_type_name[277];
static PyTypeObject BridgeStdoutType = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    b_stdout_type_name,                     /* tp_name */
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
    b_stderr_type_name,                     /* tp_name */
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

PythonBridge *PythonBridge::singletonPythonBridge = nullptr;
static PyMethodDef b_methods[] = {
    {"registerCallback", nullptr, METH_VARARGS, "Registers quik callback function"},
    {"invokeQuik", nullptr, METH_VARARGS, "Invokes quik function"},
    {"registerProcessEventsCallback", nullptr, METH_VARARGS, "Registers process events function"},
    {"quitBridge", nullptr, METH_VARARGS, "quits bridge event loop"},
    {"invokeQuikObject", nullptr, METH_VARARGS, "Invokes quik object function (data source for example)"},
    {"deleteQuikObject", nullptr, METH_VARARGS, "deletes allocated before quik object"},
    {"getQuikVariable", nullptr, METH_VARARGS, "reads global lua variable"},
    {nullptr, nullptr, 0, nullptr}
};

PyObject* b_stdout=nullptr;
PyObject* b_stdout_saved=nullptr;
PyObject* b_stderr=nullptr;
PyObject* b_stderr_saved=nullptr;

static char b_module_name[257];
static PyModuleDef b_module = {
    PyModuleDef_HEAD_INIT,
    b_module_name, "Python to QUIK bridge module", -1, b_methods,
    NULL, NULL, NULL, NULL
};
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
    PyObject *mod = PyModule_Create(&b_module);
    if(mod)
    {
        Py_INCREF(&BridgeStdoutType);
        if(PyModule_AddObject(mod, "BridgeStdout", reinterpret_cast<PyObject*>(&BridgeStdoutType))<0)
        {
            qDebug() << "Error on adding BridgeStdout object";
            Py_DECREF(mod);
            Py_DECREF(reinterpret_cast<PyObject*>(&BridgeStdoutType));
        }
        Py_INCREF(&BridgeStderrType);
        if(PyModule_AddObject(mod, "BridgeStderr", reinterpret_cast<PyObject*>(&BridgeStderrType))<0)
        {
            qDebug() << "Error on adding BridgeStderr object";
            Py_DECREF(mod);
            Py_DECREF(reinterpret_cast<PyObject*>(&BridgeStderrType));
        }
    }
    return mod;
}

PythonBridge::~PythonBridge()
{
    QStringList allCbs=registeredCallbacks.keys();
    int i;
    for(i=0;i<allCbs.count();i++)
    {
        PyObject *cb=registeredCallbacks.take(allCbs.at(i));
        Py_XDECREF(cb);
    }
    Py_Finalize();
}

PythonBridge *PythonBridge::getBridge(const QVariantMap &cfg, QObject *parent)
{
    if(!singletonPythonBridge)
        new PythonBridge(cfg, parent);
    return singletonPythonBridge;
}

PythonBridge *PythonBridge::getBridge()
{
    return singletonPythonBridge;
}

size_t PythonBridge::sendToStdout(char *msg)
{
    QString smsg = QString::fromLocal8Bit(msg);
    size_t ret = smsg.length();
    stdoutBuffer+=smsg;
    while(!stdoutBuffer.isEmpty() && stdoutBuffer.contains('\n'))
    {
        int pos=stdoutBuffer.indexOf('\n');
        QString partial = stdoutBuffer.left(pos);
        stdoutBuffer.remove(0, pos+1);
        //здесь мы выводим готовую строку
        qDebug().noquote() << partial;
        //emit stdoutLineOutputRequest(partial);
    }
    return ret;
}

void PythonBridge::stdoutFlush()
{
    qDebug().noquote() << stdoutBuffer;
    //emit stdoutLineOutputRequest(stdoutBuffer);
    stdoutBuffer.clear();
}

size_t PythonBridge::sendToStderr(char *msg)
{
    QString smsg = QString::fromLocal8Bit(msg);
    size_t ret = smsg.length();
    stderrBuffer+=smsg;
    while(!stderrBuffer.isEmpty() && stderrBuffer.contains('\n'))
    {
        int pos=stderrBuffer.indexOf('\n');
        QString partial = stderrBuffer.left(pos);
        stderrBuffer.remove(0, pos+1);
        //здесь мы выводим готовую строку
        qDebug().noquote() << partial;
        //emit stderrLineOutputRequest(partial);
    }
    return ret;
}

void PythonBridge::stderrFlush()
{
    qDebug().noquote() << stderrBuffer;
    //emit stderrLineOutputRequest(stderrBuffer);
    stderrBuffer.clear();
}

PyObject *variantToPyObject(QVariant val);
QVariant getVariantFromPyObject(PyObject *obj);
void PythonBridge::callbackRequest(QString name, const QVariantList &args, QVariant &vres)
{
    if(registeredCallbacks.contains(name))
    {
        PyObject *cb=registeredCallbacks.value(name);
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
            res = PyObject_CallObject(cb, tuple);
        }
        else
            res = PyObject_CallObject(cb, nullptr);
        if(!res)
            qDebug() << "Fault on callback" << name << "request";
        else
        {
            vres = getVariantFromPyObject(res);
            Py_XDECREF(res);
        }
    }
}

void PythonBridge::fastCallbackRequest(BridgeCallableObject cobj, const QVariantList &args, QVariant &vres)
{
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
            qDebug() << "Fault on fast callback request";
        else
        {
            vres = getVariantFromPyObject(res);
            Py_XDECREF(res);
        }
    }
}

void PythonBridge::processEventsInBridge()
{
    if(procEventCallback)
    {
        static bool showFault=true;
        PyObject *res = nullptr;
        res = PyObject_CallObject(procEventCallback, nullptr);
        if(!res)
        {
            if(showFault)
            {
                qDebug() << "Fault in processEventsInBridge";
                showFault=false;
            }
        }
        else
        {
            showFault=true;
            Py_XDECREF(res);
        }
    }
    if(exitRequested)
    {
        emit exitBridgeSignal();
    }
}

PythonBridge::PythonBridge(const QVariantMap &cfg, QObject *parent)
    : BridgePlugin(cfg, parent), isReady(false), procEventCallback(nullptr), exitRequested(false)//, refcnt(1)
{
    singletonPythonBridge=this;
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
    b_methods[0].ml_meth = PythonBridge::b_registerCallback;
    b_methods[1].ml_meth = PythonBridge::b_invokeQuik;
    b_methods[2].ml_meth = PythonBridge::b_registerProcessEventsCallback;
    b_methods[3].ml_meth = PythonBridge::b_quitBridge;
    b_methods[4].ml_meth = PythonBridge::b_invokeQuikObject;
    b_methods[5].ml_meth = PythonBridge::b_deleteQuikObject;
    b_methods[6].ml_meth = PythonBridge::b_getQuikVariable;
    QString objName=QString("qb");
    if(cfg.contains("bridgeModule"))
    {
        objName=cfg.value("bridgeModule").toString();
        qDebug() << "bridge module name:" << objName;
    }
    if(objName.length()>256)
        objName=objName.left(256);
    qstrncpy(b_module_name, objName.toLocal8Bit().data(), objName.length()+1);
    b_module_name[objName.length()]=0;
    QString stdoutTypeName = QString("%1.BridgeStdoutType").arg(objName);
    qstrncpy(b_stdout_type_name, stdoutTypeName.toLocal8Bit().data(), stdoutTypeName.length()+1);
    b_stdout_type_name[stdoutTypeName.length()]=0;
    QString stderrTypeName = QString("%1.BridgeStderrType").arg(objName);
    qstrncpy(b_stderr_type_name, stderrTypeName.toLocal8Bit().data(), stderrTypeName.length()+1);
    b_stderr_type_name[stderrTypeName.length()]=0;
    PyImport_AppendInittab(b_module_name, &PyInit_bridge);
    Py_Initialize();
    //Нужно импортировать модуль до загрузки скрипта чтобы код настройки stdout/err сработал
    PyImport_ImportModule(b_module_name);
    PyObject *sys = PyImport_ImportModule("sys");
    PyObject *path = PyObject_GetAttrString(sys, "path");
    if(!venvPackagesPath.isEmpty())
    {
        PyList_Append(path, PyUnicode_FromString(venvPackagesPath.toLocal8Bit().data()));
    }
    b_stdout_saved = PySys_GetObject("stdout");
    b_stdout = BridgeStdoutType.tp_new(&BridgeStdoutType, 0, 0);
    PySys_SetObject("stdout", b_stdout);
    b_stderr_saved = PySys_GetObject("stderr");
    b_stderr = BridgeStderrType.tp_new(&BridgeStderrType, 0, 0);
    PySys_SetObject("stderr", b_stderr);
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
}

void PythonBridge::start()
{
    if(isReady)
    {
        PyRun_SimpleString(pycode.data());
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
        break;
    }
    Py_RETURN_NONE;
}

QVariant getVariantFromPyObject(PyObject *obj)
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
    if(PyList_Check(obj))
    {
        int i, cnt = PyList_Size(obj);
        QVariantList vl;
        for(i = 0; i < cnt; ++i)
        {
            PyObject *item = PyList_GetItem(obj, i);
            vl.append(getVariantFromPyObject(item));
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
            QString skey=getVariantFromPyObject(key).toString();
            QVariant vval=getVariantFromPyObject(value);
            vm.insert(skey, vval);
        }
        return QVariant(vm);
    }
    if(PyFunction_Check(obj))
    {
        BridgeCallableObject cobj;
        cobj.data = reinterpret_cast<void *>(obj);
        return QVariant::fromValue(cobj);
    }
    return QVariant();
}

PyObject *PythonBridge::b_invokeQuikObject(PyObject *self, PyObject *args)
{

    Q_UNUSED(self)
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
        vl.append(getVariantFromPyObject(argItem));
    }
    invokeQuikObject(objid, QString::fromLocal8Bit(mname), vl, rvl);
    if(rvl.count()>0)
    {
        if(rvl.count()==1)
            return variantToPyObject(rvl.at(0));
        return variantToPyObject(QVariant(rvl));
    }
    Py_RETURN_NONE;
}

PyObject *PythonBridge::b_deleteQuikObject(PyObject *self, PyObject *args)
{
    Q_UNUSED(self)
    int objid;
    if(!PyArg_ParseTuple(args, "i", &objid))
        return nullptr;
    deleteQuikObject(objid);
    Py_RETURN_NONE;
}

PyObject *PythonBridge::b_getQuikVariable(PyObject *self, PyObject *args)
{
    Q_UNUSED(self)
    char *vname;
    if(!PyArg_ParseTuple(args, "s", &vname))
        return nullptr;
    QVariant vres;
    getQuikVariable(QString::fromLocal8Bit(vname), vres);
    return variantToPyObject(vres);
}

PyObject *PythonBridge::b_invokeQuik(PyObject *self, PyObject *args)
{
    Q_UNUSED(self)
    char *mname;
    PyObject *listObj, *argItem;
    if(!PyArg_ParseTuple(args, "sO!", &mname, &PyList_Type, &listObj))
        return nullptr;
    int i, cnt = PyList_Size(listObj);
    QVariantList vl, rvl;
    for(i=0;i<cnt;i++)
    {
        argItem = PyList_GetItem(listObj, i);
        vl.append(getVariantFromPyObject(argItem));
    }
    invokeQuik(QString::fromLocal8Bit(mname), vl, rvl);
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

PyObject *PythonBridge::b_registerCallback(PyObject *self, PyObject *args)
{
    Q_UNUSED(self)
    char *cbname;
    PyObject *cb = nullptr;
    if (!PyArg_ParseTuple(args,"sO", &cbname, &cb))
        return nullptr;
    QString scbname=QString::fromLocal8Bit(cbname);
    if(!scbname.isEmpty() && cb && PyCallable_Check(cb))
    {
        if(singletonPythonBridge)
        {
            if(singletonPythonBridge->registeredCallbacks.contains(scbname))
            {
                PyObject *ocb=singletonPythonBridge->registeredCallbacks.take(scbname);
                Py_XDECREF(ocb);
            }
            Py_INCREF(cb);
            singletonPythonBridge->registeredCallbacks.insert(scbname, cb);
        }
        registerCallback(scbname);
    }

    Py_RETURN_NONE;
}

PyObject *PythonBridge::b_registerProcessEventsCallback(PyObject *self, PyObject *args)
{
    Q_UNUSED(self)
    PyObject *cb = nullptr;
    if (!PyArg_ParseTuple(args,"O", &cb))
        return nullptr;
    if(cb && PyCallable_Check(cb))
    {
        if(singletonPythonBridge)
        {
            if(singletonPythonBridge->procEventCallback)
            {
                qDebug() << "event dispatcher already registered!";
                PyObject *ocb = singletonPythonBridge->procEventCallback;
                singletonPythonBridge->procEventCallback=nullptr;
                Py_XDECREF(ocb);
            }
            singletonPythonBridge->exitRequested=false;
            Py_INCREF(cb);
            singletonPythonBridge->procEventCallback=cb;
        }
    }

    Py_RETURN_NONE;
}

PyObject *PythonBridge::b_quitBridge(PyObject *self, PyObject *args)
{
    Q_UNUSED(self)
    Q_UNUSED(args)
    if(singletonPythonBridge)
    {
        //Нам нужно дождаться завершения processEventsCallback и только тогда завершать eventLoop
        singletonPythonBridge->exitRequested=true;
    }
    Py_RETURN_NONE;
}
