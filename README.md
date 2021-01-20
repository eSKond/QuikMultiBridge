# QuikMultiBridge

Мост Lua<->Python для написания плагинов для терминала ARQA QUIK на Python. Зачем ещё? Ну, просто данная реализация соответствует моему представлению о прекрасном. Вот некоторые особенности, некоторые уже готовы, некоторые в работе:
- Написан на C++ с использованием библиотеки Qt
- Питон загружается через соответствующие C-шные интерфейсы, то есть интерпретатор становится частью QUIK
- <span style="color:red">НЕ Позволяет добавлять собственный интерфейс 
написанный на Qml/JS - от этой идеи пришлось отказаться. Дело в том, что
интерфес должен создаваться в главном потоке приложения. Windows вроде
позволяет таки создавать интерфес в других потоках, и я это сделал 
(добавил консоль, куда выводился весь вывод print и даже загружал
qml в отдельное окошко), но во-первых всё стало работать очень нестабильно,
во-вторых линковка GUI части кьюта сильно увеличила размер dll, а самое 
главное - у меня заканчивается отпуск и времени на отлавливание багов
и нахождении обходных путей в ограничении кьюта на gui в неглавном потоке...
не так уж оно и нужно, честно говоря</span>
- <span style="color:red">НЕ Имеет встроенную консоль в которую выводится весь вывод из print - см. выше</span>
- Подгружает Python-овскую venv
- !!! Собирается единожды, после чего не требует пересборки при изменениях АПИ, что достигается полностью динамическим разбором параметров и возвращаемых значений
- Позволяет писать как индикаторы, так и роботов

В принципе, всё работает. Выжимать что-то ещё из питона не буду, для себя я скорее буду писать сервер на C++ под мои нужды. 
С питоном есть проблемы с numpy - сам numpy неправильно выгружается, поэтому если скрипт использует его, то после остановки 
запустить его снова не удастся, нужно полностью перезапускать quik. 
А раз есть проблемы с numpy, то скорее всего и pandas будет глючить - надо пробовать.
Это тоже одна из причин остановиться на этом - без numpy и pandas как-то уныло.
Кстати matplotlib открывает графики без проблем, даже интерактивные.

## Как писать скрипт
1. Инициализация бриджа из lua
```lua
require "QuikMultiBridge"

bridgeConfig = {
    venvPath="c:\\Work\\QuikMultiBridge\\PythonQuik\\tstvenv",
    bridgeModule="qbridge",
    scriptPath="c:\\Work\\QuikMultiBridge\\PythonQuik\\pyRobo.py",
    eventLoopName="main"
    };

initBridge("Python", bridgeConfig);
```
Что здесь происходит:
- подключаем dll (можно переименовать её, но смысл?)
- готовим конфиг, в котором указываем путь к venv, имя модуля, как мы к нему будем обращаться из Python, путь к скрипту, lua метод, который будет у нас основным потоком (об этом ниже)
- инициализируем бридж указывая что нужно загрузить плагин Python (там заложено несколько вариантов включая R, Qt remote objects, Qt server, но пока есть только Python) и передаём ему конфиг.
2. Открываем документацию от ARQA "Интерпретатор Lua" и пишем скрипт на Python как если бы мы писали его на Lua с небольшими отличиями:
- main реализует сам бридж (имя мы указали в конфигурации), потому-что нам нужен этот цикл как диспетчер событий, где можно рисовать интерфейс, принимать события от системы и тд. Сам же eventLoop в бридже будет вызывать специальную функцию на каждом цикле из питона.
- для вызова методов quik мы используем метод бриджа invokeQuik

