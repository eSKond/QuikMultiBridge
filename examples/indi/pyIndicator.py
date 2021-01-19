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
