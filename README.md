# Init
Tiny systemd replacement (poor man systemd)

## Purpose
The **Init** tool provides the following functionality:

 - Execute external processes, restart them if necessary (indefinite or fixed number of times)
 - Control environment per process as well as the process command line and work directory
 - Introduce  optional delays  between each process' restart
 - Execute prerun/postrun scripts before/after each process' restart (optionally)
 - Capture processes stdout/stderr and optionally write log per process as well as a consolidated log for all processes. Note that
log sizes are limited by specified value. Log rotation may be executed too
 - Optimally handle incoming connection and provides some interactive commands over the connection

## Configuration

All details regarding slave processes (name, path, working directory, environment, etc.) are defined in a configuration file in TOML format. See **init.toml** for details, every parameter is described there. All global configuration items are defined in a same file.

## Command line

```
Usage:

        ./init [SWITCHES] CONFIGURATION_FILE

Where switches are:
    -h      - print this message
    -c      - debug configuration file
    -i      - graceful exit on ctrl/c
    -v      - verbose output
    -p PORT - wait for incoming connection on port
```
## Incoming connection
If the tool is invoked with a **-p PORT** command line option, it listens for incoming connection on the port.

A few commands are supported there:

|command|description|
|-------|------------|
| **?** or **h[elp]** | print some brief description of the commands |
| **e[xit]** | Exit, abort every child process|
| **s[how]** | Show invocation details for all child processes |
| **s[how]** PROCESS | Show invocation details for specified process (by name |
| **s[how]** -p PROCESS_PID | Show invocation details for specified process (by PID |
| **i[nfo]** | Show runtime (memory/CPU usage, etc.) info for all child processes |
| **i[nfo]** PROCESS | Show runtime (memory/CPU usage, etc.) info for specified child process (by name) |
| **i[nfo]** -p PROCESS_PID | Show runtime (memory/CPU usage, etc.) info for specified child process (by PID) |

Note that every command my be used with **-c** option. In such case connection closed immediate after the command reply if printed to it. Convenient for non-interactive usage.

### Example

### From first terminal:
./init init.toml -p 2020

### From second terminal:
```
telnet 127.0.0.1 2020
Trying 127.0.0.1...
Connected to 127.0.0.1.
Escape character is '^]'.
h
Commmand syntax:
    COMMAND [GENERAL_FLAGS] [PARAMETERS]

Where GENERAL_FLAGS are:
    -c - close connection immediate after command execution.

Commands are:
    ?
    h
    help                - Print this help message
    e
    exit                - Exit the init, abort every child process
    s [PROCESS] [-p]
    show [PROCESS] [-p] - Show process invocation details
                          Show PIDs if -p is specified
    i [PROCESS]
    info [PROCESS]      - Print info about particular child or for all
                          children if PROCESS is not specified

i
cpu_test : 37.52M (0%)  66%  0/0 Active
mem_test : 2.00G (12%)  16%  0/0 Active
proc1    : 6.77M (0%)  0%  0/0 Active
proc2    : 6.77M (0%)  0%  0/5 Active
i cpu_test
cpu_test : 2.00G (12%)  100%  0/0 Active
s
cpu_test :  ./TestCpu 4
mem_test :  ./TestMem 2048
proc1    :  ./Test01 T1 0.5 0.7
  /usr/bin/bash /bin/bash
    /usr/bin/bash /bin/bash
  /usr/bin/bash /bin/bash
    /usr/bin/bash /bin/bash
  /usr/bin/bash /bin/bash
proc2    :  ./Test02 T2 1.0 1.2
e
Exiting...
```
