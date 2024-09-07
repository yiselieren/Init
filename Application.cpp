/*
 * General init server
 *
 * Per application DB
 *
 */
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <chrono>

#include "Application.h"
#include "run_utils.h"
#include "proc_utils.h"
#include "str_utils.h"
#include "Log.h"

#define PERR(args...)  do { printf(args); return; } while(0)
#define READ_BUFF 1024   /* Individual read size */

// Defauls, may be overwriten in .toml configuration file
#define LOG_SIZE_DEF      1048576
#define LOGS_AMOUNT_DEF   3
#define WD_DEF            "/tmp"
#define MAX_RESTART_DEF   0
#define START_DELAY_DEF   100
#define RESTART_DELAY_DEF 5000
#define MAX_RESTART_DEF   0
#define BACKLOG_DEF       5
#define CHECK_CHILD_DELAY   100  /* Milliseconds */
#define SELECT_TIMEOUT_DEF 1000  /* Milliseconds */
#define STOP_ON_PYTHON_EXC_CNT 5

bool        Application::abort_request = false;
int         Application::select_timeout  = SELECT_TIMEOUT_DEF;
int         Application::check_child_delay = CHECK_CHILD_DELAY;
std::string Application::cons_logfile;
int         Application::max_cons_log_size = LOG_SIZE_DEF;
int         Application::max_cons_logs_amount = LOGS_AMOUNT_DEF;
bool        Application::cons_to_stdout = false;
uint64_t    Application::total_mem = proc::getMemTotal();
int         Application::listen_backlog = BACKLOG_DEF;
std::map<std::string, Application*> Application::all_apps;

Application::Application(const std::string &n)
    : name(n)
    , log_to_stdout(false)
    , use_pty(false)
    , stop_on_python_exception(false)
    , restart_cnt(0)
    , mem(0)
    , curr_cpu_total(0)
    , curr_cpu(0)
    , curr_cpu_perc(0)
{
    wd = WD_DEF;
    max_restart = MAX_RESTART_DEF;
    start_delay = START_DELAY_DEF;
    restart_delay = RESTART_DELAY_DEF;
    max_restart = MAX_RESTART_DEF;
    stop_on_python_exception_cnt = STOP_ON_PYTHON_EXC_CNT;
}
#define INDENT 21
#define PRINT_S(NAME) do {                                  \
        printf("    %s:%*s\"%s\"\n", #NAME, (int)(INDENT-strlen(#NAME)), " ", NAME.c_str()); \
    } while (0)
#define PRINT_I(NAME) do {                      \
        printf("    %s:%*s%d\n", #NAME, (int)(INDENT-strlen(#NAME)), " ", NAME); \
    } while (0)
#define PRINT_B(NAME) do {                      \
        printf("    %s:%*s%s\n", #NAME, (int)(INDENT-strlen(#NAME)), " ", NAME ? "true" : "false"); \
    } while (0)
#define PRINT_V(NAME) do {                                              \
        printf("    %s:%*s", #NAME, (int)(INDENT-strlen(#NAME)), " "); \
        for (size_t i = 0; i < NAME.size(); i++)                        \
            printf("\"%s\"%s", NAME[i].c_str(),                         \
                   i < NAME.size() - 1 ? ", " : "");                    \
        printf("\n");                                                   \
    } while (0)
#define PRINT_M(NAME) do {                                              \
    size_t indent = 0;                                                  \
    for (auto it = NAME.cbegin(); it != NAME.cend(); ++it)              \
        if (it->first.size() > indent)                                  \
            indent = it->first.size();                                  \
    indent++;                                                           \
    printf("    %s:\n", #NAME);                                          \
    for (auto it = NAME.cbegin(); it != NAME.cend(); ++it)              \
        printf("        %s%*s = \"%s\"\n",                              \
               it->first.c_str(), (int)(indent - it->first.size()),     \
               " ", it->second.c_str());                                \
    } while (0)

void Application::print_all()
{
    printf("General:\n");
    PRINT_I(select_timeout);
    PRINT_I(check_child_delay);
    PRINT_S(cons_logfile);
    PRINT_I(max_cons_log_size);
    PRINT_I(max_cons_logs_amount);
    PRINT_I(listen_backlog);
    PRINT_B(cons_to_stdout);
    for (auto it = all_apps.cbegin(); it != all_apps.cend(); ++it) {
        printf("\n%s:\n", it->first.c_str());
        it->second->print();
    }
}

void Application::print() const
{
    PRINT_V(exec);
    PRINT_I(max_restart);
    PRINT_I(start_delay);
    PRINT_I(restart_delay);
    PRINT_I(max_log_size);
    PRINT_I(max_logs_amount);
    PRINT_S(wd);
    PRINT_S(logfile);
    PRINT_V(prerun);
    PRINT_V(postrun);
    PRINT_V(final_postrun);
    PRINT_M(env);
}

