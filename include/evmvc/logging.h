/*
MIT License

Copyright (c) 2019 Michel Dénommée

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef _libevmvc_logging_h
#define _libevmvc_logging_h

#include "stable_headers.h"
#include "utils.h"

#include <sstream>
#include <iomanip>
#include <date/date.h>

namespace evmvc {

enum class log_level
{
    off = 0,
    audit_failed,
    audit_succeeded,
    fatal,
    error,
    warning,
    info,
    debug,
    trace
};

evmvc::string_view to_string(log_level lvl)
{
    switch(lvl){
        case log_level::audit_failed: return std::string("AUDIT-FAILED");
        case log_level::audit_succeeded: return std::string("AUDIT-SUCCEEDED");
        case log_level::fatal: return std::string("FATAL");
        case log_level::error: return std::string("ERROR");
        case log_level::warning: return std::string("WARNING");
        case log_level::info: return std::string("INFO");
        case log_level::debug: return std::string("DEBUG");
        case log_level::trace: return std::string("TRACE");
        case log_level::off: return std::string("OFF");
        default: return std::string("N/A");
    }
}

namespace sinks{
    
    class logger_sink;
    typedef std::shared_ptr<logger_sink> sp_logger_sink;
    
    class logger_sink
    {
    public:
        logger_sink(log_level lvl)
            : _lvl(lvl)
        {
        }
        
        bool should_log(log_level lvl)
        {
            return lvl >= _lvl;
        }
        
        virtual void log(
            evmvc::string_view log_path, log_level lvl,
            evmvc::string_view msg) = 0;
        
        void set_level(log_level lvl)
        {
            _lvl = lvl;
        }
        
        log_level level() const
        {
            return _lvl;
        }
        
    protected:
        log_level _lvl;
    };
} // ns: evmvc::sinks

namespace _internal{
    
    static std::string get_timestamp();
    
} // ns: evmvc::_internal

class logger;
typedef std::shared_ptr<logger> sp_logger;

class logger
    : public std::enable_shared_from_this<logger>,
    public sinks::logger_sink
{
private:
    
    logger(sp_logger parent, evmvc::string_view path)
        : sinks::logger_sink(parent->_lvl),
        _parent(parent),
        _path(parent->_path + "/" + std::string(path.data()))
    {
    }
    
public:

    logger()
        : sinks::logger_sink(log_level::info), _parent(), _path("")
    {
    }
    
    logger(log_level lvl)
        : sinks::logger_sink(lvl), _parent(), _path("")
    {
    }
    
    std::string path() const
    {
        return _path;
    }
    
    sp_logger add_path(evmvc::string_view path)
    {
        return std::make_shared<logger>(
            this->shared_from_this(), path
        );
    }
    
    void register_sink(evmvc::sinks::sp_logger_sink sink)
    {
        _sinks.emplace_back(sink);
    }
    
    void log(
        evmvc::string_view log_path, log_level lvl, evmvc::string_view msg)
    {
        for(auto& sink : _sinks){
            if(sink->should_log(lvl))
                sink->log(log_path, lvl, msg);
        }
    }
    
    template <typename... Args>
    void trace(evmvc::string_view f, const Args&... args) const
    {
        if(should_log(log_level::trace))
            log(_path, lvl, fmt::format(f, args...));
    }
    
    template <typename... Args>
    void debug(evmvc::string_view fmt, const Args&... args) const
    {
        if(should_log(log_level::debug))
            log(_path, lvl, fmt::format(f, args...));
    }
    
    template <typename... Args>
    void info(evmvc::string_view fmt, const Args&... args) const
    {
        if(should_log(log_level::info))
            log(_path, lvl, fmt::format(f, args...));
    }
    
    template <typename... Args>
    void warn(evmvc::string_view fmt, const Args&... args) const
    {
        if(should_log(log_level::warning))
            log(_path, lvl, fmt::format(f, args...));
    }
    
    template <typename... Args>
    void error(evmvc::string_view fmt, const Args&... args) const
    {
        if(should_log(log_level::error))
            log(_path, lvl, fmt::format(f, args...));
    }
    
    template <typename... Args>
    void fatal(evmvc::string_view fmt, const Args&... args) const
    {
        if(should_log(log_level::fatal))
            log(_path, lvl, fmt::format(f, args...));
    }
    
    template <typename... Args>
    void success(evmvc::string_view fmt, const Args&... args) const
    {
        if(should_log(log_level::audit_succeeded))
            log(_path, lvl, fmt::format(f, args...));
    }

    template <typename... Args>
    void fail(evmvc::string_view fmt, const Args&... args) const
    {
        if(should_log(log_level::audit_failed))
            log(_path, lvl, fmt::format(f, args...));
    }
    
private:
    
    sp_logger _parent;
    std::string _path;
    std::vector<sinks::sp_logger_sink> _sinks;
};

namespace sinks{
    class console_sink
        : public logger_sink
    {
    public:
        console_sink(bool enable_color)
            : logger_sink(log_level::info), color(enable_color)
        {
        }
        
        void log(
            evmvc::string_view log_path,
            log_level lvl, evmvc::string_view msg)
        {
            std::clog <<
                _gen_header(color, log_path, lvl) <<
                evmvc::replace_substring_copy(msg.data(), "\n", "\n  ") <<
                _gen_footer(color, lvl) << "\n";
        }
        
        bool color;
    private:
        
        static evmvc::string_view _level_color(log_level lvl)
        {
            switch(lvl){
                case log_level::audit_failed:
                    return "31;1";
                case log_level::audit_succeeded:
                    return "34;1";
                case log_level::fatal:
                    return "31;1";
                case log_level::error:
                    return "31";
                case log_level::warning:
                    return "33;1";
                case log_level::info:
                    return "34;1";
                case log_level::debug:
                    return "36";
                case log_level::trace:
                    return "37";
                default:
                    return "37";
            }
        }
        
        static std::string _gen_header(
            bool color, evmvc::string_view log_path, log_level lvl)
        {
            if(color)
                return fmt::format(
                    "\x1b[{}m[{}] [{}-{}]\x1b[0m\n",
                    _level_color(lvl).data(),
                    _internal::get_timestamp(),
                    to_string(lvl).data(),
                    log_path.data()
                );
            else
                return fmt::format(
                    "[{}] [{}-{}]\n",
                    _internal::get_timestamp(),
                    to_string(lvl).data(),
                    log_path.data()
                );
        }
        
        static std::string _gen_footer(bool color, log_level lvl)
        {
            if(color)
                return fmt::format(
                    "\n\x1b[{}m¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\x1b[0m",
                    _level_color(lvl).data()
                );
            else
                return "\n¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯";
        }
    };
    
    class rotating_file_sink
        : public logger_sink
    {
    public:
        rotating_file_sink(boost::filesystem::path base_filename,
            size_t max_size, size_t backlog_size)
            : logger_sink(log_level::info), 
            _base_filename(base_filename),
            _max_size(max_size),
            _backlog_size(backlog_size),
            _flush_on_lvl(log_level::error)
        {
        }
        
        void log(
            evmvc::string_view log_path,
            log_level lvl, evmvc::string_view msg)
        {
            
        }
        
        void flush()
        {
            
        }
        
        void flush_on(log_level lvl)
        {
            _flush_on_lvl = lvl;
        }
        
    private:
        
        static std::string _gen_header(
            evmvc::string_view log_path, log_level lvl)
        {
            return fmt::format(
                "[{}] [{}-{}] ",
                _internal::get_timestamp(),
                to_string(lvl).data(),
                log_path.data()
            );
        }
        
        boost::filesystem::path _base_filename;
        size_t _max_size;
        size_t _backlog_size;
        log_level _flush_on_lvl;
    };
    
} // ns: evmvc::sinks

namespace _internal{
    
    static std::string get_timestamp()
    {
        return date::format(
            "%F %H:%M%S.%s",
            std::chrono::system_clock::now()
        );
        
        // const auto currentTime = std::chrono::system_clock::now();
        // time_t time = std::chrono::system_clock::to_time_t(currentTime);
        // auto currentTimeRounded = std::chrono::system_clock::from_time_t(time);
        // if( currentTimeRounded > currentTime ) {
        //     --time;
        //     currentTimeRounded -= std::chrono::seconds( 1 );
        // }
        // //const tm *values = localtime( &time );
        // struct tm t;
        // localtime_r(&time, &t);
        // int hours = t.tm_hour;
        // int minutes = t.tm_min;
        // int seconds = t.tm_sec;
        // // etc.
        // int milliseconds =
        //     std::chrono::duration_cast<std::chrono::duration<int,std::milli> >( currentTime - currentTimeRounded ).count( );
        
        // return fmt::format(
        //     "",
        //     hours, minutes, seconds, milliseconds
        // );
        
        // std::stringstream ss;
        // ss << boost::format("%|02|")%hours << ":" << 
        //     boost::format("%|02|")%minutes << ":" << 
        //     boost::format("%|02|")%seconds << "." << 
        //     boost::format("%|03|")%milliseconds;
        // return ss.str();
    }
    
} // ns: evmvc::_internal



evmvc::sp_logger& default_logger()
{
    static evmvc::sp_logger _default_logger =
        std::make_shared<evmvc::logger>();
    return _default_logger;
}

template <typename... Args>
void trace(evmvc::string_view f, const Args&... args)
{
    if(default_logger()->should_log(log_level::trace))
        default_logger()->log(_path, lvl, fmt::format(f, args...));
}

template <typename... Args>
void debug(evmvc::string_view fmt, const Args&... args)
{
    if(default_logger()->should_log(log_level::debug))
        default_logger()->log(_path, lvl, fmt::format(f, args...));
}

template <typename... Args>
void info(evmvc::string_view fmt, const Args&... args)
{
    if(default_logger()->should_log(log_level::info))
        default_logger()->log(_path, lvl, fmt::format(f, args...));
}

template <typename... Args>
void warn(evmvc::string_view fmt, const Args&... args)
{
    if(default_logger()->should_log(log_level::warning))
        default_logger()->log(_path, lvl, fmt::format(f, args...));
}

template <typename... Args>
void error(evmvc::string_view fmt, const Args&... args)
{
    if(default_logger()->should_log(log_level::error))
        default_logger()->log(_path, lvl, fmt::format(f, args...));
}

template <typename... Args>
void fatal(evmvc::string_view fmt, const Args&... args)
{
    if(default_logger()->should_log(log_level::fatal))
        default_logger()->log(_path, lvl, fmt::format(f, args...));
}

template <typename... Args>
void success(evmvc::string_view fmt, const Args&... args)
{
    if(default_logger()->should_log(log_level::audit_succeeded))
        default_logger()->log(_path, lvl, fmt::format(f, args...));
}

template <typename... Args>
void fail(evmvc::string_view fmt, const Args&... args)
{
    if(default_logger()->should_log(log_level::audit_failed))
        default_logger()->log(_path, lvl, fmt::format(f, args...));
}






} // ns: evmvmc
#endif //_libevmvc_logging_h
