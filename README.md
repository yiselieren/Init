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