void Application::killapp(pid_t pid, Log& log)
{
    int status;
    pid_t w;
    kill(pid, SIGTERM);
    std::this_thread::sleep_for(std::chrono::milliseconds(check_child_delay));
    w = waitpid(pid, &status, WUNTRACED | WNOHANG);
    if (w == pid || w < 0) {
        log.print("*****\n***** Process \"%s\" killed with SIGQUIT\n*****", name.c_str());
        return;
    }

    kill(pid, SIGKILL);
    std::this_thread::sleep_for(std::chrono::milliseconds(check_child_delay));
    w = waitpid(pid, &status, WUNTRACED | WNOHANG);
    if (w == pid || w < 0) {
        log.print("*****\n***** Process \"%s\" killed with SIGKILL\n*****", name.c_str());
        return;
    }
    log.print("*****\n***** Can't kill the \"%s\" process", name.c_str());
}

void Application::sample_resources(pid_t pid)
{
    mem = proc::getMem(pid);

    uint64_t new_cpu_total = proc::getCPUTotal();
    uint64_t new_cpu = proc::getCPU(pid);

    if (curr_cpu_total && curr_cpu)
    {
        uint64_t delta_total = new_cpu_total - curr_cpu_total;
        uint64_t us = delta_total ? (100 * (new_cpu - curr_cpu)) / delta_total : 0;
        us  = us > 100 ? 100 : us;
        curr_cpu_perc = us;
    }
    curr_cpu_total = new_cpu_total;
    curr_cpu = new_cpu;

#if 0
    struct timeval tv;
    gettimeofday(&tv, NULL);
    printf("get sample for %s /pid:%d/, mem %s/%s (%d%%), cpu:%d%%, time:%ld.%06ld\n",
           name.c_str(), pid,
           str::humanReadableBytes(total_mem).c_str(), str::humanReadableBytes(mem).c_str(),
           (int)(mem * 100 / total_mem), curr_cpu_perc,
           tv.tv_sec, tv.tv_usec);
#endif
}

bool Application::check_pyton_exception(const std::string& l)
{
    const char *python_traceback = "Traceback (most recent call last):";
    std::vector<std::string> lines = str::split_by(l, "\r\n");
    for (unsigned i=0; i<lines.size(); i++)
        if (lines[i].find(python_traceback) != std::string::npos)
            return true;
    return false;
}

