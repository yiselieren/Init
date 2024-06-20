/*
 * General init server
 *
 * Logger
 *
 */
#ifndef _LOG_H
#define _LOG_H

#include <stdarg.h>

#include <mutex>
#include <stdio.h>

class Application;
class Log
{
public:
    Log(const std::string& app_name, const std::string& logfile, int max_log_size, int max_log_amount,
        bool log_to_stdout = false);
    ~Log();

    // Open/close general (consolidated) log
    static void open(const std::string& cons_logfile, int max_cons_log_size, int max_cons_log_amount,
                     bool dupl_to_stdout = false);        // Open general (consolidated) log
    static void close();                                  // Close general (consolidated) log

    void out(const std::string& o);
    void out(const char *o);
    void print(const char *format, ...) __attribute__ ((format (printf, 2, 3)));
    void vprint(const char *format, va_list args);
private:
    static FILE        *_cons_log;
    static std::string _cons_logfile;
    static int         _max_cons_log_size;
    static int         _max_cons_log_amount;
    static bool        _cons_log_to_stdout;

    void        rotate(const std::string& name, int max_amount);

    std::string _app_name;
    FILE        *_log;
    std::string _logfile;
    std::mutex  _cons_log_access;
    int         _max_log_size;
    int         _max_log_amount;
    bool        _log_to_stdout;
};

#endif
