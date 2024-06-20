/*
 * General init server
 *
 * Connection handler
 *
 */
#ifndef _CONNECTION_H
#define _CONNECTION_H

class Connection
{
public:
    Connection(bool verbose = false)
        : _verbose(verbose)
        { }

    void listen(uint16_t port);

private:
    typedef bool (Connection::*CmdHandler)(int, const std::vector<std::string>&);
    struct Commands {
        const char *name;
        const char *sub_help;
        const char *help;
        CmdHandler command_handler;
    };
    static const Commands commands[];
    static const char *common_help;

    void reply(int fd, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
    int  write(int fd, const char *ptr)      { return write(fd, ptr, strlen(ptr));    }
    int  write(int fd, const std::string& s) { return write(fd, s.c_str(), s.size()); }
    int  write(int fd, const char *ptr, int nbytes);
    void handle_connection(int conn_fd);
    bool handle_command(int fd, const std::string& cmd);

    void cmd_print_children(int fd, pid_t pid, const std::string& pref, bool show_pid = false);
    bool cmd_help(int fd, const std::vector<std::string>& pars);
    bool cmd_exit(int fd, const std::vector<std::string>& pars);
    bool cmd_show(int fd, const std::vector<std::string>&);
    bool cmd_info(int fd, const std::vector<std::string>&);

    uint16_t _port = 0;
    bool     _verbose;
};

#endif
