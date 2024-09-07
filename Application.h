/*
 * General init server
 *
 * Per application DB
 *
 */
#ifndef _APPLICATION_H
#define _APPLICATION_H

#include <unistd.h>
#include <signal.h>

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>

class Log;
class Application
{
public:
    Application(const std::string &name);

    void print() const;              // Print the instance to stdout (DEBUG)
    void static print_all();         // Print the instance to stdout (DEBUG)
    void execute();                  // Main execution loop
    void killapp(pid_t pid, Log& log);
    void execute_once(const std::string& name, const std::vector<std::string>& cmd_line,
                      Log& log, bool collect_statistics = false);
    void sample_resources(pid_t pid);
    bool check_pyton_exception(const std::string& l);

    static bool abort_request;     // When true - each thread will abort its process and exit
    static int  select_timeout;    // (milliseconds) We will timeout if read takes more than this
                                   // time to check process status and collect statistics
    static int  check_child_delay; // (milliseconds) Delay between sending SIGINT or SIGKILL
                                   // and check the status of a child when killing a child
    static std::string cons_logfile;         // File name of consolidated log
    static bool        cons_to_stdout;       // Duplicate consolidated log to stdout
    static int         max_cons_log_size;    // Max consolidated logfile size
    static int         max_cons_logs_amount; // Max consolidated logfile amount
    static int         listen_backlog;       // backlog parameter for listen()
    static std::map<std::string, Application*> all_apps;  // All applications by name from toml conf. file

    // Set only once, while reading configuration
    std::string                        name;             // app's name
    std::vector<std::string>           exec;             // app's executable name and start params
    int                                max_restart;      // Maxinal restart counter
                                                         // 0 - indefinetely, -1 - do not restart
    int                                start_delay;      // Delay before initial app. start
    int                                restart_delay;    // Delay before app. restart
    int                                max_log_size;     // Max logfile size
    int                                max_logs_amount;  // Max logfile amount
    std::string                        wd;               // app's working directory
    std::map<std::string, std::string> env;              // app's environment
    std::vector<std::string>           prerun;           // Pre-run command
    std::vector<std::string>           postrun;          // Post-run command
    std::vector<std::string>           final_postrun;    // Final post-run comamnd
    std::string                        logfile;          // Log file name
    bool                               log_to_stdout;    // Duplicate log to stdout
    bool                               use_pty;          // Use pty (forkpty intead of fork) for slave process
    bool                               stop_on_python_exception;  // Stop when python exception is detected
    int                                stop_on_python_exception_cnt; // Witing counter

    // Running data
    int                                restart_cnt;      // How many times app is restarted
    pid_t                              running_pid;      // PID of running process, 0 otherwise
                                                         // Related to "main" process only, not for pre/post run

    // Statistics data
    static uint64_t                    total_mem;
    uint64_t                           mem;
    uint64_t                           curr_cpu_total;
    uint64_t                           curr_cpu;
    int                                curr_cpu_perc;
};

#endif
