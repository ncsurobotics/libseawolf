
from . import seawolf as _sw

get = _sw.Var_get
set = _sw.Var_set
setAutoNotify = _sw.Var_setAutoNotify

stale = _sw.Var_stale
poked = _sw.Var_poked
subscribe = _sw.Var_subscribe
unsubscribe = _sw.Var_unsubscribe
sync = _sw.Var_sync
