QT -= gui
QT += core

TEMPLATE = lib
DEFINES += QUIKMULTIBRIDGE_LIBRARY

# QMAKE_EXTENSION_SHLIB = dll
CONFIG += c++11
CONFIG += plugin no_plugin_name_prefix
CONFIG += dll

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# lua
# INCLUDEPATH += c:\Work\lua-5.4.2_Win64_dll16_lib\include
INCLUDEPATH += c:\Work\lua-5.3.6_Win64_dll16_lib\include
# /usr/local/include
# LIBS += -Lc:\Work\lua-5.4.2_Win64_dll16_lib\fromquik -llua54
LIBS += -Lc:\Work\lua-5.3.6_Win64_dll16_lib\fromquik -llua53
# -L/usr/local/lib -llua

# python
INCLUDEPATH += c:\Users\sergey\AppData\Local\Programs\Python\Python39\include
# /usr/local/Cellar/python@3.9/3.9.1_4/Frameworks/Python.framework/Versions/3.9/include/python3.9
CONFIG(release, debug|release): LIBS += -Lc:\Users\sergey\AppData\Local\Programs\Python\Python39\libs -lpython39
CONFIG(debug, debug|release): LIBS += -Lc:\Users\sergey\AppData\Local\Programs\Python\Python39\libs -lpython39
# -L/usr/local/opt/python@3.9/Frameworks/Python.framework/Versions/3.9/lib -lpython3.9 -ldl -framework CoreFoundation

SOURCES += \
    bridgeplugin.cpp \
    pythonbridge.cpp \
    quikmultibridge.cpp

HEADERS += \
    QuikMultiBridge_global.h \
    bridgeplugin.h \
    pythonbridge.h \
    quikmultibridge.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    README.md

FORMS +=
