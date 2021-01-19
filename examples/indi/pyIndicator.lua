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
