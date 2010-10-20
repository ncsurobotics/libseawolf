%module seawolf
%{
#include "seawolf.h"
%}

/* Handle return value of Notify_getWithAlloc() */
%ignore Notify_get;
%rename(Notify_get) Notify_getWithAlloc;
%typemap(out) char** {
    $result = Py_BuildValue("(ss)", $1[0], $1[1]);
    free($1[0]);
    free($1);
}

/* "Bad" functions that won't work in Python */
%ignore Seawolf_atExit;

%include "seawolf.h"
%include "seawolf/logging.h"
%include "seawolf/notify.h"
%include "seawolf/pid.h"
%include "seawolf/serial.h"
%include "seawolf/timer.h"
%include "seawolf/var.h"
