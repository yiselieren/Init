/*
 *  proc_utils.h - Miscellaneous /proc manipulation routines implementation
 */
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "general.h"
#include "proc_utils.h"

#define LINK_BUFLEN 1024
#define STAT_BUFLEN 1024

using namespace std;
namespace proc
{

Info::~Info()
{
    if (_dir)
        closedir(_dir);
    _dir = 0;
} // Info::~Info

/*
 * Init proc info class (open /proc directory)
 */
void Info::init()
{
    _dir = opendir("/proc");
    //if (!_dir)
    //    TRACE_ERROR("Can't open \"/proc\":%s", strerror(errno));
} // Info::init

/*
 * get info
 */
bool Info::get()
{
    struct dirent   *entry;
    char            *name;
    char            stat_buf[STAT_BUFLEN];

    if (!_dir)
        return false;

    // Find next "digit" filename in /proc
    for(;;)
    {
        int         pid;
        char        exen[25];
        struct stat sb;

        // Next file in /proc directory
        if((entry = readdir(_dir)) == NULL)
        {
            closedir(_dir);
            _dir = 0;
            return false;
        }
        name = entry->d_name;

        // Non "digit" filename
        if (*name < '0' || *name > '9')
            continue;

        // /proc/.../exe entry
        pid = atoi(name);
        sprintf(exen, "/proc/%d/exe", pid);
        if(stat(exen, &sb))
            continue;

        // Read the link of the /proc/.../exe entry and return it like a result
        memset(_name, 0, PROC_NAME_LEN);
        if (readlink(exen, _name, PROC_NAME_LEN-1) < 0)
            continue;
        _pid = pid;

        // /proc/.../stat entry
        sprintf(exen, "/proc/%d/stat", pid);
        if(stat(exen, &sb) == 0)
        {
            FILE *fp = fopen(exen, "r");

            if (fp)
            {
                char *p1, *p2;

                p1 = fgets(stat_buf, STAT_BUFLEN, fp);
                fclose(fp);
                p1 = strchr(stat_buf, '(');
                p2 = strrchr(stat_buf, ')');
                if (p1 && p2)
                {
                    *p2 = 0;
                    memset(_short_cmd, 0, PROC_SHCMD_LEN);
                    strncpy(_short_cmd, ++p1, PROC_SHCMD_LEN-1);
                }
                else
                    strcpy(_short_cmd, "Unknown");
            }
            else
                strcpy(_short_cmd, "Unknown");
        }
        else
            strcpy(_short_cmd, "Unknown");

        return true;
    }
} // Info::get

/*
 * Get self name (full)
 */
string getFullName()
{
    return getFullName(getpid());
}
string getFullName(pid_t pid)
{
    char        exen[25];
    struct stat sb;
    string      rc;

    sprintf(exen, "/proc/%d/exe", pid);
    if(lstat(exen, &sb) == 0)
    {
        char      link_buf[LINK_BUFLEN];
        memset(link_buf, 0, LINK_BUFLEN);
        if (readlink(exen, link_buf, LINK_BUFLEN-1) > 0)
            rc.assign(link_buf);
        else
            rc = "Unknown";
    }
    else
        rc = "Unknown";

    return rc;
} // getFullName

/*
 * Is process running
 */
bool isRunning(pid_t pid)
{
    char        exen[32];
    struct stat sb;

    sprintf(exen, "/proc/%d/stat", pid);
    return stat(exen, &sb) == 0;
} // isRunning

/*
 * Get command line of a process
 */
string getCommandLine(pid_t pid)
{
    char   sname[4096];
    struct stat  sb;

    sprintf(sname, "/proc/%d/cmdline", pid);
    if(stat(sname, &sb))
        return "";

    FILE *fp = fopen(sname, "r");
    if (!fp)
        return "";

    int n = fscanf(fp, "%s", sname);
    fclose(fp);
    return n == 1 ? sname : "";
}

/*
 * Get list of all children of a specified pid
 * If 'all' is true - get the list recursive
 */
list<pid_t> getChildren(pid_t pid, bool all)
{
    list<pid_t> rc;
    dirent      *entry;
    DIR         *dir;
    char        sname[256];

#if 0
    dir = opendir("/proc");
    if (!dir)
        return rc;
    while((entry = readdir(dir)) != 0)
    {
        printf("entry: \"%s\"\n", entry->d_name);
    }
    closedir(dir);
#endif

    dir = opendir("/proc");
    if (!dir)
        return rc;

    while((entry = readdir(dir)) != 0)
    {
        char         *endp, *name = entry->d_name;
        unsigned     ppid, cpid;
        struct stat  sb;

        // /proc/PID/stat entry
        cpid = strtol(name, &endp, 10);
        if (*endp)
            continue;
        sprintf(sname, "/proc/%d/stat", cpid);
        if(stat(sname, &sb))
            continue;

        FILE *fp = fopen(sname, "r");
        if (!fp)
            return rc;

        int n = fscanf(fp,
                       "%u  %*s "                /* pid, short name */
                       "%*c  %u "                /* state, ppid */
                       "%*u  %*u %*s %*s "       /* pgid, sid, tty, tpgid */
                       "%*s  %*s %*s %*s %*s "   /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
                       "%*u  %*u "               /* utime, stime */
                       "%*s  %*s %*s "           /* cutime, cstime, priority */
                       "%*d  "                   /* nice */
                       "%*s  %*s %*u "           /* timeout, it_real_value, start_time */
                       "%*u  ",                  /* vsize */
                       &cpid, &ppid);
        fclose(fp);
        if (n != 2)
            continue;
        if (ppid == (unsigned)pid)
        {
            rc.push_back(cpid);
            if (all)
            {
                list<pid_t> rc1 = getChildren(cpid, true);
                rc.insert(rc.end(), rc1.begin(), rc1.end());
            }
        }
    }
    closedir(dir);

    return rc;
} // getChildren

/*
 * Get self name (short)
 */
string getShortName()
{
    return getShortName(getpid());
}
string getShortName(pid_t pid)
{
    char        exen[32];
    struct stat sb;
    string      rc;

    sprintf(exen, "/proc/%d/stat", pid);
    if(stat(exen, &sb) == 0)
    {
        FILE        *fp = fopen(exen, "r");

        if (fp)
        {
            char *p1, *p2;
            char stat_buf[STAT_BUFLEN];

            memset(stat_buf, 0, STAT_BUFLEN);
            p1 = fgets(stat_buf, STAT_BUFLEN-1, fp);
            fclose(fp);
            p1 = strchr(stat_buf, '(');
            p2 = strrchr(stat_buf, ')');
            if (p1 && p2 && p2>p1+1)
            {
                *p2 = 0;
                rc.assign(++p1);
            }
            else
                rc = "Unknown";
        }
        else
            rc = "Unknown";
    }
    else
        rc = "Unknown";

    return rc;
} // getShortName

/*
 * Return uptime in seconds
 */
uint64_t getUptime()
{
    float rc;
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp)
        return 0;
    if (fscanf(fp, "%f", &rc) != 1)
        return 0;
    fclose(fp);
    return (uint64_t)(rc + 0.5);
} // getUptime

