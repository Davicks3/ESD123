import ctypes, sys
lib = "/Library/Frameworks/dwf.framework/dwf"
dwf = ctypes.cdll.LoadLibrary(lib)
try:
    if hasattr(dwf, "FDwfDeviceCloseAll"):
        dwf.FDwfDeviceCloseAll()
        print("Called FDwfDeviceCloseAll()")
except Exception as e:
    print("closeall failed:", e)

h = ctypes.c_int()
rc = dwf.FDwfDeviceOpen(ctypes.c_int(-1), ctypes.byref(h))
print("FDwfDeviceOpen rc:", int(rc), "handle:", getattr(h,"value",None))
if not bool(rc):
    buf = ctypes.create_string_buffer(512)
    try:
        dwf.FDwfGetLastErrorMsg(buf)
        print("SDK error:", buf.value.decode('utf-8', errors='replace'))
    except Exception as e:
        print("FDwfGetLastErrorMsg failed:", e)
else:
    print("Opened ok; closing...")
    try:
        dwf.FDwfDeviceClose(h)
    except Exception:
        pass