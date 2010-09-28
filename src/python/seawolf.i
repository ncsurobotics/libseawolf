%module seawolf
%begin %{
#include "seawolf.h"
%}

%include "seawolf.h"
%include "seawolf/logging.h"
%include "seawolf/notify.h"
%include "seawolf/pid.h"
%include "seawolf/serial.h"
%include "seawolf/timer.h"
%include "seawolf/util.h"
%include "seawolf/var.h"
