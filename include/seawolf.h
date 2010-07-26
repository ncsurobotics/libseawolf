/**
 * \file
 * \brief Top level libseawolf include
 */

/**
 * \mainpage
 *
 * \section intro Introduction
 *
 * Here you will find all documentation for the Seawolf core library
 * (<i>libseawolf</i>) and the Seawolf hub server (<i>hub server</i>). Together
 * these are referred to as the <i>Seawolf Framework</i> (SF). The framework
 * provides a foundation for building distributed software platforms for use in
 * robotics and other sensor/control systems. libseawolf provides numerous
 * utilities to ease development, and in part with the hub server, also provides
 * message passing, shared variables, and centralized configuration and logging
 * to make building distributed systems quick and easy.
 *
 * \section Design
 *
 * When refering to an individual program compiled against libseawolf we will
 * refer to it as an \e application. When referring to multiple applications
 * running together with a single hub server, we refer to this as an
 * <i>application pool</i>. When using the Seawolf Framework the goal is to
 * design multiple applications which can work with one another as an
 * application pool. The primary advantage of this design is that applications
 * can easily be distributed across multiple devices connected via a
 * network. Additionally, this multiple application approach generally improves
 * reliability and fault tolerance, as applications are well isolated from one
 * another. A typical application pool may look as follows,
 *
 * \dot
 *  graph {
 *    node [shape=Mrecord, style="filled", fillcolor="#aaddff", fontname="Sans"];
 *    edge [dir=both, arrowsize=0.9, color="#101020"];
 *    fontname="Sans";
 *    nodesep = 0.6;
 *    pad = 0.5;
 *
 *    application_1:lib:s -- hub;
 *    application_2:lib:s -- hub;
 *    application_3:lib:s -- hub;
 *    application_4:lib:s -- hub;
 *
 *    subgraph cluster_applications {
 *      application_4 [label="{Application 1|<lib> libseawolf}"];
 *      application_3 [label="{Application 2|<lib> libseawolf}"];
 *      application_2 [label="{Application 3|<lib> libseawolf}"];
 *      application_1 [label="{...|<lib> libseawolf}"];
 *      label = "Application Pool";
 *      style = "filled, rounded";
 *      color = "#eeeeee";
 *    }
 *    hub [label="{Hub server|{libseawolf | db}}", fillcolor="#aaaaff"];
 *  }
 * \enddot
 *
 * As can be seen, each application is built on top of libseawolf, and, when
 * running, all applications in the pool connect to a single hub
 * server. libseawolf handles all of the difficult work of communicating with
 * the hub server and presents an easy to use API for application developers
 * providing shared variables, interprocess notifications, and centralized
 * logging.
 *
 * \section writinganapp Writing an Application
 *
 * Writing applications using the Seawolf Framework is made as simple as
 * possible. The following is the outline of a typical Seawolf Framework based
 * application,
 *
 * \code
 * #include "seawolf.h"
 *
 * int main(void) {
 *    Seawolf_loadConfig("../conf/seawolf.conf");
 *    Seawolf_init("My application name");
 *
 *    ...
 *
 *    Seawolf_close();
 *    return 0;
 * }
 * \endcode
 *
 * For most applications this is all the boilerplate code that is
 * neccessary. The first two lines in main() are responsible for loading a
 * configuration file and initializing the library. The configuration file
 * specifies the address and port of the hub server to connect to, the password
 * to use when authenticating with the hub server, and options to control
 * logging functionality. No other configuration is done is this file. The
 * argument passed to Seawolf_init() specifies the name of the application. This
 * name is primarily for logging purposes.
 *
 * \section hubserver Hub Server
 *
 * The hub server is responsible for performing centralized logging, variable
 * storage, and notification passing for applications. The hub server requires a
 * short configuration file and uses a simple flat-file database to store
 * variable definitions and persistant variable values. All of these files are
 * human readable and can be edited with a simple text editor.
 *
 * \subsection hubconfig Configuration
 *
 * The hub server takes a number of run-time options which can be specified in a
 * configuration file. When running the hub the -c command line argument can be
 * used to explicitly give the location of a configuration file. If this is not
 * given the hub will look for a .swhubrc file in the current users home
 * directory and, failing that, try /etc/seawolf_hub.conf. If no configuration
 * file can be found the the hub print a warning and falls back to its default
 * internal configuration.
 *
 * The syntax of the configuration file is the same as other libseawolf
 * configuration files and is outlined in the \ref Config "configuration"
 * module. Below is an example configuration file listing all valid options with
 * their default values.
 *
 * \code
 * #
 * # Example configuration file
 * ##
 * 
 * # Bind to localhost on port 31427
 * bind_address = 127.0.0.1
 * bind_port = 31427
 *
 * # Use an empty password
 * password = 
 *
 * # Location of the variable definitions file
 * var_defs = seawolf_var.defs
 *
 * # Location of the variable database (stores values for persistent variables)
 * var_db = seawolf_var.db
 *
 * # No log file, print messages to standard output
 * log_file = 
 * log_level = NORMAL
 * log_replicate_stdout = 1
 * \endcode
 *
 * \subsection hubvardef Variable Definitions
 * 
 * The hub reads a list of variable definitions from the file specified by the
 * var_defs option in the configuration file. Only variables defined in this
 * file can be access by applications and attempts to access undefined variables
 * will result in an error and the client application being disconnected from
 * the hub. A variable definition specifies a name, a default value, designates
 * whether the variable should be persistent, and specifies if the variable
 * should be readonly. Persistent variables are stored on disk in the variable
 * database (specified by the var_db option in the configuration file) and
 * therefore their values persist even if the hub should be stopped and
 * restarted. The format of the variable definitions file is illustrated in the
 * following example,
 *
 * \code
 * #
 * # Example variable definitions
 * ##
 * 
 * # NAME               = DEAFULT, PERSIST, READONLY
 * Aft                  = 0.0,     0,       0
 * CountDownStatus      = 0.0,     0,       0
 * Depth                = 0.0,     0,       0
 * DepthHeading         = 0.0,     0,       0
 * DepthPID.d           = 0.0,     1,       0
 * DepthPID.i           = 0.0,     1,       0
 * DepthPID.p           = 0.0,     1,       0
 * ...
 *
 * \endcode
 * 
 * A varible name goes to the left of the = and the comma separated values to
 * the right of the = are default value, persistent, and readonly
 * respectively. Persistent and readonly are boolean flags which should be set
 * to either 1 or 0.
 *
 * \subsection hubvardb Variable Database
 *
 * The variable database stores the current values for persistent variables. The
 * location is given by the var_db option in the configuration file. Typically
 * this file won't be edited by hand, but it can be if needed. The hub may
 * overrride any changes made to this file while it is running, so if changes
 * are to be made to the variable database by hand, they should be made while
 * the hub is not running. The format of the database is similar to the
 * configuration file and the definitions file,
 *
 * \code
 * # VARIABLE           = VALUE
 * DepthPID.d           = -0.2500
 * DepthPID.i           = 0.5000
 * DepthPID.p           = 20.0000
 * ...
 * \endcode
 *
 * \subsection hubrunning Running the Hub
 *
 * By default, when you build and install libseawolf the hub will be installed
 * to /usr/local/bin as `seawolf-hub'. Running the hub is a simple matter of
 * creating a configuration file and variable definitions file and then
 * executing
 *
 * \code
 * $ seawolf-hub -c seawolf.conf
 * \endcode
 *
 * If you have placed you configuration file in one of the default locations
 * described \ref hubconfig "above" then this can be shortened to just
 *
 * \code
 * $ seawolf-hub
 * \endcode
 *
 * \section components API Organization
 * 
 * The library is organized into \e components such that functions in a component
 * have the form \e Component_someFunction(...). These components can further be
 * grouped according to use or functionality.
 *
 * \subsection core_routines Core
 * These routines include core library initialization and configuration
 *  - \ref Main "Seawolf" - Library initialization and control routines.
 *
 * \subsection comm_routines Communication
 * These components focus on interprocess communication and application -> hub
 * communication.
 *  - \ref Comm "Comm" - Low level routines for communicating with a connected
 *    hub server. Unlikely to be used in applications.
 *  - \ref Logging "Logging" - Facilities for logging, both local and centralized
 *    through the hub server
 *  - \ref Notify "Notify" - Sending and receiving notifications. Notifications
 *    are broadcast messages sent between applications
 *  - \ref Var "Var" - Support for setting and retrieving shared variables
 * 
 * \subsection datastructure_routines Data Structures
 * A number of useful and common data structures are provided for general use.
 *  - \ref Dictionary "Dictionary" - A dictionary, also known as a lookup table
 *    or hash map
 *  - \ref List "List" - A simple list/array with support for many useful routines
 *  - \ref Queue "Queue" - A thread-safe queue implementation
 *  - \ref Stack "Stack" - A simple first-in-last-out stack
 *
 * \subsection hardware_routines Hardware Access
 * Serial access is complicated and error-prone so a easy to use serial API as
 * well as routines for establishing and identify microcontrollers serially
 * attached is provided.
 *  - \ref Serial "Serial" - Routines for opening serial devices and sending and
 *    receive data
 *  - \ref Ard "ArdComm" - Identifying and handshaking with microcontrollers
 *    following the ArdComm protocol
 * 
 * \subsection misc_routines Utilities
 * A number of utilities often useful in robotics and other control systems
 * applications are provided as part of the library
 *  - \ref Config "Configuration" - Loading configuration options
 *  - \ref PID "PID" - Generic implementation of a
 *    Proportional-Integral-Derivative (PID) controller
 *  - \ref Task "Task" - Support for task scheduling and running backgroud tasks
 *  - \ref Timer "Timer" - Timers for easily keeping track of changes in time
 *  - \ref Util "Util" - A number of misc, but useful utilities
 */

