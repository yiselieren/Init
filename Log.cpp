/*
 * General init server
 *
 * Logger
 *
 */
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "Log.h"
#include "str_utils.h"

std::string Log::_cons_logfile;
FILE        *Log::_cons_log = NULL;
int         Log::_max_cons_log_size = 0;
int         Log::_max_cons_log_amount = 0;
bool        Log::_cons_log_to_stdout = false;

Log::Log(const std::string& app_name, const std::string& logfile, int max_log_size, int max_log_amount,
         bool log_to_stdout)
    : _app_name(app_name)
    , _log(NULL)
    , _max_log_size(max_log_size)
    , _max_log_amount(max_log_amount)
    , _log_to_stdout(log_to_stdout)
{
    if (!logfile.empty()) {
        _logfile = logfile;
        _log = fopen(_logfile.c_str(), "a");
        if (!_log)
            fprintf(stderr, "Can't open log file \"%s\": %s\n", _logfile.c_str(), strerror(errno));
    }
}

Log::~Log()
{
    if (_log)
        fclose(_log);
    _log = NULL;
}

void Log::open(const std::string& cons_logfile, int max_cons_log_size, int max_cons_log_amount, bool dupl_to_stdout)
{
    if (!cons_logfile.empty() && !_cons_log) {
        _max_cons_log_size = max_cons_log_size;
        _max_cons_log_amount = max_cons_log_amount;
        _cons_logfile = cons_logfile;
        _cons_log_to_stdout = dupl_to_stdout;
        _cons_log = fopen(_cons_logfile.c_str(), "a");
        if (!_cons_log)
            fprintf(stderr, "Can't open consolidated log file \"%s\": %s\n", cons_logfile.c_str(), strerror(errno));
    }
}

void Log::close()
{
    if (_cons_log)
        fclose(_cons_log);
    _cons_log = NULL;
}

void Log::rotate(const std::string& name, int max_amount)
{
    unlink(str::print("%s.%d", name.c_str(), max_amount).c_str());

    for (int i= max_amount; i > 1; i--) {
        std:: string new_name = str::print("%s.%d", name.c_str(), i);
        std:: string old_name = str::print("%s.%d", name.c_str(), i-1);
        rename(old_name.c_str(), new_name.c_str());
    }

    std:: string new_name = str::print("%s.1", name.c_str());
    rename(name.c_str(), new_name.c_str());
}

void Log::out(const std::string& o)
{
    out(str::trimRight(o).c_str());
}
void Log::out(const char *o)
{
    struct timeval tv;
    const int max_tstamp = 32;
    char tstamp[max_tstamp];

    gettimeofday(&tv, NULL);
    snprintf(tstamp, max_tstamp-1, "%ld.%06ld", tv.tv_sec, tv.tv_usec);
    int prefix_len = strlen(tstamp);

    std::vector<std::string> output = str::split_by(o, "\r\n");
    if (_log) {
        // Check if rotate is necessary
        fpos_t pos;
        if (!fgetpos(_log, &pos)) {
            if (pos.__pos >= _max_log_size) {
                // Rotate per application log
                fclose(_log);
                _log = NULL;

                rotate(_logfile, _max_log_amount);

                _log = fopen(_logfile.c_str(), "w");
                if (!_log)
                    fprintf(stderr, "Can't open log file \"%s\": %s\n", _logfile.c_str(), strerror(errno));
            }
        }

        // First line of output with timestamp
        fprintf(_log, "%s %s\n", tstamp, output[0].c_str());
        if (_log_to_stdout)
            printf("%s %s\n", tstamp, output[0].c_str());
        // Subsequent output lines, without timestamp but aligned
        for (size_t i = 1; i < output.size(); i++) {
            fprintf(_log, "%*s %s\n", prefix_len, " ", output[i].c_str());
            if (_log_to_stdout)
                printf("%*s %s\n", prefix_len, " ", output[i].c_str());
        }
        fflush(_log);
    }
    if (_cons_log) {
        // all consolidated log manipulation needs lock, it may work from different threads
        const std::lock_guard<std::mutex> lock(_cons_log_access);

        // Check if rotate is necessary
        fpos_t pos;
        if (!fgetpos(_cons_log, &pos)) {
            if (pos.__pos >= _max_cons_log_size) {
                // Rotate consolidate log
                fclose(_cons_log);
                _cons_log = NULL;

                rotate(_cons_logfile, _max_cons_log_amount);

                _cons_log = fopen(_cons_logfile.c_str(), "w");
                if (!_cons_log)
                    fprintf(stderr, "Can't open log file \"%s\": %s\n", _cons_logfile.c_str(), strerror(errno));
            }
        }

        prefix_len += _app_name.size() + 3;
        // First line of output with timestamp and process name
        fprintf(_cons_log, "%s [%s] %s\n", tstamp, _app_name.c_str(), output[0].c_str());
        if (_cons_log_to_stdout)
            printf("%s [%s] %s\n", tstamp, _app_name.c_str(), output[0].c_str());
        // Subsequent output lines, without these but aligned
        for (size_t i = 1; i < output.size(); i++) {
            fprintf(_cons_log, "%*s %s\n", prefix_len, " ", output[i].c_str());
            if (_cons_log_to_stdout)
                printf("%*s %s\n", prefix_len, " ", output[i].c_str());
        }
        fflush(_cons_log);
    }
}

void Log::print(const char *format, ...)
{
    va_list   args;

    va_start(args, format);
    vprint(format, args);
    va_end(args);
}

void Log::vprint(const char *format, va_list args)
{
    va_list orig_args;
    va_copy(orig_args, args);
    int len = vsnprintf(NULL, 0, format, args) + 1;

    char buf[len+1];
    vsnprintf(buf, len, format, orig_args);
    out(buf);
}