Но, лучше, давайте посмотрим пример скрипта Python:
```python
import qbridge
from datetime import datetime


ds = None
dsSize = None
dsWasChecked = False
quitRequest = False
updateOutEnabled = True


def dsUpdateCallback(candleIndex):
    global ds, updateOutEnabled
    if updateOutEnabled is True:
        print("C({:d}): {:.2f}".
              format(candleIndex,
                     qbridge.invokeQuikObject(ds, "C", [candleIndex])))
        updateOutEnabled = False


def processBridgeEvents():
    global ds, dsSize, dsWasChecked, quitRequest, updateOutEnabled
    print(datetime.now().strftime("%Y.%m.%d %H:%M:%S"))
    if ds is None and dsWasChecked is False:
        res = qbridge.invokeQuik("CreateDataSource", ["TQBR", "SBER", 1])
        print("data source created: {:d}".format(res))
        if res is not None:
            ds = res
            dsSize = qbridge.invokeQuikObject(ds, "Size", [])
            qbridge.invokeQuikObject(ds, "SetUpdateCallback",
                                     [dsUpdateCallback])
            print("size of DS:{:d}".format(dsSize))
            dsWasChecked = True
    if dsSize is not None:
        if dsSize > 0:
            dsSize -= 1
    updateOutEnabled = True
    if quitRequest is True:
        # закрываем
        qbridge.invokeQuikObject(ds, "Close", [])
        # удаляем
        qbridge.deleteQuikObject(ds)
        # очищаем
        ds = None
        dsSize = None
        qbridge.quitBridge()
    else:
        qbridge.invokeQuik("sleep", [1000])


def OnStop(flg):
    global quitRequest
    if flg == 1:
        print("Stopped from dialog")
    else:
        print("Stopped on exit")
    quitRequest = True
    return 10000


if __name__ == '__main__':
    print("Hello from python!")
    qbridge.registerCallback('OnStop', OnStop)
    print("Callback OnStop registered")
    qbridge.registerProcessEventsCallback(processBridgeEvents)
    print("ProcessEventsCallback registered")

```
import qbridge - тут мы импортируем модуль бриджа с именем, который мы ему задали в конфигурации

Дальше инициализируем глобальные переменные:
- ds: наш DataSource
- dsSize: размер ds. Это пример, необходимость его в глобальном пространстве обуславливается логикой робота
- quitRequest: это сигнал на выход. Вообще для этого есть отдельный метод но его лучше вызывать из processEvents (см. ниже)
- updateOutEnabled: ну такой вот пример, кому не нравится - напишите лучше :)

Далее определяем колбек на обновление нашего запрошенного ниже источника данных. Ну, логику его работы
предоставлю возможность разобрать читателю

Метод processBridgeEvents по сути является единичным шагом цикла main, 
как вы бы его писали на lua. В нём мы выводим дату/время и затем начинается
магия. Если источник данных ещё не создан, то создаём его
вызовом 

qbridge.invokeQuik("CreateDataSource", ["TQBR", "SBER", 1])

qbridge.invokeQuikObject(ds, "Size", []) - получаем размер
и затем устанавливаем колбек на обновление:

qbridge.invokeQuikObject(ds, "SetUpdateCallback", [dsUpdateCallback])

В этом же методе можно увидеть пример вызова метода Close через invokeQuikObject и удаления
объекта с deleteQuikObject

Ну и там же завершение работы робота и бриджа:

qbridge.quitBridge()

Как видите, мы не закладываемся на сигнатуру вызова, вместо этого разработчик 
сам смотрит что и как передавать по документации. 
Параметры всегда передаются как массив (list в терминах Python), 
даже если в массиве один элемент. То, что в lua называется таблицами 
передаётся как dict

Функция OnStop ничем не примечательна, кроме способа завершения скрипта 
установкой переменной quitRequest в True. Так сделано, чтобы закрытие
бриджа происходило в main (имеется в виду main из документации quik)
В принципе я постарался исключить нежелательные эффекты вызова
quitBridge вне main, но переписывать скрипт лень.
Вообще вызов quitBridge завершает eventLoop и подчищает всё за собой

В блоке if (стандартная фишка Python, но в принципе можно и без if) мы делаем некоторую 
подготовительную работу:
- регистрируем колбэки (в данном случае один - OnStop)
- регистрируем специальный колбэк который вызывается из eventLoop - то есть шаг цикла main

То есть бридж предоставляет только 7 методов:
- registerCallback
- registerProcessEventsCallback
- invokeQuik
- invokeQuikObject
- deleteQuikObject
- getQuikVariable
- quitBridge

В принципе, мы уже так или иначе рассмотрели все методы, кроме getQuikVariable.
Этот метод позволяет прочитать (в одну сторону из луа в питон) объект из 
инициализирующего скрипта lua. Это нужно, в первую очередь, для написания индикаторов на
питоне, потому-что им нужна таблица Settings определенного формата
Я решил не заморачиваться - раз уж есть инициализирующий lua скрипт, то пусть
и Settings будут там же, а из питона мы их прочитаем с помощью getQuikVariable

