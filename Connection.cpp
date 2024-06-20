/*
 * General init server
 *
 * Incoming connection
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>


#include <thread>
#include <map>
#include <set>
#include <vector>
#include <string>

#include "str_utils.h"
#include "proc_utils.h"
#include "Application.h"
#include "Connection.h"

#define READ_BUFF 1024   /* Individual read size */
#define PERR(args...)  do { printf(args); return; } while(0)

// Commands definitions
const char *Connection::common_help =
    "Commmand syntax:\r\n"
    "    COMMAND [GENERAL_FLAGS] [PARAMETERS]\r\n\r\n"
    "Where GENERAL_FLAGS are:\r\n"
    "    -c - close connection immediate after command execution.\r\n\r\n"
    "Commands are:\r\n"
    ;

const Connection::Commands Connection::commands[] = {
    {"?",    "",          "",                                         &Connection::cmd_help},
    {"h",    "",          "",                                         &Connection::cmd_help},
    {"help", "",          "Print this help message",                  &Connection::cmd_help},
    {"e",    "",          "",                                         &Connection::cmd_exit},
    {"exit", "",          "Exit the init, abort every child process", &Connection::cmd_exit},
    {"s",    "[PROCESS] [-p]", "",                                    &Connection::cmd_show},
    {"show", "[PROCESS] [-p]", "Show process invocation details\n"
     "Show PIDs if -p is specified",                                  &Connection::cmd_show},
    {"i",    "[PROCESS]", "",                                         &Connection::cmd_info},
    {"info", "[PROCESS]", "Print info about particular child or for all\n"
     "children if PROCESS is not specified",                          &Connection::cmd_info}
};

void Connection::cmd_print_children(int fd, pid_t pid, const std::string& pref, bool show_pid)
{
    std::list<pid_t> children = proc::getChildren(pid);
    for (auto it = children.cbegin(); it != children.cend(); ++it) {
        if (show_pid)
            reply(fd, "%s%d:%s %s\r\n", pref.c_str(), *it, proc::getFullName(pid).c_str(), proc::getCommandLine(pid).c_str());
        else
            reply(fd, "%s%s %s\r\n", pref.c_str(), proc::getFullName(pid).c_str(), proc::getCommandLine(pid).c_str());
        cmd_print_children(fd, *it, pref + "  ", show_pid);
    }
}

