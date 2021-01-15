#include "pythonbridge.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <stdio.h>

PythonBridge *PythonBridge::singletonPythonBridge = nullptr;
static PyMethodDef b_methods[] = {
    {"registerCallback", nullptr, METH_VARARGS, "Registers quik callback function"},
    {"invokeQuik", nullptr, METH_VARARGS, "Invokes quik function"},
    {"registerProcessEventsCallback", nullptr, METH_VARARGS, "Registers process events function"},
    {"quitBridge", nullptr, METH_VARARGS, "quits bridge event loop"},
    {nullptr, nullptr, 0, nullptr}
};
static char b_module_name[257];
static PyModuleDef b_module = {
    PyModuleDef_HEAD_INIT, b_module_name, "Python to QUIK bridge module", -1, b_methods,
    NULL, NULL, NULL, NULL
};
static PyObject *PyInit_bridge(void)
{
    return PyModule_Create(&b_module);
}

PythonBridge::~PythonBridge()
{
    qDebug() << "~PythonBridge()";
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
            qDebug() << "Fault on callback request";
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
                //Py_DECREF(_value);
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
        PyObject *res = nullptr;
        res = PyObject_CallObject(procEventCallback, nullptr);
        if(!res)
            qDebug() << "Fault on fast callback request";
        else
        {
            Py_XDECREF(res);
        }
    }
}

PythonBridge::PythonBridge(const QVariantMap &cfg, QObject *parent)
    : BridgePlugin(cfg, parent), procEventCallback(nullptr)
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
                //QString("%1bin%2python").arg(fullPythonPath).arg(QDir::separator());
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
    PyImport_AppendInittab(b_module_name, &PyInit_bridge);
    Py_Initialize();
    if(!venvPackagesPath.isEmpty())
    {
        PyObject *sys = PyImport_ImportModule("sys");
        PyObject *path = PyObject_GetAttrString(sys, "path");
        PyList_Append(path, PyUnicode_FromString(venvPackagesPath.toLocal8Bit().data()));
    }
    //PyObject *pName, *pModule, *pFunc;
    if(cfg.contains("scriptPath"))
    {
        QString scriptPath = cfg.value("scriptPath").toString();
        if(QFileInfo::exists(scriptPath))
        {
            qDebug().noquote().nospace() << "Script path: \"" << scriptPath << "\"";
            //pName = PyUnicode_DecodeFSDefault(scriptPath.toLocal8Bit().data());
            //pModule = PyImport_Import(pName);
            //Py_DECREF(pName);
            QFile f(scriptPath);
            if(f.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                pycode = f.readAll();
                f.close();
                qDebug() << "Python file closed";
                if(!pycode.isEmpty())
                {
                    qDebug() << "script loaded, lets start it";
                    PyRun_SimpleString(pycode.data());
                    qDebug() << "script started";
                }
            }
            /*
            FILE *scriptFile = _Py_fopen(scriptPath.toLocal8Bit().data(), "rb");//fopen(scriptPath.toLocal8Bit().data(), "rb");
            if(scriptFile)
            {
                qDebug() << "Start python script...";
                PyRun_SimpleFile(scriptFile, scriptPath.toLocal8Bit().data());
                qDebug() << "Python script started";
                //fclose(scriptFile);
            }
            */
        }
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
            //Py_DECREF(_value);
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
            //Py_DECREF(_value);
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
    qDebug() << "getVariantFromPyObject:";
    if(PyBool_Check(obj))
    {
        qDebug() << "    bool";
        if(Py_True == obj)
            return QVariant(true);
        return QVariant(false);
    }
    if(PyFloat_Check(obj))
    {
        qDebug() << "    float";
        return QVariant(PyFloat_AsDouble(obj));
    }
    if(PyLong_Check(obj))
    {
        qDebug() << "    longlong";
        return QVariant(PyLong_AsLongLong(obj));
    }
    /*
    if(PyString_Check(obj))
    {
        if(qstrlen(PyString_AsString(obj)) == PyString_GET_SIZE(obj))
        {
            return QVariant(QString::fromLocal8Bit(PyString_AsString(obj)));
        }
        else
        {
            return QVariant(QString::fromLocal8Bit(PyString_AsString(obj), PyString_GET_SIZE(obj)));
        }
    }
    */
    if(PyUnicode_Check(obj))
    {
        qDebug() << "    unicode";
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
        qDebug() << "    list";
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
        qDebug() << "    dict";
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
        qDebug() << "    function";
        BridgeCallableObject cobj;
        cobj.data = reinterpret_cast<void *>(obj);
        return QVariant::fromValue(cobj);
    }
    qDebug() << "    nil";
    return QVariant();
}

PyObject *PythonBridge::b_invokeQuik(PyObject *self, PyObject *args)
{
    qDebug() << "b_invokeQuik";
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
            return variantToPyObject(rvl.at(0));
        return variantToPyObject(QVariant(rvl));
    }
    Py_RETURN_NONE;
}

PyObject *PythonBridge::b_registerCallback(PyObject *self, PyObject *args)
{
    qDebug() << "b_registerCallback";
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
    qDebug() << "b_registerProcessEventsCallback";
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
            Py_INCREF(cb);
            singletonPythonBridge->procEventCallback=cb;
        }
    }

    Py_RETURN_NONE;
}

PyObject *PythonBridge::b_quitBridge(PyObject *self, PyObject *args)
{
    qDebug() << "b_quitBridge";
    Q_UNUSED(self)
    Q_UNUSED(args)
    if(singletonPythonBridge)
    {
        emit singletonPythonBridge->exitBridgeSignal();
    }
    Py_RETURN_NONE;
}
