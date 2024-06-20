/*
 *  str_utils.h - Miscellaneous string related general routines
 *
 */

#ifndef _STR_UTILS_H
#define _STR_UTILS_H

#include <stdarg.h>
#include <string>
#include <vector>

namespace str {

// Replace environment variables in the ${VAR} form
std::string replace_env(const std::string& s);

// split/join
std::vector<std::string> split_by(const std::string& s, const char *delimiters, bool allow_empty = false);
std::string join(std::vector<std::string> l, const char *sep);

// trim whitespaces
std::string trimRight(const std::string& s, const char *delimiters = " \t\r\n");
std::string trimLeft(const std::string& s, const char *delimiters = " \t\r\n");
std::string trim(const std::string& s, const char *delimiters = " \t\r\n");

// Misc - number of bytes to human readable form
std::string humanReadableBytes(uint64_t cnt, const char *pref="");

// printf like, return std::string
std::string print(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
std::string vprint(const char *format, va_list args);

// printf like, append to std::string
void appends(std::string& str, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
void vappends(std::string& str, const char *format, va_list args);

} // namespace str

#endif