/*
 * Return IP by interface name
 */
string getIP(const char *iface_name)
{
    string rc = string();
    int nifaces = 10;

    /*
     * get possible interfaces list
     */
    struct ifconf ifconf;
    int           sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
        return rc;

    while(1)
    {

        memset(&ifconf, 0, sizeof(ifconf));
        ifconf.ifc_buf = (char *)malloc(sizeof(struct ifreq) * nifaces);
        ifconf.ifc_len = sizeof(struct ifreq) * nifaces;

        if (ioctl(sock, SIOCGIFCONF , (char*)&ifconf) < 0)
        {
            free(ifconf.ifc_buf);
            close(sock);
            return rc;
        }
        if ((unsigned)ifconf.ifc_len < sizeof(struct ifreq) * nifaces)
            break;

        free(ifconf.ifc_buf);
        nifaces *= 2;
    }
    nifaces = ifconf.ifc_len/sizeof(struct ifreq);
    struct ifreq * ifreqs = (struct ifreq *)ifconf.ifc_buf;

    for(int i = 0; i < nifaces; i++)
    {
        if (!strcmp(ifreqs[i].ifr_name, iface_name))
        {
            rc = inet_ntoa(((struct sockaddr_in*)&ifreqs[i].ifr_addr)->sin_addr);
            break;
        }
    }
    free(ifconf.ifc_buf);
    close(sock);
    return rc;
} // getIP

