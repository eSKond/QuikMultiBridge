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
