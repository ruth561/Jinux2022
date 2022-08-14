#include <cstdio>

#include "logging.hpp"
#include "console.hpp"

int printk(const char *format, ...);

extern Console *console;


namespace logging
{

    int Logger::debug(const char *format, ...)
    {
        int res = 0;
        va_list ap;
        char s[1024];

        if (level_ <= kDEBUG) {
            printk("DEBUG  >> ");
            va_start(ap, format);
            res = vsprintf(s, format, ap);
            va_end(ap);

            console->PutString(s);
        }
        return res;
    }

    int Logger::info(const char *format, ...)
    {
        int res = 0;
        va_list ap;
        char s[1024];

        if (level_ <= kINFO) {
            printk("INFO   >> ");
            va_start(ap, format);
            res = vsprintf(s, format, ap);
            va_end(ap);

            console->PutString(s);
        }
        return res;
    }

    int Logger::warning(const char *format, ...)
    {
        int res = 0;
        va_list ap;
        char s[1024];

        if (level_ <= kWARNING) {
            printk("WARNING>> ");
            va_start(ap, format);
            res = vsprintf(s, format, ap);
            va_end(ap);

            console->PutString(s);
        }
        return res;
    }

    int Logger::error(const char *format, ...)
    {
        int res = 0;
        va_list ap;
        char s[1024];

        if (level_ <= kERROR) {
            printk("ERROR  >> ");

            va_start(ap, format);
            res = vsprintf(s, format, ap);
            va_end(ap);
            // printk("***");

            console->PutString(s);
        }
        return res;
    }
}
