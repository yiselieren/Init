/*
 *  str_utils.h - Miscellaneous string related general routines
 *
 */

#include "general.h"
#include "str_utils.h"

namespace str {

std::string replace_env(const std::string& s)
{
    std::string res = s;

    while (true) {
        std::string::size_type n1 = res.find("${");
        if (n1 == std::string::npos)
            break;
        std::string::size_type n2 = res.find("}", n1+2);
        if (n2 == std::string::npos)
            break;
        std::string var = res.substr(n1+2, n2-n1-2);
        char *val = getenv(var.c_str());
        if (val)
            res = res.substr(0, n1) + std::string(val) + res.substr(n2+1);
        else
        {
            printf("Environment variable \"%s\" not found\n", var.c_str());
            res = res.substr(0, n1) + res.substr(n2+1);
        }
    }

    return res;
} // replace_env

std::string replace_cmd(const std::string& s)
{
    std::string res = s;

    while (true) {
        std::string::size_type n1 = res.find("$(");
        if (n1 == std::string::npos)
            break;
        std::string::size_type n2 = res.find(")", n1+2);
        if (n2 == std::string::npos)
            break;
        std::string cmd = res.substr(n1+2, n2-n1-2);
        std::string val;
        FILE *fp = popen(cmd.c_str(), "r");
        if (fp)
        {
            const int B = 512;
            char b[B+1];
            while (fgets(b, B , fp))
                val += b;
            val = trimRight(val);
            pclose(fp);
        }
        if (!val.empty())
            res = res.substr(0, n1) + std::string(val) + res.substr(n2+1);
        else
        {
            printf("Command \"%s\" failed\n", cmd.c_str());
            res = res.substr(0, n1) + res.substr(n2+1);
        }
    }

    return res;
} // replace_cmd

std::string replace_all(const std::string& s)
{
    return replace_env(replace_cmd(s));
}

std::vector<std::string> split_by(const std::string& s, const char *delimiters, bool allow_empty)
{
    std::vector<std::string> res;

    // Special case - delimiters are empty. Split to individual characters
    if (!*delimiters)
    {
        for (std::string::size_type k=0; k < s.length(); ++k)
        {
            char c = s[k];
            res.push_back(std::string(&c, 1));
        }
        return res;
    }

    // Begining of field
    std::string::size_type j = s.find_first_not_of(delimiters);

    // String consists only from delimiters
    if (j == std::string::npos)
    {
        if (allow_empty) {
            for (std::string::size_type k=0; k < s.length(); ++k)
                res.push_back("");
        }
        return res;
    } else if (allow_empty  &&  j > 0)
        for (std::string::size_type k=0; k < j; ++k)
            res.push_back("");

    // Begining of delimiters
    std::string::size_type i = s.find_first_of(delimiters, j);

    while (i != std::string::npos)
    {
        res.push_back(s.substr(j, i - j));
        j = s.find_first_not_of(delimiters, i);

        // Rest of string - delimiters only
        if (j == std::string::npos)
        {
            if (allow_empty)
                for (std::string::size_type k=i; k < s.length(); ++k)
                    res.push_back("");
            return res;
        }
        else if (allow_empty  &&  j > i+1)
            for (std::string::size_type k=i+1; k < j; ++k)
                res.push_back("");

        // Next begining of delimiters
        i = s.find_first_of(delimiters, j);
    }

    res.push_back(s.substr(j));
    return res;
} // split_by

std::string join(std::vector<std::string> l, const char *sep)
{
    int         size = l.size();
    int         cnt  = 0;
    std::string rc;
    for (auto i = l.begin(); i != l.end(); ++i)
    {
        rc += *i;
        if (++cnt < size)
            rc += sep;
    }
    return rc;
} // join

std::string trimRight(const std::string& s, const char *delimiters)
{
    std::string::size_type st = s.find_last_not_of(delimiters);
    if (!st  ||  st == std::string::npos)
        return std::string();
    return s.substr(0, st+1 >= s.size() ?  std::string::npos : st+1);
} // trimRight

std::string trimLeft(const std::string& s, const char *delimiters)
{
    std::string::size_type st = s.find_first_not_of(delimiters);
    if (st == std::string::npos)
        return std::string();
    return s.substr(st);
} // trimLeft

std::string trim(const std::string& s, const char *delimiters)
{
    std::string::size_type st1 = s.find_first_not_of(delimiters);
    if (st1 == std::string::npos)
        return std::string();

    std::string::size_type st2 = s.find_last_not_of(delimiters);
    if (st2 == std::string::npos)
        return std::string();
    if (st2 < st1)
        return std::string();
    return s.substr(st1, st2+1 >= s.size() ?  std::string::npos : st2+1 - st1);
} // trim

std::string print(const char *format, ...)
{
    va_list   args;

    va_start(args, format);
    std::string ret = vprint(format, args);
    va_end(args);
    return ret;
}

std::string vprint(const char *format, va_list args)
{
    va_list orig_args;
    va_copy(orig_args, args);
    int len = vsnprintf(NULL, 0, format, args) + 1;

    char buf[len+1];
    vsnprintf(buf, len, format, orig_args);
    return buf;
}

void appends(std::string& str, const char *format, ...)
{
    va_list   args;

    va_start(args, format);
    vappends(str, format, args);
    va_end(args);
}

void vappends(std::string& str, const char *format, va_list args)
{
    va_list orig_args;
    va_copy(orig_args, args);
    int len = vsnprintf(NULL, 0, format, args) + 1;

    char buf[len+1];
    vsnprintf(buf, len, format, orig_args);
    str.append(buf);
}

#define K 1024
#define M (K*1024)
#define G (M*1024)
#define T (G*1024ULL)
std::string humanReadableBytes(uint64_t cnt, const char *pref)
{
    std::string rc;

    if (cnt > T)
        rc = print("%.2f%sT", (double)cnt / (double)T, pref);
    else if (cnt > G)
        rc = print("%.2f%sG", (double)cnt / (double)G, pref);
    else if (cnt > M)
        rc = print("%.2f%sM", (double)cnt / (double)M, pref);
    else if (cnt > K)
        rc = print("%.2f%sK", (double)cnt / (double)K, pref);
    else
#ifdef ARCH64
        rc = print("%lu%s", cnt, pref);
#else
        rc = print("%llu%s", cnt, pref);
#endif
    return rc;
}


} // namestace str
