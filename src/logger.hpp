#pragma once

#include <syslog.h>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <string>
#include <iostream>

#define LOG_FORMAT "%s"

// simple singleton logger with lazy initialization, to log to syslog
class SimpleSysLogger
{
public:
    SimpleSysLogger() :
        prefix(APP_NAME)
    {
        openlog(prefix, 0, 0);
    }

    ~SimpleSysLogger()
    {
        closelog();
    }
    SimpleSysLogger(const SimpleSysLogger&) = delete;
    SimpleSysLogger& operator=(SimpleSysLogger&) = delete;

    static SimpleSysLogger* instance()
    {
        static SimpleSysLogger instance;
        return &instance;
    }
    void logError()
    {
        syslog(LOG_ERR, LOG_FORMAT, strerror(errno));
    }
    void logMsg(int level, const std::string& msg)
    {
        assert(level >= 0 && level < LOG_DEBUG);
        syslog(level, LOG_FORMAT, msg.c_str());
    }
    void logToCout(const std::string& msg) noexcept
    {
        #ifndef NDEBUG
        std::cout << msg << std::endl;
        #endif
    }

private:
    const char* prefix;
};