void Application::execute_once(const std::string& name, const std::vector<std::string>& cmd_line,
                               Log& log, bool /*collect_statistics*/)
{
    int   stdin_fd;  // stdin file descriptor of the slave process
    int   stdout_fd; // stdout file descriptor of the slave process
    pid_t pid;       // pid of the slave process
    struct timeval tv;

    /*
     * Starting application
     */
    int argc = cmd_line.size() + 1;
    char **argv = new char*[cmd_line.size() + 2];

    for (size_t i = 0; i < cmd_line.size(); i++)
        argv[i] = (char*)cmd_line[i].c_str();
    argv[cmd_line.size()] = NULL;

    if (use_pty)
        exec_process_pty(&pid, &stdout_fd, wd.c_str(), argc, argv);
    else
        exec_process(&pid, &stdin_fd, &stdout_fd, nullptr, wd.c_str(), argc, argv);
    running_pid = pid;
    delete [] argv;

    if (fcntl(stdout_fd, F_SETFL, O_NONBLOCK) < 0)
        PERR("App: \"%s\", can't set non block mode for output: %s\n", name.c_str(), strerror(errno));

    gettimeofday(&tv, NULL);
    uint64_t prev_sampl = tv.tv_sec * 1000000UL + tv.tv_usec;
    int python_exception_cnt = 0;
    bool kill_request = false;
    while (true) {
        /*
         * Loop thru process's output
         */
        fd_set rds;
        fd_set eds;
        struct timeval tv;

        if (Application::abort_request) {
            killapp(pid, log);
            break;
        }

        if (kill_request && (python_exception_cnt++ >= stop_on_python_exception_cnt))
        {
            log.print("***** Process \"%s\" - kill request\n", name.c_str());
            killapp(pid, log);
        }

        FD_ZERO(&rds);
        FD_ZERO(&eds);
        FD_SET(stdout_fd, &rds);
        FD_SET(stdout_fd, &eds);
        tv.tv_sec = Application::select_timeout / 1000;
        tv.tv_usec = (Application::select_timeout % 1000) * 1000;
        int select_rc = select(stdout_fd + 1, &rds, NULL, &eds, &tv);
        if (select_rc < 0) {
            if (errno == EINTR)
                continue;
            else
                PERR("\"%s\" - select error: %s\n", name.c_str(), strerror(errno));
        }
        if (FD_ISSET(stdout_fd, &rds)) {
            std::string app_out;
            while (true) {

                gettimeofday(&tv, NULL);
                uint64_t curr_sampl = tv.tv_sec * 1000000UL + tv.tv_usec;
                if (curr_sampl - prev_sampl >= (uint64_t)(Application::select_timeout * 1000UL)) {
                    // time to sample memory/cpu
                    sample_resources(pid);
                    prev_sampl = curr_sampl;
                }

                char b[READ_BUFF];
                bzero(b, READ_BUFF - 1);
                int rd = read(stdout_fd, b, READ_BUFF - 1);
                if (rd > 0)
                    app_out.append(b, rd);
                if (rd < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        // Read error
                        log.print("***** Process \"%s\" - read error: %s (%d)\n", name.c_str(), strerror(errno), errno);
                        printf("From \"%s\"/%d - read error: %s (%d)\n", name.c_str(), pid, strerror(errno), errno);
                        killapp(pid, log);
                    }
                    break;
                } else if (rd < READ_BUFF - 1) {
                    // We already read all available data
                    break;
                }
            }
            log.out(app_out);
            if (stop_on_python_exception && check_pyton_exception(app_out))
                kill_request = true;
        }
        if (FD_ISSET(stdout_fd, &eds)) {
            log.print("***** Process \"%s\" - read FDS exception\n", name.c_str());
            printf("From \"%s\"/%d - read FDS exception\n", name.c_str(), pid);
            killapp(pid, log);
        }

        int status;
        pid_t w = waitpid(pid, &status, WUNTRACED | WNOHANG);
        if (w < 0) {
            // Process doesn't exists, probably killed before because of some errors
            break;
        } else if (w == pid) {
            log.print("*****\n***** Process \"%s\" exited with %d status\n*****", name.c_str(), status);
            break;
        }

        gettimeofday(&tv, NULL);
        uint64_t curr_sampl = tv.tv_sec * 1000000UL + tv.tv_usec;
        if (curr_sampl - prev_sampl >= (uint64_t)(Application::select_timeout * 1000UL)) {
            // time to sample memory/cpu
            sample_resources(pid);
            prev_sampl = curr_sampl;
        }

    }
    running_pid = 0;
} // execute_once

void Application::execute()
{
    Log log(name, logfile, max_log_size, max_logs_amount, log_to_stdout);

    while (true) {
        // Start/restart delays
        if (restart_cnt == 0) {
            if (start_delay) {
                log.print("***** Delay %d milliseconds before first start", start_delay);
                std::this_thread::sleep_for(std::chrono::milliseconds(start_delay));
            }
        } else if (restart_delay) {
            log.print("***** Delay %d milliseconds before restart", restart_delay);
            std::this_thread::sleep_for(std::chrono::milliseconds(restart_delay));
        }

        // Start/restart logs
        std::string cmd(exec[0]);
        for (size_t i = 1; i < exec.size(); i++)
            str::appends(cmd, " %s", exec[i].c_str());
        log.print("***** %s the \"%s\" process with:\n*****     %s", restart_cnt ? "Restart" : "Start", name.c_str(), cmd.c_str());

        // Prerun?
        if (prerun.size() > 0) {
            log.print("***** \"%s\" prerun", name.c_str());
            execute_once(name + " - prerun", prerun, log);
        }

        // -------------------
        // Execute application
        // -------------------
        execute_once(name, exec, log, true);

        // Abort request - do not restart, kill and exit
        if (Application::abort_request)
            break;

        // Postrun?
        if (postrun.size() > 0) {
            log.print("***** \"%s\" postrun", name.c_str());
            execute_once(name + " - postrun", postrun, log);
        }

        // Restart if necessary (max_restart is zero or restart counter not exceeded maximal
        restart_cnt++;
        if (!max_restart || restart_cnt <= max_restart) {
            // Restart
            if (max_restart)
                log.print("***** Restarting the \"%s\" process, %d / %d", name.c_str(), restart_cnt, max_restart);
            else
                log.print("***** Restarting the \"%s\" process", name.c_str());
        } else {
            log.print("***** Process \"%s\" is not restarted, max restart (counter %d) is reached", name.c_str(), max_restart);
            restart_cnt = 0;
            // Final postrun?
            if (final_postrun.size() > 0) {
                log.print("***** \"%s\" final postrun", name.c_str());
                execute_once(name + " - final postrun", final_postrun, log);
            }
            break;
        }
    }
} // execute