uint64_t getMemTotal()
{
    struct sysinfo si;

    memset(&si, 0, sizeof(struct sysinfo));
    sysinfo(&si);
    return si.totalram;
} // getMemTotal

uint64_t getMemUsed()
{
    struct sysinfo si;

    memset(&si, 0, sizeof(struct sysinfo));
    sysinfo(&si);
    return si.totalram - si.freeram;
} // getMemUsed

uint64_t getMemFree()
{
    struct sysinfo si;

    memset(&si, 0, sizeof(struct sysinfo));
    sysinfo(&si);
    return si.freeram;
} // getMemFree

uint64_t getMem(int pid)
{
    uint64_t    vsize;
    static char b[24];

    sprintf(b, "/proc/%d/stat", pid);
    FILE *fp = fopen(b, "r");
    if (!fp)
        return 0;

    int n = fscanf(fp,
                   "%*u  %*s "               /* pid, short name */
                   "%*c  %*u "               /* state, ppid */
                   "%*u  %*u %*s %*s "       /* pgid, sid, tty, tpgid */
                   "%*s  %*s %*s %*s %*s "   /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
                   "%*u  %*u "               /* utime, stime */
                   "%*s  %*s %*s "           /* cutime, cstime, priority */
                   "%*d  "                   /* nice */
                   "%*s  %*s %*u "           /* timeout, it_real_value, start_time */
#ifdef ARCH64
                   "%lu  ",                  /* vsize */
#else
                   "%llu  ",                 /* vsize */
#endif
                   &vsize);
    fclose(fp);
    if (n != 1)
        return 0;
    return vsize;
} // getCPU

uint64_t getMem(const char *pname)
{
    uint64_t      vsize;
    uint64_t      rc=0;
    static char   b[24];
    struct dirent *entry;
    DIR           *dir;

    dir = opendir("/proc");
    if (!dir)
        return 0;

    while((entry = readdir(dir)) != 0)
    {
        char        sname[256];
        char        *endp, *name = entry->d_name;
        int         pid;
        struct stat sb;

        // /proc/PID/stat entry
        pid = strtol(name, &endp, 10);
        if (*endp)
            continue;
        sprintf(b, "/proc/%d/stat", pid);
        if(stat(b, &sb))
            continue;

        FILE *fp = fopen(b, "r");
        if (!fp)
            return rc;

        memset(sname, 0, 256);
        int n = fscanf(fp,
                       "%*u  %255s "             /* pid, short name */
                       "%*c  %*u "               /* state, ppid */
                       "%*u  %*u %*s %*s "       /* pgid, sid, tty, tpgid */
                       "%*s  %*s %*s %*s %*s "   /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
                       "%*u  %*u "               /* utime, stime */
                       "%*s  %*s %*s "           /* cutime, cstime, priority */
                       "%*d  "                   /* nice */
                       "%*s  %*s %*u "           /* timeout, it_real_value, start_time */
#ifdef ARCH64
                       "%lu  ",                  /* vsize */
#else
                       "%llu  ",                 /* vsize */
#endif
                       sname, &vsize);
        fclose(fp);
        if (n != 2)
            continue;
        if (*sname != '('  ||  sname[strlen(sname)-1] != ')')
            continue;
        sname[strlen(sname)-1] = 0;
        if (!strcmp(&sname[1], pname))
        {
            rc = vsize;
            break;
        }
    }
    closedir(dir);

    return rc;
} // getMem

