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

#include "evmvc_config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
#include <sstream>
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

#include <openssl/opensslconf.h>
#include <openssl/dh.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <event2/event.h>
#include <event2/util.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/listener.h>
#include <pcre.h>
#include <zlib.h>

#include <boost/logic/tribool.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "tools-md/tools-md.h"

#ifndef _libevmvc_stable_headers_h
#define _libevmvc_stable_headers_h

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
    #include "nlohmann/json.hpp"
#pragma GCC diagnostic pop


#if LIBEVENT_VERSION_NUMBER < 0x02010000
inline size_t evbuffer_add_iovec(
    struct evbuffer* buf, struct evbuffer_iovec* v, size_t nv)
{
    size_t l = 0;
    for(size_t i = 0; i < nv; ++i)
        l += v[i].iov_len;
    evbuffer_expand(buf, l);
    for(size_t i = 0; i < nv; ++i)
        evbuffer_add(buf, v[i].iov_base, v[i].iov_len);
    return l;
}
#endif


// EVMVC namespace start
namespace evmvc {
namespace bfs = boost::filesystem;

typedef nlohmann::json json;

class app_t;
typedef std::shared_ptr<app_t> app;
typedef std::weak_ptr<app_t> wp_app;

class child_server_t;
typedef std::shared_ptr<child_server_t> child_server;

class channel;
class worker_t;
class http_worker_t;
class cache_worker;
typedef std::shared_ptr<worker_t> worker;
typedef std::weak_ptr<worker_t> wp_worker;
typedef std::shared_ptr<http_worker_t> http_worker;
typedef std::weak_ptr<http_worker_t> wp_http_worker;
typedef std::shared_ptr<cache_worker> sp_cache_worker;
typedef std::weak_ptr<cache_worker> wp_cache_worker;

class connection;
typedef std::shared_ptr<connection> sp_connection;
typedef std::weak_ptr<connection> wp_connection;
class parser;
typedef std::shared_ptr<parser> sp_parser;
class url;


class route_result_t;
typedef std::shared_ptr<route_result_t> route_result;

class route_t;
typedef std::shared_ptr<route_t> route;

class router_t;
typedef std::shared_ptr<router_t> router;


struct ci_less_hash
{
    size_t operator()(const std::string& kv) const
    {
        return std::hash<std::string>{}(boost::to_lower_copy(kv));
    }
};
struct ci_less_eq
{
    struct nocase_compare
    {
        bool operator()(const unsigned char& c1, const unsigned char& c2) const
        {
            return tolower(c1) < tolower(c2);
        }
    };
    bool operator()(const std::string& s1, const std::string& s2) const
    {
        return strcasecmp(s1.c_str(), s2.c_str()) == 0;
    }
};
typedef std::unordered_map<
    std::string, 
    std::vector<std::string>,
    ci_less_hash, ci_less_eq
    > header_map_t;
typedef std::shared_ptr<header_map_t> header_map;

class http_param;
typedef std::shared_ptr<http_param> sp_http_param;
//typedef std::vector<sp_http_param> http_params;
class http_params_t;
typedef std::unique_ptr<http_params_t> http_params;

class response_t;
typedef std::shared_ptr<evmvc::response_t> response;

class request_t;
typedef std::shared_ptr<evmvc::request_t> request;

enum class running_state
{
    stopped,
    starting,
    running,
    stopping,
};
inline md::string_view to_string(evmvc::running_state s)
{
    switch(s){
        case evmvc::running_state::stopped:
            return "stopped";
        case evmvc::running_state::starting:
            return "starting";
        case evmvc::running_state::running:
            return "running";
        case evmvc::running_state::stopping:
            return "stopping";
        default:
            throw std::invalid_argument(fmt::format(
                "UNKNOWN running_sate: '{}'", (int)s
            ));
    }
}

enum class http_version
{
    unknown,
    http_10,
    http_11,
    http_2
};
inline md::string_view to_string(http_version v)
{
    switch(v){
        case http_version::http_10:
            return "HTTP/1.0";
        case http_version::http_11:
            return "HTTP/1.1";
        case http_version::http_2:
            return "HTTP/2";
        default:
            return "unknown";
    }
}


namespace multip{
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
}
namespace _internal{
    md::log::logger& default_logger();
    
    evmvc::response create_http_response(
        wp_connection conn,
        http_version ver,
        url uri,
        header_map hdrs,
        route_result rr
    );
    evmvc::response on_headers_completed(
        wp_connection conn,
        md::log::logger log,
        url uri,
        http_version ver,
        header_map hdrs,
        app a
    );
    void on_multipart_request_completed(
        request req,
        response res,
        app a
    );
    
    void send_error(
        evmvc::response res, int status_code,
        md::string_view msg = ""
    );
    
    
    
    #define EVMVC_CTRL_MSG_MAX_ADDR_LEN 113
    typedef struct ctrl_msg_data_t
    {
        size_t srv_id;
        int iproto;
        //char addr[EVMVC_CTRL_MSG_MAX_ADDR_LEN];
        //uint16_t port;
    } ctrl_msg_data;
    
}}//ns evmvc::_internal


#if EVMVC_BUILD_DEBUG
#define EVMVC_DEF_TRACE(msg, ...) md::log::default_logger()->trace(msg, ##__VA_ARGS__)
#define EVMVC_DEF_DBG(msg, ...) md::log::default_logger()->debug(msg, ##__VA_ARGS__)
#define EVMVC_DEF_INFO(msg, ...) md::log::default_logger()->info(msg, ##__VA_ARGS__)

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
            "    libevent v{}\n",
            EVMVC_VERSION_NAME,
            __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
            uts.sysname, uts.release, uts.version, uts.machine,
            __DATE__,
            BOOST_LIB_VERSION,
            OPENSSL_VERSION_TEXT,
            ZLIB_VERSION,
            PCRE_MAJOR, PCRE_MINOR, EVMVC_PCRE_DATE,
            EVMVC_ICU_VERSION,
            _EVENT_VERSION
        );
    }
    
    return ver;
}



} //ns evmvc

#endif//_libevmvc_stable_headers_h
