
# Include all functions from the wrapped library
import seawolf as _sw

# Import other namespaces
import var
import logging
import notify

# Constants
CRITICAL = _sw.CRITICAL
ERROR = _sw.ERROR
WARNING = _sw.WARNING
NORMAL = _sw.NORMAL
INFO = _sw.INFO
DEBUG = _sw.DEBUG

FILTER_ACTION = _sw.FILTER_ACTION
FILTER_MATCH = _sw.FILTER_MATCH
FILTER_PREFIX = _sw.FILTER_PREFIX

SEAWOLF_DEFAULT_CONFIG = _sw.SEAWOLF_DEFAULT_CONFIG
SEAWOLF_MAX_NAME_LEN = _sw.SEAWOLF_MAX_NAME_LEN

init = _sw.Seawolf_init
close = _sw.Seawolf_close
exitError = _sw.Seawolf_exitError
exit = _sw.Seawolf_exit
closing = _sw.Seawolf_closing
getName = _sw.Seawolf_getName
loadConfig = _sw.Seawolf_loadConfig

class Serial:
    def __init__(self, *args):
        self.sp = _sw.Serial_open(*args)

    @staticmethod
    def open(*args):
        return Serial(*args)

    def close(self):
        return _sw.Serial_close(self.sp)

    def closePort(self, *args):
        return _sw.Serial_closePort(self.sp, *args)

    def setBlocking(self, *args):
        return _sw.Serial_setBlocking(self.sp, *args)

    def setNonBlocking(self, *args):
        return _sw.Serial_setNonBlocking(self.sp, *args)

    def setBaud(self, *args):
        return _sw.Serial_setBaud(self.sp, *args)

    def flush(self, *args):
        return _sw.Serial_flush(self.sp, *args)

    def getByte(self, *args):
        return _sw.Serial_getByte(self.sp, *args)

    def getLine(self):
        buff = []
        while True:
            c = self.getByte()

            if c == -1 or c == 0:
                return None

            if c == ord("\n"):
                break

            c.append(c)
        return "".join(buff)

    def get(self, *args):
        return _sw.Serial_get(self.sp, *args)

    def sendByte(self, *args):
        return _sw.Serial_sendByte(self.sp, *args)

    def send(self, *args):
        return _sw.Serial_send(self.sp, *args)

    def setDTR(self, *args):
        return _sw.Serial_setDTR(self.sp, *args)

    def available(self, *args):
        return _sw.Serial_available(self.sp, *args)

class PID:
    def __init__(self, *args):
        self.pid = _sw.PID_new(*args)

    def start(self, *args):
        return _sw.PID_start(self.pid, *args);

    def update(self, *args):
        return _sw.PID_update(self.pid, *args);

    def resetIntegral(self, *args):
        return _sw.PID_resetIntegral(self.pid, *args);

    def setCoefficients(self, *args):
        return _sw.PID_setCoefficients(self.pid, *args);

    def setSetPoint(self, *args):
        return _sw.PID_setSetPoint(self.pid, *args);

    def __del__(self, *args):
        _sw.PID_destroy(self.pid)

class Timer:
    def __init__(self):
        self.timer = _sw.Timer_new()

    def getDelta(self):
        return _sw.Timer_getDelta(self.timer)

    def getTotal(self):
        return _sw.Timer_getTotal(self.timer)

    def reset(self):
        _sw.Timer_reset(self.timer)

    def __del__(self):
        _sw.Timer_destroy(self.timer);
