/*
 *  proc_utils.h - Miscellaneous /proc manipulation routines definition
 */

#ifndef PROC_UTILS_H
#define PROC_UTILS_H

#include <stdint.h>
#include <dirent.h>

#include <string>
#include <list>

namespace proc
{

// Get processes info
#define PROC_NAME_LEN  80
#define PROC_SHCMD_LEN 80
class Info
{
public:
    Info() { init(); }
    ~Info();

    void init();
    bool get();
    char *name()      { return _name;      }
    char *short_cmd() { return _short_cmd; }
    int  pid()        { return _pid;       }

private:
    DIR  *_dir;
    char _name[PROC_NAME_LEN];
    char _short_cmd[PROC_SHCMD_LEN];
    int  _pid;
};

// Process info
bool isRunning(pid_t pid);

// Get IP
std::string procGetIP(const char *iface_name);

// Get (all) children
std::list<pid_t> getChildren(int pid, bool all = false);

// Get command line
std::string getCommandLine(pid_t pid);

// Get self name
std::string getFullName();
std::string getFullName(pid_t pid);
std::string getShortName();
std::string getShortName(pid_t pid);

// Get uptime
uint64_t    getUptime();

// Get memory info
uint64_t    getMemTotal();
uint64_t    getMemUsed();
uint64_t    getMemFree();
uint64_t    getMem(int pid);
uint64_t    getMem(const char *name);

// Get CPU info
uint64_t    getCPUTotal();
uint64_t    getCPUIdle();
uint64_t    getCPU(int pid);
uint64_t    getCPU(const char *name);

}

#endif
