/*
 * General init server
 *
 */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>

#include <fstream>
#include <string>
#include <thread>

#include "general.h"
#include "toml.h"
#include "str_utils.h"
#include "Application.h"
#include "Log.h"
#include "Connection.h"

#define PERR(args...)  do { printf(args); return 1; } while(0)


static void finish(int)
{
    printf("\nInterrupted, set abort request for all child\n");
    Application::abort_request = true;
}

static void usage(const char *s)
{
    const char *msg =
        "Usage:\n\n\t%s [SWITCHES] CONFIGURATION_FILE\n"
        "\nWhere switches are:\n"
        "    -h      - print this message\n"
        "    -c      - debug configuration file\n"
        "    -i      - gracefull exit on ctrl/c\n"
        "    -v      - verbose output\n"
        "    -p PORT - wait for incomming connection on port\n"
        "\n";
    printf(msg, s);
    exit(1);
}

#define GET_STATIC(TYPE, WHERE, NAME, STATIC_NAME) do {        \
        auto vit = WHERE.find(NAME);                           \
        if (vit != WHERE.end())                                \
            Application::STATIC_NAME = vit->second.as<TYPE>(); \
    } while(0)
#define GET_STATIC_S(WHERE, NAME, STATIC_NAME) do {                                     \
        auto vit = WHERE.find(NAME);                                                    \
        if (vit != WHERE.end())                                                         \
            Application::STATIC_NAME = str::replace_env(vit->second.as<std::string>()); \
    } while(0)
#define GET_OPTIONAL(TYPE, WHERE, TO, NAME) do { \
        auto vit = WHERE.find(#NAME);            \
        if (vit != WHERE.end())                  \
            (TO).NAME = vit->second.as<TYPE>();    \
    } while(0)
#define GET_OPTIONAL_S(WHERE, TO, NAME) do {                            \
        auto vit = WHERE.find(#NAME);                                   \
        if (vit != WHERE.end())                                         \
            (TO).NAME = str::replace_env(vit->second.as<std::string>());  \
    } while(0)