Ну и давайте посмотрим пример реализации MA на питоне:

Инициализирующий Lua скрипт:
```lua
require "QuikMultiBridge"

Settings = {
    Name = "SimPyMA",
    mode = "C",
    period = 5,
    line = { {
        Name = "Python Moving Average",
        Color = RGB(90, 110, 200),
        Type = TYPE_LINE,
        Width = 1
        }
    }
};

bridgeConfig = {
    venvPath="c:\\Work\\QuikMultiBridge\\PythonQuik\\tstvenv",
    bridgeModule="qbridge",
    scriptPath="c:\\Work\\QuikMultiBridge\\PythonQuik\\pyIndicator.py"
    };

initBridge("Python", bridgeConfig);
```
В общем всё просто, только обратите внимание - здесь не нужно определять
eventLoopName

Теперь сам индикатор на питоне:
```python
import qbridge


funName = 'C'
arr = []
arrsum = 0
period = 10


def Init():
    print("Init")
    return 1


def OnCalculate(idx):
    global funName, arr, period, arrsum
    print("OnCalculate")
    cexist = qbridge.invokeQuik("CandleExist", [idx])
    if cexist is True:
        p = qbridge.invokeQuik(funName, [idx])
        arr.append(p)
        arrsum += p
        if len(arr) > period:
            arrsum -= arr.pop(0)
        return arrsum/len(arr)
    else:
        return None


if __name__ == '__main__':
    settings = qbridge.getQuikVariable("Settings")
    if settings is not None:
        funName = settings["mode"]
        period = settings["period"]
    print("Register callbacks")
    qbridge.registerCallback('Init', Init)
    qbridge.registerCallback('OnCalculate', OnCalculate)
```
Собственно всё в соответствии с документацией Lua, только, пожалуй,
ссылка на цену из свечки получилась даже изящней чем то, монстроидное, что
они предлагают в документации. Ну и пример использование getQuikVariable
в наличии

## Некоторые неочевидные моменты
1. Можно исключить встроенный eventLoop просто удалив строку
```lua
eventLoopName="main"
```
из конфигурации бриджа, так же как мы это делаем для индикатора
И после этого можно вызовом registerCallback зарегистрировать собственный 
main полностью реализующий главный цикл робота в питоне

2. Можно таки изменять Settings индикатора из питона.
Для этого надо написать в инициализирующем скрипте Lua что-то типа:
```lua
Settings = {}
function setSettings(sets)
    Settings = sets
end
```
и затем в питоне:
```python
settings = {'Name': 'SimPyMA', 'mode': 'C', 'period': 5,
    'line': [
        {'Name': 'Python Moving Average', 
         'Color': int("{:02x}{:02x}{:02x}".format(90,110,200), 16),
         'Type': 1,
         'Width': 1
        }
    ]}
qbridge.invokeQuik("setSettings", [settings])
```
То есть в принципе такой финт можно провернуть с любой функцией инициализирующего 
скрипта. Возможно какие-то вещи проще будет сделать на Lua

3. Ещё одна фича, полезность которой я разглядеть не могу, но вдруг?
Если вы зарегистрировали какой-то коллбек при помощи registerCallback, то
можете смело его вызывать с invokeQuik. Назовём эту фичу "back callback"

4. Запускать одновременно индикатор и скрипт, два индикатора, два скрипта
не получится - там бы нужно создавать полностью собственное пространство и 
я этого не делал. Возможно когда-нибудь сделаю, но скорее всего нет.

## Ещё немного про бинарник.
Я собирал с помощью статической компоновки как кьюта, 
так и VC runtime, поэтому единственные несистемные зависимости там - LUA53.dll -
должен лежать уже в папке квика (туда же и бинарник нужно кинуть), и вторая зависимость -
python39.dll - скорее всего при установке питона вы добавили его в PATH, поэтому
он доступен. Заметьте, что venv он грузит уже загрузив python39.dll, поэтому он 
обязательно должен быть доступен как-то - хоть весь его сгрузите в каталог квика