uint64_t getCPUTotal()
{
    uint64_t usr=0, nice=0, sys=0, idle=0, iowait=0, irq=0, softirq=0, steal=0;
    FILE *fp = fopen("/proc/stat", "r");
    int rc;

    if (!fp)
        return 0;
#ifdef ARCH64
    rc = fscanf(fp, "cpu  %ld %ld %ld %ld %ld %ld %ld %ld",
               &usr, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
#else
    rc = fscanf(fp, "cpu  %lld %lld %lld %lld %lld %lld %lld %lld",
               &usr, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
#endif
    fclose(fp);

    if (rc < 4)
        return 0;

    return usr + nice + sys + idle + iowait + irq + softirq + steal;
} // getCPUTotal

uint64_t getCPUIdle()
{
    uint64_t idle=0, iowait=0;
    FILE *fp = fopen("/proc/stat", "r");
    int rc;

    if (!fp)
        return 0;
#ifdef ARCH64
    rc = fscanf(fp, "cpu  %*d %*d %*d %ld %ld", &idle, &iowait);
#else
    rc = fscanf(fp, "cpu  %*d %*d %*d %lld %lld", &idle, &iowait);
#endif
    fclose(fp);

    if (rc != 2)
        return 0;

    return idle + iowait;
} // getCPUTotal

uint64_t getCPU(int pid)
{
    uint64_t utime, stime;
    uint64_t start_time;
    static char b[24];

    sprintf(b, "/proc/%d/stat", pid);
    FILE *fp = fopen(b, "r");
    if (!fp)
        return 0;

#ifdef ARCH64
    int n = fscanf(fp,
                   "%*u  %*s "               /* pid, name */
                   "%*c  %*u "               /* state, ppid */
                   "%*u  %*u %*s %*s "       /* pgid, sid, tty, tpgid */
                   "%*s  %*s %*s %*s %*s "   /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
                   "%lu %lu "                /* utime, stime */
                   "%*s  %*s %*s "           /* cutime, cstime, priority */
                   "%*d  "                   /* nice */
                   "%*s  %*s %lu "           /* timeout, it_real_value, start_time */
                   "%*u  ",                  /* vsize */
                   &utime, &stime, &start_time);
#else
    int n = fscanf(fp,
                   "%*u  %*s "               /* pid, name */
                   "%*c  %*u "               /* state, ppid */
                   "%*u  %*u %*s %*s "       /* pgid, sid, tty, tpgid */
                   "%*s  %*s %*s %*s %*s "   /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
                   "%llu %llu "              /* utime, stime */
                   "%*s  %*s %*s "           /* cutime, cstime, priority */
                   "%*d  "                   /* nice */
                   "%*s  %*s %llu "          /* timeout, it_real_value, start_time */
                   "%*u  ",                  /* vsize */
                   &utime, &stime, &start_time);
#endif
    fclose(fp);
    if (n != 3)
        return 0;
    return utime + stime;
} // getCPU

uint64_t getCPU(const char *pname)
{
    uint64_t      utime, stime;
    uint64_t      start_time;
    uint64_t      rc=0;
    static char   b[24];
    struct dirent *entry;
    DIR           *dir;

    dir = opendir("/proc");
    if (!dir)
        return 0;

    while((entry = readdir(dir)) != 0)
    {
        char        sname[256];
        char        *endp, *name = entry->d_name;
        int         pid;
        struct stat sb;

        // /proc/PID/sta entry
        pid = strtol(name, &endp, 10);
        if (*endp)
            continue;
        sprintf(b, "/proc/%d/stat", pid);
        if(stat(b, &sb))
            continue;

        FILE *fp = fopen(b, "r");
        if (!fp)
            break;

        memset(sname, 0, 256);
#ifdef ARCH64
        int n = fscanf(fp,
                       "%*u  %255s "             /* pid, short name */
                       "%*c  %*u "               /* state, ppid */
                       "%*u  %*u %*s %*s "       /* pgid, sid, tty, tpgid */
                       "%*s  %*s %*s %*s %*s "   /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
                       "%lu %lu "                /* utime, stime */
                       "%*s  %*s %*s "           /* cutime, cstime, priority */
                       "%*d  "                   /* nice */
                       "%*s  %*s %lu "           /* timeout, it_real_value, start_time */
                       "%*u  ",                  /* vsize */
                       sname, &utime, &stime, &start_time);
#else
        int n = fscanf(fp,
                       "%*u  %255s "             /* pid, short name */
                       "%*c  %*u "               /* state, ppid */
                       "%*u  %*u %*s %*s "       /* pgid, sid, tty, tpgid */
                       "%*s  %*s %*s %*s %*s "   /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
                       "%llu %llu "              /* utime, stime */
                       "%*s  %*s %*s "           /* cutime, cstime, priority */
                       "%*d  "                   /* nice */
                       "%*s  %*s %llu "          /* timeout, it_real_value, start_time */
                       "%*u  ",                  /* vsize */
                       sname, &utime, &stime, &start_time);
#endif
        fclose(fp);
        if (n != 4)
            continue;
        if (*sname != '('  ||  sname[strlen(sname)-1] != ')')
            continue;
        sname[strlen(sname)-1] = 0;
        if (strcmp(&sname[1], pname))
            continue;
        rc += utime + stime;
    }
    closedir(dir);

    return rc;
} // getCPU

}
