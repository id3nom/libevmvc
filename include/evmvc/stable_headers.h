/*
    This file is part of libpq-async++
    Copyright (C) 2011-2018 Michel Denommee (and other contributing authors)
    
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "evmvc_config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <aio.h>
#include <signal.h>

#include <fcntl.h>
#include <unistd.h>
#include <cinttypes>

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <deque>
#include <vector>
#include <initializer_list>
#include <pthread.h>

#include "fmt/format.h"
//#include <spdlog/spdlog.h>

extern "C" {
#ifndef EVHTP_DISABLE_REGEX
    #define EVHTP_DISABLE_REGEX
#endif
#include <event2/http.h>
#include <evhtp/evhtp.h>

#include <pcre.h>

}
#include <zlib.h>

#include <boost/logic/tribool.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#ifdef EVMVC_USE_STD_STRING_VIEW
    #include <string_view>
#else
    #include <boost/utility/string_view.hpp>
#endif

#ifndef _libevmvc_stable_headers_h
#define _libevmvc_stable_headers_h

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
    #include "nlohmann/json.hpp"
#pragma GCC diagnostic pop

// EVMVC namespace start
namespace evmvc {
namespace bfs = boost::filesystem;

#ifdef EVMVC_USE_STD_STRING_VIEW
    /// The type of string view used by the library
    using string_view = std::string_view;
    
    /// The type of basic string view used by the library
    template<class CharT, class Traits>
    using basic_string_view = std::basic_string_view<CharT, Traits>;

#else
    
    /// The type of string view used by the library
    using string_view = boost::string_view;
    
    /// The type of basic string view used by the library
    template<class CharT, class Traits>
    using basic_string_view = boost::basic_string_view<CharT, Traits>;
    
#endif

typedef nlohmann::json json;

class logger;
typedef std::shared_ptr<logger> sp_logger;

class app;
typedef std::shared_ptr<app> sp_app;
typedef std::weak_ptr<app> wp_app;

class route_result;
typedef std::shared_ptr<route_result> sp_route_result;

class route;
typedef std::shared_ptr<route> sp_route;

class router;
typedef std::shared_ptr<router> sp_router;

class http_param;
typedef std::shared_ptr<http_param> sp_http_param;
typedef std::vector<sp_http_param> http_params;

class response;
typedef std::shared_ptr<evmvc::response> sp_response;

class request;
typedef std::shared_ptr<evmvc::request> sp_request;


// evmvc::_internal namespace
namespace _internal{
    struct multipart_content_t;
    typedef struct multipart_content_t multipart_content;
    struct multipart_content_form_t;
    typedef struct multipart_content_form_t multipart_content_form;
    struct multipart_content_file_t;
    typedef struct multipart_content_file_t multipart_content_file;
    struct multipart_subcontent_t;
    typedef struct multipart_subcontent_t multipart_subcontent;
    struct multipart_parser_t;
    typedef struct multipart_parser_t multipart_parser;
    
    // typedef struct app_request_t
    // {
    //     bool is_multipart;
    //     evmvc::app* app;
    //     evmvc::_internal::multipart_parser* mp;
    // } app_request;
    typedef struct request_args_t
    {
        evmvc::sp_route_result rr;
        evmvc::sp_response res;
    } request_args;
    
    evmvc::sp_logger& default_logger();
    
    evmvc::sp_response create_http_response(
        wp_app a,
        evhtp_request_t* ev_req,
        const evmvc::http_params& params
    );
    
    evmvc::sp_response create_http_response(
        sp_logger log,
        //struct app_request_t* ar,
        evhtp_request_t* ev_req, sp_route rt,
        const evmvc::http_params& params
    );
    
    evhtp_res on_headers(
        evhtp_request_t* req, evhtp_headers_t* hdr, void* arg);
    void on_multipart_request(evhtp_request_t* req, void* arg);
    void on_app_request(evhtp_request_t* req, void* arg);
    
    void send_error(
        evmvc::sp_response res, int status_code,
        evmvc::string_view msg = "");
    
    
    /* Write "n" bytes to a descriptor. */
    ssize_t writen(int fd, const void *vptr, size_t n)
    {
        size_t nleft;
        ssize_t nwritten;
        const char* ptr;
        
        ptr = (const char*)vptr;
        nleft = n;
        while(nleft > 0){
            if((nwritten = write(fd, ptr, nleft)) <= 0){
                if (errno == EINTR)
                    nwritten = 0;/* and call write() again */
                else
                    return(-1);/* error */
            }
            
            nleft -= nwritten;
            ptr   += nwritten;
        }
        return(n);
    }
    /* end writen */
    
}}//ns evmvc::_internal

#include "stack_debug.h"

