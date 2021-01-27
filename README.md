# QuikMultiBridge

Мост Lua<->Python для написания плагинов для терминала ARQA QUIK на Python. Зачем ещё? Ну, просто данная реализация соответствует моему представлению о прекрасном. Вот некоторые особенности, некоторые уже готовы, некоторые в работе:
- Написан на C++ с использованием библиотеки Qt
- Питон загружается через соответствующие C-шные интерфейсы, то есть интерпретатор становится частью QUIK
- Таки позволяет добавлять собственный интерфейс, но я этим пока не занимался. Но если есть желание можете
реализовать сами, главное помните, что окна должны создаваться внутри qmbMain после создания QCoreApplication.
Ну и интерфейс всё-же желательно писать на кьюте. Впрочем если в питоновской части откроете matplotlib да или 
тот же PyQt - тоже пожалуйста. Но я бы на устойчивость при этом не ставил.
- Не имеет встроенную консоль в которую выводится весь вывод из print - мне это просто не нужно, я вполне уже
пообвыкся с DebugView. Но добавить - не проблема, см пункт выше
- Подгружает Python-овскую venv
- !!! Собирается единожды, после чего не требует пересборки при изменениях АПИ, что достигается полностью динамическим разбором параметров и возвращаемых значений
- Позволяет писать как индикаторы, так и роботов. Да, вот здесь пришлось повозиться - одновременно несколько
роботов да ещё и индикаторы никак не хотели работать. Питоновский GIL выкинул на помойку, сделал собственное
переключение между тредами мьютексом. Ниже опишу некоторые важные детали.

В принципе, всё работает. Выжимать что-то ещё из питона не буду, для себя я скорее буду писать сервер на C++ под мои нужды. 
С numpy и pandas проблем больше нет - удалось решить. Утечек тоже не наблюдаю. Можно пользоваться.

## Как писать скрипт робота
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
- подключаем dll (можно переименовать её, но там нужно тогда немного иначе загружать, так что смысла не вижу)
- готовим конфиг, в котором указываем путь к venv, имя модуля, как мы к нему будем обращаться из Python, путь к скрипту, lua метод, который будет у нас основным потоком (об этом ниже)
- инициализируем бридж указывая что нужно загрузить плагин Python (там заложено несколько вариантов включая R, Qt remote objects, Qt server, но пока есть только Python) и передаём ему конфиг.
2. Открываем документацию от ARQA "Интерпретатор Lua" и пишем скрипт на Python как если бы мы писали его на Lua с небольшими отличиями:
- сразу по завершении колбека указанного в eventLoopName (main в примере) бридж самоликвидируется и подчищает за собой.
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


def bridgeMain():
    global ds, dsSize, dsWasChecked, quitRequest, updateOutEnabled

    print("main started")
    qbridge.invokeQuik("PrintDbgStr", ["main started+"])
    while quitRequest is False:
        print(datetime.now().strftime("%Y.%m.%d %H:%M:%S"))
        if ds is None and dsWasChecked is False:
            print("Let create datasource")
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
        qbridge.invokeQuik("sleep", [1000])
    # закрываем
    qbridge.invokeQuikObject(ds, "Close", [])
    # удаляем
    qbridge.deleteQuikObject(ds)
    # очищаем
    ds = None
    dsSize = None


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

Обращаю внимание, что с глобальными переменными нужно обращаться осторожно, я не стал разносить каждый экземпляр по разным
сабинтерпретаторам, поэтому у них получилось одно пространство имён. Это легко решается если упаковать весь ваш код в классы.
С функциями такой проблемы нет. Почему так? Ну это какая-то странная фишка питона, не знаю, в общем. 

Далее определяем колбек на обновление нашего запрошенного ниже источника данных. Ну, логику его работы
предоставлю возможность разобрать читателю

Метод bridgeMain по сути является тем самым главным циклом main, 
как вы бы его писали на lua. В каждом цикле мы выводим дату/время и затем начинается
магия. Если источник данных ещё не создан, то создаём его
вызовом 

qbridge.invokeQuik("CreateDataSource", ["TQBR", "SBER", 1])

qbridge.invokeQuikObject(ds, "Size", []) - получаем размер
и затем устанавливаем колбек на обновление:

qbridge.invokeQuikObject(ds, "SetUpdateCallback", [dsUpdateCallback])

В этом же методе можно увидеть пример вызова метода Close через invokeQuikObject и удаления
объекта с deleteQuikObject

Ну и после завершение этой функции данный бридж завершается

Как видите, мы не закладываемся на сигнатуру вызова, вместо этого разработчик 
сам смотрит что и как передавать по документации. 
Параметры всегда передаются как массив (list в терминах Python), 
даже если в массиве один элемент. То, что в lua называется таблицами 
передаётся как dict

Функция OnStop ничем не примечательна, кроме способа завершения скрипта 
установкой переменной quitRequest в True.

В блоке if (стандартная фишка Python, но в принципе можно и без if) мы делаем некоторую 
подготовительную работу:
- регистрируем колбэки - OnStop и main

То есть бридж предоставляет только 5 методов:
- registerCallback
- invokeQuik
- invokeQuikObject
- deleteQuikObject
- getQuikVariable