#define GET_MANDATORY(TYPE, WHERE, TO, NAME, T) do {              \
        auto vit = WHERE.find(#NAME);                             \
        if (vit == WHERE.end())                                   \
            PERR("app:\"%s\" - \"%s\" is mandatory\n", T, #NAME); \
        (TO).NAME = vit->second.as<TYPE>();                         \
    } while(0)
#define GET_MANDATORY_S(WHERE, TO, NAME, T) do {                       \
        auto vit = WHERE.find(#NAME);                                  \
        if (vit == WHERE.end())                                        \
            PERR("app:\"%s\" - \"%s\" is mandatory\n", T, #NAME);      \
        (TO).NAME = str::replace_env(vit->second.as<std::string>());     \
    } while(0)
#define GET_OPTIONAL_VECTOR(WHERE, TO, NAME, ELEMENT_TYPE, APP_TABLE_NAME) do {   \
    auto it = WHERE.find(#NAME);                                                  \
    if (it != WHERE.end()) {                                                      \
        toml::Value p = it->second;                                               \
        if (p.type() != toml::Value::ARRAY_TYPE)                                  \
            PERR("app:\"%s\", %s may be an array only\n", APP_TABLE_NAME, #NAME); \
        std::vector<toml::Value> par = p.as<toml::Array>();                       \
        for (size_t i = 0; i < par.size(); i++)                                   \
            (TO).NAME.push_back(str::replace_env(par[i].as<ELEMENT_TYPE>()));       \
        }                                                                         \
    } while(0)
#define GET_OPTIONAL_VECTOR_S(WHERE, TO, NAME, APP_TABLE_NAME) do {               \
    auto it = WHERE.find(#NAME);                                                  \
    if (it != WHERE.end()) {                                                      \
        toml::Value p = it->second;                                               \
        if (p.type() != toml::Value::ARRAY_TYPE)                                  \
            PERR("app:\"%s\", %s may be an array only\n", APP_TABLE_NAME, #NAME); \
        std::vector<toml::Value> par = p.as<toml::Array>();                       \
        for (size_t i = 0; i < par.size(); i++)                                   \
            (TO).NAME.push_back(str::replace_env(par[i].as<std::string>()));      \
        }                                                                         \
    } while(0)

#define GET_OPTIONAL_MAP(WHERE, TO, NAME, ELEMENT_TYPE, APP_TABLE_NAME) do {        \
    auto it = WHERE.find(#NAME);                                                    \
    if (it != WHERE.end()) {                                                        \
        toml::Value p = it->second;                                                 \
        if (p.type() != toml::Value::TABLE_TYPE)                                    \
            PERR("app:\"%s\", %s may be an table only\n", APP_TABLE_NAME, #NAME);   \
        std::map<std::string, toml::Value> par = p.as<toml::Table>();               \
        for (auto it = par.cbegin(); it != par.cend(); ++it)                        \
            (TO).NAME.[(it->first] = rit->second.as<ELEMENT_TYPE>();                \
        }                                                                           \
    } while(0)
#define GET_OPTIONAL_MAP_S(WHERE, TO, NAME, APP_TABLE_NAME) do {                          \
    auto it = WHERE.find(#NAME);                                                          \
    if (it != WHERE.end()) {                                                              \
        toml::Value p = it->second;                                                       \
        if (p.type() != toml::Value::TABLE_TYPE)                                          \
            PERR("app:\"%s\", %s may be an table only\n", APP_TABLE_NAME, #NAME);         \
        std::map<std::string, toml::Value> par = p.as<toml::Table>();                     \
        for (auto it = par.cbegin(); it != par.cend(); ++it)                              \
            (TO).NAME[it->first] = str::replace_env(it->second.as<std::string>());        \
        }                                                                                 \
    } while(0)
#define GET_OPTIONAL_S_OR_ARRAY(WHERE, TO, NAME, TBL_NAME) do {                                   \
        auto vit = WHERE.find(#NAME);                                                             \
        if (vit == WHERE.end())                                                                   \
            break;                                                                                \
        if (vit->second.type() == toml::Value::STRING_TYPE) {                                     \
            (TO).NAME = str::split_by(vit->second.as<std::string>(), " \t");                      \
        } else if (vit->second.type() == toml::Value::ARRAY_TYPE) {                               \
            std::vector<toml::Value> par = vit->second.as<toml::Array>();                         \
            for (size_t i = 0; i < par.size(); i++) {                                             \
                (TO).NAME.push_back(str::replace_env(par[i].as<std::string>()));                  \
            }                                                                                     \
        } else                                                                                    \
            PERR("app:\"%s\" - \"%s\" parameter may be string or array only\n", TBL_NAME, #NAME); \
        } while(0)
#define GET_MANDATORY_S_OR_ARRAY(WHERE, TO, NAME, TBL_NAME) do {                                  \
        auto vit = WHERE.find(#NAME);                                                             \
        if (vit == WHERE.end())                                                                   \
            PERR("app:\"%s\" - \"%s\" parameter is mandatory\n", TBL_NAME, #NAME);                \
        if (vit->second.type() == toml::Value::STRING_TYPE) {                                     \
            (TO).NAME = str::split_by(vit->second.as<std::string>(), " \t");                      \
        } else if (vit->second.type() == toml::Value::ARRAY_TYPE) {                               \
            std::vector<toml::Value> par = vit->second.as<toml::Array>();                         \
            for (size_t i = 0; i < par.size(); i++) {                                             \
                (TO).NAME.push_back(str::replace_env(par[i].as<std::string>()));                  \
            }                                                                                     \
        } else                                                                                    \
            PERR("app:\"%s\" - \"%s\" parameter may be string or array only\n", TBL_NAME, #NAME); \
        } while(0)


int read_config(const char *config_name)
{
    /*
     * Parse config file
     * -----------------
     */
    std::ifstream     conf_s(config_name);
    if (!conf_s)
        PERR("Can't open \"%s\": %s\n", config_name, strerror(errno));
    toml::ParseResult conf_t = toml::parse(conf_s);
    if (!conf_t.valid())
        PERR("Can't parse \"%s\" config file: %s\n", config_name, conf_t.errorReason.c_str());

    const toml::Value& conf = conf_t.value;

    /*
     * Fetch global configuration
     * ---------------------------
     */

    // Never runs, used only for global params
    Application default_app("default_app");

    std::map<std::string, toml::Value> conf_general = conf.as<toml::Table>();
    GET_OPTIONAL(int, conf_general, default_app, max_restart);
    GET_OPTIONAL(int, conf_general, default_app, start_delay);
    GET_OPTIONAL(int, conf_general, default_app, restart_delay);

    // Log sizes for default application are sizes of consolidated log
    GET_OPTIONAL(int, conf_general, default_app, max_log_size);
    GET_OPTIONAL(int, conf_general, default_app, max_logs_amount);
    Application::max_cons_log_size = default_app.max_log_size;
    Application::max_cons_logs_amount = default_app.max_logs_amount;

    GET_OPTIONAL_S_OR_ARRAY(conf_general, default_app, prerun, "General");
    GET_OPTIONAL_S_OR_ARRAY(conf_general, default_app, postrun, "General");
    GET_OPTIONAL_S_OR_ARRAY(conf_general, default_app, final_postrun, "General");
    GET_OPTIONAL_MAP_S(conf_general, default_app, env, "General");
    GET_STATIC_S(conf_general, "logfile", cons_logfile);
    GET_STATIC(bool, conf_general, "logstdout", cons_to_stdout);
    GET_STATIC(int, conf_general, "select_timeout", select_timeout);
    GET_STATIC(int, conf_general, "check_child_delay", check_child_delay);
    GET_STATIC(int, conf_general, "listen_backlog", listen_backlog);

    const toml::Value *apps = conf.find("applications");
    if (!apps)
        PERR("\"processes\" table must be present in configuration\n");
    if (apps->type() != toml::Value::ARRAY_TYPE)
        PERR("\"processes\" must be an array\n");

    // Applications to run
    std::vector<toml::Value> app_names = apps->as<toml::Array>();

    // Open general log
    Log::open(Application::cons_logfile, Application::max_cons_log_size, Application::max_cons_logs_amount, Application::cons_to_stdout);

    for (size_t i = 0; i < app_names.size(); i++) {
        std::string table_name = app_names[i].as<std::string>();
        Application *app = new Application(default_app);
        app->name = table_name;

        const toml::Value *app_table = conf.find(table_name);
        if (!app_table)
            PERR("\"%s\" table must be present\n", table_name.c_str());
        if (app_table->type() != toml::Value::TABLE_TYPE)
            printf("\"%s\" should be a table\n", table_name.c_str());

        std::map<std::string, toml::Value> v = app_table->as<toml::Table>();

        /*
         * Fetch per application configuration
         * -----------------------------------
         */
        GET_OPTIONAL_MAP_S(v, *app, env, table_name.c_str());
        GET_OPTIONAL(int,  v, *app, max_restart);
        GET_OPTIONAL(int,  v, *app, start_delay);
        GET_OPTIONAL(int,  v, *app, restart_delay);
        GET_OPTIONAL(int,  v, *app, max_log_size);
        GET_OPTIONAL(bool, v, *app, log_to_stdout);
        GET_OPTIONAL_S(v, *app, wd);
        GET_OPTIONAL_S(v, *app, logfile);
        GET_MANDATORY_S_OR_ARRAY(v, *app, exec, table_name.c_str());
        GET_OPTIONAL_S_OR_ARRAY(v, *app, prerun, table_name.c_str());
        GET_OPTIONAL_S_OR_ARRAY(v, *app, postrun, table_name.c_str());
        GET_OPTIONAL_S_OR_ARRAY(v, *app, final_postrun, table_name.c_str());

        Application::all_apps[table_name] = app;
    }

    return 0;
}

int main(int ac, char *av[])
{
    if (ac < 2)
        usage(av[0]);

    int argcnt=0;
    bool debug_conf = false;
    bool handle_sigquit = false;
    bool verbose = false;
    uint16_t port = 0;
    char *conf_name = nullptr, *endp;

    for (int i=1; i<ac; i++)
    {
        if (*av[i] == '-')
        {
            switch (*++av[i])
            {
            case 'c':
                debug_conf = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'i':
                handle_sigquit = true;
                break;
            case 'p':
                if (++i >= ac)
                    PERR("After switch \"%s\" port is expected.\n", av[--i]);
                port = strtol(av[i], &endp, 0);
                if (*endp)
                    PERR("\"%s\" is invalid port\n", av[i]);
                break;
            case 'h':
                usage(av[0]);
                break;
            default:
                PERR("\"%c\" is invalid flag\n",*--av[i]);
            }
        }
        else
        {
            switch (argcnt++)
            {
            case 0:
                conf_name = av[i];
                break;
            default:
                PERR("Too many parameters\n");
            }
        }
    }
    if (!conf_name)
        PERR("Configuration file is mandatory\n");

    // Signals
    if (handle_sigquit)
        signal(SIGINT, finish);


    if (read_config(conf_name))
        return 1;
    if (debug_conf)
        Application::print_all();

    // Run
    Connection conn(verbose);
    std::vector<std::thread> threads;
    for (auto it = Application::all_apps.begin(); it != Application::all_apps.end(); ++it)
        threads.emplace_back(std::thread(&Application::execute, it->second));
    std::thread *incoming = nullptr;
    if (port)
        incoming = new std::thread(&Connection::listen, &conn, port);

    // Wait for finish
    for (size_t i = 0; i < threads.size(); i++)
        threads[i].join();
    if (port) {
        incoming->join();
        delete incoming;
    }

    // Clean applicatuions
    for (auto it = Application::all_apps.begin(); it != Application::all_apps.end(); ++it)
        delete it->second;

    // Close general log
    Log::close();

    return 0;
}