bool Connection::cmd_show(int fd, const std::vector<std::string>& initial_params)
{
    bool show_pid = false;
    std::vector<std::string> params;
    for (size_t i = 0; i < initial_params.size(); i++) {
        if (initial_params[i] == std::string("-p"))
            show_pid = true;
        else
            params.emplace_back(initial_params[i]);
    }

    std::vector<std::string> app_names;
    if (params.size() > 0)
        app_names = params;
    else {
        for (auto it = Application::all_apps.cbegin(); it != Application::all_apps.cend(); ++it)
            app_names.emplace_back(it->first);
    }

    size_t mlen = 1;
    for (auto it = app_names.cbegin(); it != app_names.cend(); ++it) {
        if (Application::all_apps.find(*it) == Application::all_apps.end()) {
            reply(fd, "Application \"%s\" doesn't exist\r\n", it->c_str());
            return true;
        }
        if (it->size() > mlen)
            mlen = it->size();
    }

    for (auto it = app_names.cbegin(); it != app_names.cend(); ++it) {
        auto a_it = Application::all_apps.find(*it);
        if (a_it == Application::all_apps.end()) {
            reply(fd, "Application \"%s\" doesn't exist\r\n", it->c_str());
            return true;
        }
        Application *a = a_it->second;
        reply(fd, "%s%*s : ", it->c_str(), int(mlen - it->size()), "");
        for (auto p_it = a->exec.cbegin(); p_it != a->exec.cend(); p_it++)
            reply(fd, " %s", p_it->c_str());
        reply(fd, "\n");
        pid_t curr_pid = a->running_pid;
        if (curr_pid)
            cmd_print_children(fd, curr_pid, std::string("  "), show_pid);
    }

    return true;
}
bool Connection::cmd_info(int fd, const std::vector<std::string>& apps_to_print)
{
    std::vector<std::string> app_names;
    if (apps_to_print.size() > 0)
        app_names = apps_to_print;
    else {
        for (auto it = Application::all_apps.cbegin(); it != Application::all_apps.cend(); ++it)
            app_names.emplace_back(it->first);
    }

    size_t mlen = 1;
    for (auto it = app_names.cbegin(); it != app_names.cend(); ++it) {
        if (Application::all_apps.find(*it) == Application::all_apps.end()) {
            reply(fd, "Application \"%s\" doesn't exist\r\n", it->c_str());
            return true;
        }
        if (it->size() > mlen)
            mlen = it->size();
    }

    for (auto it = app_names.cbegin(); it != app_names.cend(); ++it) {
        auto a_it = Application::all_apps.find(*it);
        if (a_it == Application::all_apps.end()) {
            reply(fd, "Application \"%s\" doesn't exist\r\n", it->c_str());
            return true;
        }
        Application *a = a_it->second;
        reply(fd, "%s%*s : %s (%d%%)  %d%%  %d/%d %s\n", it->c_str(), int(mlen - it->size()), "",
              str::humanReadableBytes(a->mem).c_str(), (int)(a->mem * 100 / a->total_mem), a->curr_cpu_perc,
              a->restart_cnt, a->max_restart, a->running_pid ? "Active" : "Not active");
    }
    return true;
}
bool Connection::cmd_exit(int fd, const std::vector<std::string>&)
{
    reply(fd, "Exiting...\r\n");
    Application::abort_request = true;
    return false;
}
bool Connection::cmd_help(int fd, const std::vector<std::string>&)
{
    size_t mlen = 1;
    for (unsigned i=0; i<sizeof(commands)/sizeof(commands[0]); i++) {
        size_t l = strlen(commands[i].name) + strlen(commands[i].sub_help) + 1;
        if (l > mlen)
            mlen = l;
    }

    struct H {
        H(const std::vector<std::string>& c, const std::string h)
            : cmds(c)
            , help(h)
            { }
        std::vector<std::string> cmds;
        std::string              help;
    };
    std::vector<H> helps;
    std::set<std::string> help_handled;

    // Go thru commands and collect help. Group by same command_handler
    for (unsigned i=0; i<sizeof(commands)/sizeof(commands[0]); i++) {
        std::string hmessage = commands[i].help;
        std::vector<std::string> cmds;
        if (help_handled.find(commands[i].name) == help_handled.end()) {
            // Still not processed
            cmds.emplace_back(strlen(commands[i].sub_help)
                              ? std::string(commands[i].name) + " " + commands[i].sub_help
                              : commands[i].name);
            CmdHandler cmd = commands[i].command_handler;
            help_handled.insert(commands[i].name);
            for (unsigned j=0; j<sizeof(commands)/sizeof(commands[0]); j++) {
                if (help_handled.find(commands[j].name) == help_handled.end() && cmd == commands[j].command_handler) {
                    help_handled.insert(commands[j].name);
                    hmessage.append(commands[j].help);
                    cmds.emplace_back(strlen(commands[j].sub_help)
                                      ? std::string(commands[j].name) + " " + commands[j].sub_help
                                      : commands[j].name);
                }
            }
            helps.emplace_back(cmds, hmessage);
        }
    }

    // print reply
    reply(fd, "%s", common_help);
    for (size_t i=0; i<helps.size(); i++) {
        for (size_t j=0; j<helps[i].cmds.size(); j++) {
            if (j < helps[i].cmds.size()-1)
                reply(fd, "    %s\r\n", helps[i].cmds[j].c_str());
            else {
                std::vector<std::string> helps_v = str::split_by(helps[i].help, "\n");
                for (size_t k = 0; k < helps_v.size(); k++) {
                    if (k)
                        reply(fd, "    %*s   %s\r\n", (int)(mlen), "", helps_v[k].c_str());
                    else
                        reply(fd, "    %s%*s - %s\r\n",
                              helps[i].cmds[j].c_str(),
                              (int)(mlen- helps[i].cmds[j].size()), "",
                              helps_v[k].c_str());
                }
            }
        }
    }
    return true;
}
bool Connection::handle_command(int fd, const std::string& command)
{
    bool keep_connection = true;

    if (_verbose)
        printf("Command: \"%s\"\n", command.c_str());
    std::vector<std::string> pars;
    std::vector<std::string> inital_pars = str::split_by(command, " \t");
    std::string              cmd(inital_pars[0]);

    // Create subcommands parameter list, handle general flags
    for (size_t i = 1; i < inital_pars.size(); i++)
        if (inital_pars[i] == std::string("-c"))
            keep_connection = false;
        else
            pars.emplace_back(inital_pars[i]);

    for (unsigned i=0; i<sizeof(commands)/sizeof(commands[0]); i++)
        if (cmd == std::string(commands[i].name)) {
            bool rc = (this->*commands[i].command_handler)(fd, pars);
            return rc ? keep_connection : rc;
        }
    reply(fd, "Command \"%s\" not found\r\n", cmd.c_str());

    return true;
}
int Connection::write(int fd, const char *ptr, int nbytes)
{
    int     nleft, nwritten;

    nleft = nbytes;
    while (nleft > 0) {
        do
            nwritten = ::write(fd, ptr, nleft);
        while (nwritten < 0 && errno == EINTR);

        if (nwritten < 0) {
            fprintf(stderr, "writen: %s", strerror(errno));
            return -1;
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }

    return(nbytes - nleft);
}

void Connection::reply(int conn_fd, const char *format, ...)
{
    va_list args;
    va_list orig_args;

    va_start(args, format);
    va_copy(orig_args, args);
    int len = vsnprintf(NULL, 0, format, args) + 1;

    char buf[len+1];
    vsnprintf(buf, len, format, orig_args);
    write(conn_fd, buf, len);
}

void Connection::handle_connection(int conn_fd)
{
    const char *delims = "\r\n";
    enum { TEXT_MODE, DELIM_MODE } delim_mode = TEXT_MODE;

    if (_verbose)
        printf("Handling connection, fd:%d\n", conn_fd);
    while (true) {
        fd_set         rds;
        fd_set         eds;
        struct timeval tv;

        if (Application::abort_request)
            break;

        FD_ZERO(&rds);
        FD_ZERO(&eds);
        FD_SET(conn_fd, &rds);
        FD_SET(conn_fd, &eds);
        tv.tv_sec = Application::select_timeout / 1000000;
        tv.tv_usec = Application::select_timeout % 1000000;
        int select_rc = select(conn_fd + 1, &rds, NULL, &eds, &tv);
        if (select_rc < 0) {
            if (errno == EINTR)
                continue;
            else
                PERR("Connection - select error: %s\n", strerror(errno));
        }
        if (select_rc == 0) {
            // Select timeout
            continue;
        } else if (FD_ISSET(conn_fd, &eds)) {
            printf("Connection - read FDS exception\n");
            reply(conn_fd, "Connection - read FDS exception\n");
            break;
        } else if (FD_ISSET(conn_fd, &rds)) {
            std::string app_out;
            bool finish = false;
            while (true) {
                char b;
                int rd = read(conn_fd, &b, 1);
                if (rd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    // Read error
                    reply(conn_fd, "Connection - read error: %s (%d)\n", strerror(errno), errno);
                    break;
                } else if (rd) {
                    bool is_delim = strchr(delims, b) != NULL;
                    switch (delim_mode) {
                    case TEXT_MODE:
                        if (is_delim) {
                            if (app_out.size() > 0) {
                                if (!handle_command(conn_fd, app_out)) {
                                    finish = true;
                                    break;
                                }
                                app_out.clear();
                            }
                            delim_mode = DELIM_MODE;
                        } else {
                            app_out += b;
                        }
                        break;
                    case DELIM_MODE:
                        if (!is_delim) {
                            app_out += b;
                            delim_mode = TEXT_MODE;
                        }
                        break;
                    }
                    if (finish)
                        break;
                } else {
                    // 0 bytes read, connection closed
                    finish = true;
                    if (app_out.size() > 0)
                        handle_command(conn_fd, app_out);
                    break;
                }
            }
            if (finish) {
                close(conn_fd);
                break;
            }
        }
    }
    if (_verbose)
        printf("Exiting connection fd:%d\n", conn_fd);
}

void Connection::listen(uint16_t port)
{
    int                 one = 1;
    int                 server_fd;
    struct sockaddr_in  serv_addr;

    _port = port;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        PERR("Can't open unix socket: %s\n", strerror(errno));

    // Reuse address
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one)) < 0)
        PERR("setsockopt (SO_REUSEADDR): %s\n", strerror(errno));

    // Bind to local address
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port        = htons(port);
    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        PERR("bind: %s\n", strerror(errno));

    // Get ready to accept connection
    if (::listen(server_fd, Application::listen_backlog) < 0)
        PERR("listen: %s\n", strerror(errno));

    if (_verbose)
        printf("Waiting for incomming connections.\n");
    std::vector<std::thread> incoming_connections;
    for (;;) {
        fd_set         rds;
        struct timeval tv;

        if (Application::abort_request)
            break;
        FD_ZERO(&rds);
        FD_SET(server_fd, &rds);
        tv.tv_sec = Application::select_timeout / 1000000;
        tv.tv_usec = Application::select_timeout % 1000000;
        int select_rc = select(server_fd+ 1, &rds, NULL, NULL, &tv);
        if (select_rc < 0) {
            if (errno == EINTR)
                continue;
            else
                PERR("Incoming Connection, select error: %s\n", strerror(errno));
        }
        if (FD_ISSET(server_fd, &rds)) {
            int conn_fd = accept(server_fd, 0, 0);
            incoming_connections.emplace_back(std::thread(&Connection::handle_connection, this, conn_fd));
        }
    }

    for (size_t i = 0; i < incoming_connections.size(); i++)
        incoming_connections[i].join();
}