/**
 * \defgroup Core Core Routines
 * \defgroup Communications Communications
 * \defgroup DataStructures Data Structures
 * \defgroup Hardware Hardware Access
 * \defgroup Utilities Utilities
 */

#ifndef __SEAWOLF_ROOT_INCLUDE_H
#define __SEAWOLF_ROOT_INCLUDE_H

/**
 * Make available POSIX functions
 */
#define _POSIX_C_SOURCE 200112L

/**
 * Make more functions available
 */
#define _XOPEN_SOURCE 600

/* Make sure some standard includes are available */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

/* Include all Seawolf III development headers */
#include "seawolf/comm.h"
#include "seawolf/logging.h"
#include "seawolf/var.h"
#include "seawolf/notify.h"

#include "seawolf/serial.h"
#include "seawolf/ardcomm.h"

#include "seawolf/config.h"
#include "seawolf/util.h"
#include "seawolf/timer.h"
#include "seawolf/task.h"
#include "seawolf/pid.h"

#include "seawolf/stack.h"
#include "seawolf/list.h"
#include "seawolf/queue.h"
#include "seawolf/dictionary.h"

/** Default location of the configuration file */
#define SEAWOLF_DEFAULT_CONFIG "/etc/seawolf.conf"

/* Initialize and close */
void Seawolf_init(const char* name);
void Seawolf_close(void);
void Seawolf_exitError(void);
void Seawolf_exit(void);
bool Seawolf_closing(void);
char* Seawolf_getName(void);
void Seawolf_loadConfig(const char* filename);

#endif // #ifndef __SEAWOLF_ROOT_INCLUDE_H