#define EVMVC_ERR(msg, ...) \
    evmvc::stacked_error( \
        fmt::format(msg, ##__VA_ARGS__), \
        __FILE__, __LINE__, __PRETTY_FUNCTION__ \
    )

#if EVMVC_BUILD_DEBUG
#define EVMVC_DEF_TRACE(msg, ...) evmvc::_internal::default_logger()->trace(msg, ##__VA_ARGS__)
#define EVMVC_DEF_DBG(msg, ...) evmvc::_internal::default_logger()->debug(msg, ##__VA_ARGS__)
#define EVMVC_DEF_INFO(msg, ...) evmvc::_internal::default_logger()->info(msg, ##__VA_ARGS__)

#define EVMVC_TRACE(log, msg, ...) log->trace(msg, ##__VA_ARGS__)
#define EVMVC_DBG(log, msg, ...) log->debug(msg, ##__VA_ARGS__)
#define EVMVC_INFO(log, msg, ...) log->info(msg, ##__VA_ARGS__)

#else
#define EVMVC_DEF_TRACE(msg, ...) 
#define EVMVC_DEF_DBG(msg, ...) 
#define EVMVC_DEF_INFO(msg, ...) 

#define EVMVC_TRACE(log, msg, ...) 
#define EVMVC_DBG(log, msg, ...) 
#define EVMVC_INFO(log, msg, ...) 

#endif

#define __EVMVC_STRING(s) #s
#define EVMVC_STRING(s) __EVMVC_STRING(s)

// file read buffer size of 10KiB
#define EVMVC_READ_BUF_SIZE 10240

// http://www.zlib.net/manual.html#Advanced
#define EVMVC_COMPRESSION_NOT_SUPPORTED (-MAX_WBITS - 1000)
#define EVMVC_ZLIB_DEFLATE_WSIZE (-MAX_WBITS)
#define EVMVC_ZLIB_ZLIB_WSIZE MAX_WBITS
#define EVMVC_ZLIB_GZIP_WSIZE (MAX_WBITS | 16)
#define EVMVC_ZLIB_MEM_LEVEL 8
#define EVMVC_ZLIB_STRATEGY Z_DEFAULT_STRATEGY

#define EVMVC_PCRE_DATE EVMVC_STRING(PCRE_DATE)


namespace evmvc {

inline std::string version()
{
    static std::string ver;
    static bool version_init = false;
    if(!version_init){
        version_init = true;
        
        struct utsname uts;
        uname(&uts);
        
        ver = fmt::format(
            "{}\n  built with gcc v{}.{}.{} "
            "({} {} {} {}) {}\n"
            "    Boost v{}\n"
            "    {}\n" //openssl
            "    zlib v{}\n"
            "    libpcre v{}.{} {}\n"
            "    libicu v{}\n"
            "    libevent v{}\n"
            "    libevhtp v{}\n",
            EVMVC_VERSION_NAME,
            __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
            uts.sysname, uts.release, uts.version, uts.machine,
            __DATE__,
            BOOST_LIB_VERSION,
            OPENSSL_VERSION_TEXT,
            ZLIB_VERSION,
            PCRE_MAJOR, PCRE_MINOR, EVMVC_PCRE_DATE,
            EVMVC_ICU_VERSION,
            _EVENT_VERSION,
            EVHTP_VERSION
        );
    }
    
    return ver;
}

class stacked_error : public std::runtime_error 
{
public:
    stacked_error(evmvc::string_view msg)
        : std::runtime_error(msg.data()),
        _stack(evmvc::_internal::get_stacktrace()),
        _file(), _line(), _func()
    {
    }
    
    stacked_error(
        evmvc::string_view msg,
        evmvc::string_view filename,
        int line,
        evmvc::string_view func)
        : std::runtime_error(msg.data()),
        _stack(evmvc::_internal::get_stacktrace()),
        _file(filename), _line(line), _func(func)
    {
    }
    
    ~stacked_error(){}
    
    std::string stack() const { return _stack;}
    evmvc::string_view file(){ return _file;}
    int line(){ return _line;}
    evmvc::string_view func(){ return _func;}
    
private:
    std::string _stack;
    evmvc::string_view _file;
    int _line;
    evmvc::string_view _func;
};


struct event_base** ev_base()
{
    static struct event_base* __ev_base = nullptr;
    return &__ev_base;
}
struct event_base** thread_ev_base()
{
    static thread_local struct event_base* _trd_ev_base = nullptr;
    return &_trd_ev_base;
}
// struct evhtp_t** htp_instance()
// {
//     static struct evhtp_t* __htp = nullptr;
//     return &__htp;
// }
// struct event_base* get_best_base()
// {
//     evthr_pool_t* pool = (*htp_instance())->thr_pool;
//     
//     return evthr_get_base(thr);
// }


} //ns evmvc

#ifdef EVMVC_USE_STD_STRING_VIEW
    
#else
namespace fmt {
    template <>
    struct formatter<evmvc::string_view> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

        template <typename FormatContext>
        auto format(const evmvc::string_view &s, FormatContext &ctx) {
            return format_to(ctx.out(), "{}", s.data());
        }
    };
}
    
#endif

#endif//_libevmvc_stable_headers_h
