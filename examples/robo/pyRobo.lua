-- PrintDbgStr("start")
-- print(package.cpath)

PrintDbgStr("Lua: Try load bridge...")
require "QuikMultiBridge"
PrintDbgStr("Lua: Bridge loaded")

bridgeConfig = {
    venvPath="c:\\Work\\QuikMultiBridge\\PythonQuik\\tstvenv",
    bridgeModule="qbridge",
    scriptPath="c:\\Work\\QuikMultiBridge\\PythonQuik\\pyRobo.py",
    eventLoopName="main"
    };

PrintDbgStr("Lua: Try load python script...")
initBridge("Python", bridgeConfig);
PrintDbgStr("Lua: Python script loaded")