В принципе, мы уже так или иначе рассмотрели все методы, кроме getQuikVariable.
Этот метод позволяет прочитать (в одну сторону из луа в питон) объект из 
инициализирующего скрипта lua. Это нужно, в первую очередь, для написания индикаторов на
питоне, потому-что им нужна таблица Settings определенного формата
Я решил не заморачиваться - раз уж есть инициализирующий lua скрипт, то пусть
и Settings будут там же, а из питона мы их прочитаем с помощью getQuikVariable

Но для индикаторов есть некоторые нюансы. При открытии окна списка индикаторов, если мы 
регистрируем колбеки из питона, то при добавлении происходит какая-то чёрная магия
и индикатор валит квик целиком. Причём даже не происходит попытки вызова колбеков.
Потратил несколько бессонных ночей на это, в результате оказалось, что индикаторы нужно 
писать несколько иначе.

Давайте посмотрим пример реализации MA на питоне:

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

function Init()
    PrintDbgStr("Prepare bridge config...")
    indBridgeConfig = {
        venvPath="c:\\Work\\QuikMultiBridge\\PythonQuik\\tstvenv",
        bridgeModule="iqb",
        scriptPath="c:\\Work\\QuikMultiBridge\\PythonQuik\\pyIndicator.py",
        eventLoopName="pyOnDestroy"
    }

    PrintDbgStr("Call initBridge")
    initBridge("Python", indBridgeConfig)
    PrintDbgStr("initBridge call finished")

    PrintDbgStr("Call pyInit")
    if pyInit then
        return pyInit()
    end
    return 1
end

function OnCalculate(indx)
    if pyOnCalculate then
        return pyOnCalculate(indx)
    end
    return nil
end

function OnDestroy()
    if pyOnDestroy then
        PrintDbgStr("Call pyOnDestroy")
        pyOnDestroy()
    end
end
```
На что обратить внимание:
- все колбеки определены в луа. Питон тоже регистрирует колбеки с префиксом 'py',
но мы их вызываем явно из луа с предварительной проверкой, что колбек инициализирован
- Инициализация бриджа происходит в функции Init - это позволяет не загружать питон
до того, как он действительно понадобится (из окна списка индикаторов)
- eventLoopName определён как pyOnDestroy - как мы помним особенность eventLoopName:
после его завершения мы подчищаем за собой. Поэтому несмотря на то, что это не eventLoop
как в роботе, мы используем pyOnDestroy как имя для event loop.

Теперь сам индикатор на питоне:
```python
import iqb


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
    cexist = iqb.invokeQuik("CandleExist", [idx])
    if cexist is True:
        p = iqb.invokeQuik(funName, [idx])
        arr.append(p)
        arrsum += p
        if len(arr) > period:
            arrsum -= arr.pop(0)
        return arrsum/len(arr)
    else:
        return None


if __name__ == '__main__':
    settings = iqb.getQuikVariable("Settings")
    if settings is not None:
        funName = settings["mode"]
        period = settings["period"]
    print("Register callbacks")
    iqb.registerCallback('pyInit', Init)
    iqb.registerCallback('pyOnCalculate', OnCalculate)
```
Собственно всё в соответствии с документацией Lua, только, пожалуй,
ссылка на цену из свечки получилась даже изящней чем то, монстроидное, что
они предлагают в документации. Ну и пример использование getQuikVariable
в наличии

## Некоторые неочевидные моменты
1. Можно таки изменять Settings индикатора из питона.
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

2. Ещё одна фича, полезность которой я разглядеть не могу, но вдруг?
Если вы зарегистрировали какой-то коллбек при помощи registerCallback, то
можете смело его вызывать с invokeQuik. Назовём эту фичу "back callback"

3. Можно запускать одновременно индикатор и скрипт, два индикатора, два скрипта.
При этом можно из одного скрипта вызывать другой - они работают в одном интерпретаторе. 
А можно и из индикаторов получать данные.

4. Я долго бодался с питоновским GIL пытаясь нормально заставить работать интерпретатор
в многопоточной среде, и в конце концов плюнул и сделал свою кооперативное переключение 
потоков. Поэтому учтите, что вызовы из питона любых методов бриджа (invokeQuik например),
как и print - это возможность поработать другим потокам. Если вы долго не вызываете ничего,
то этот поток блокирует все остальные. Я ничего нового не сделал - так же должен работать 
и родной питоновский GIL, но мали что он должен - не работает он так.

## Ещё немного про бинарник.
Я собирал с помощью статической компоновки как кьюта, 
так и VC runtime, поэтому единственные несистемные зависимости там - LUA53.dll -
должен лежать уже в папке квика (туда же и бинарник нужно кинуть), и вторая зависимость -
python39.dll - скорее всего при установке питона вы добавили его в PATH, поэтому
он доступен. Заметьте, что venv он грузит уже загрузив python39.dll, поэтому он 
обязательно должен быть доступен как-то - хоть весь его сгрузите в каталог квика


## ЗЫ
Примеры не обновлял - лень. Они все здесь есть, в доке.
Собирал и тестировал с lua 5.3
