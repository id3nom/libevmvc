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

#ifndef _libevmvc_configuration_h
#define _libevmvc_configuration_h

#include "stable_headers.h"
#include "utils.h"
#include "ssl_utils.h"
#include "logging.h"

namespace evmvc {

typedef EVP_PKEY* (*ssl_decrypt_cb)(const char* cert_key_file);
typedef void* (*ssl_scache_init)(evmvc::sp_child_server);

enum class ssl_verify_mode
{
    none = SSL_VERIFY_NONE,
    peer = SSL_VERIFY_PEER,
    fail_if_no_peer_cert = SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
    client_once = SSL_VERIFY_CLIENT_ONCE
    //post_handshake = SSL_VERIFY_POST_HANDSHAKE,
};
EVMVC_ENUM_FLAGS(evmvc::ssl_verify_mode);

enum class ssl_cache_type
{
    disabled = 0,
    internal,
    user,
    builtin
};

#define _EVMVC_DEF_SSL_OPTS SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | \
    SSL_MODE_RELEASE_BUFFERS | SSL_OP_NO_COMPRESSION
#define _EVMVC_DEF_SSL_VERIF_MODE ssl_verify_mode::none
#define _EVMVC_DEF_SSL_CACHE_TYPE evmvc::ssl_cache_type::disabled
#define _EVMVC_DEF_SSL_CIPHERS ""
#define _EVMVC_DEF_SSL_NC "prime256v1"

class ssl_options
{
public:
    ssl_options()
        :
        cert_file(""),
        cert_key_file(""),
        cafile(""),
        capath(""),
        
        ciphers(_EVMVC_DEF_SSL_CIPHERS),
        named_curve(_EVMVC_DEF_SSL_NC),
        dhparams(""),
        
        ssl_opts(_EVMVC_DEF_SSL_OPTS),
        ssl_ctx_timeout(0),
        
        verify_mode(_EVMVC_DEF_SSL_VERIF_MODE),
        verify_depth(0),
        
        x509_verify_cb(nullptr),
        x509_chk_issued_cb(nullptr),
        decrypt_cb(nullptr),
        
        store_flags(0),
        
        cache_type(_EVMVC_DEF_SSL_CACHE_TYPE),
        cache_timeout(0),
        cache_size(0),
        cache_init_cb(nullptr),
        cache_args(nullptr)
    {
    }
    
    ssl_options(const evmvc::ssl_options& o)
        : cert_file(o.cert_file),
        cert_key_file(o.cert_key_file),
        cafile(o.cafile),
        capath(o.capath),
        
        ciphers(o.ciphers),
        named_curve(o.named_curve),
        dhparams(o.dhparams),
        
        ssl_opts(o.ssl_opts),
        ssl_ctx_timeout(o.ssl_ctx_timeout),
        
        verify_mode(o.verify_mode),
        verify_depth(o.verify_depth),
        
        x509_verify_cb(o.x509_verify_cb),
        x509_chk_issued_cb(o.x509_chk_issued_cb),
        decrypt_cb(o.decrypt_cb),
        
        store_flags(o.store_flags),
        
        cache_type(o.cache_type),
        cache_timeout(o.cache_timeout),
        cache_size(o.cache_size),
        cache_init_cb(o.cache_init_cb),
        cache_args(o.cache_args)
    {
    }
    
    ssl_options(evmvc::ssl_options&& o)
        : cert_file(std::move(o.cert_file)),
        cert_key_file(std::move(o.cert_key_file)),
        cafile(std::move(o.cafile)),
        capath(std::move(o.capath)),
        
        ciphers(std::move(o.ciphers)),
        named_curve(std::move(o.named_curve)),
        dhparams(std::move(o.dhparams)),
        
        ssl_opts(o.ssl_opts),
        ssl_ctx_timeout(o.ssl_ctx_timeout),
        
        verify_mode(o.verify_mode),
        verify_depth(o.verify_depth),
        
        x509_verify_cb(o.x509_verify_cb),
        x509_chk_issued_cb(o.x509_chk_issued_cb),
        decrypt_cb(o.decrypt_cb),
        
        store_flags(o.store_flags),
        
        cache_type(o.cache_type),
        cache_timeout(o.cache_timeout),
        cache_size(o.cache_size),
        cache_init_cb(o.cache_init_cb),
        cache_args(o.cache_args)
    {
        o.ciphers = _EVMVC_DEF_SSL_CIPHERS;
        o.named_curve = _EVMVC_DEF_SSL_NC;

        o.ssl_opts = _EVMVC_DEF_SSL_OPTS;
        o.ssl_ctx_timeout = 0;
        
        o.verify_mode = _EVMVC_DEF_SSL_VERIF_MODE;
        o.verify_depth = 0;
        
        o.x509_verify_cb = nullptr;
        o.x509_chk_issued_cb = nullptr;
        o.decrypt_cb = nullptr;
        
        o.store_flags = 0;
        
        o.cache_type = _EVMVC_DEF_SSL_CACHE_TYPE;
        o.cache_timeout = 0;
        o.cache_size = 0;
        o.cache_init_cb = nullptr;
        o.cache_args = nullptr;
    }
    
    ssl_options& operator=(ssl_options&& o)
    {
        cert_file = std::move(o.cert_file);
        cert_key_file = std::move(o.cert_key_file);
        cafile = std::move(o.cafile);
        capath = std::move(o.capath);
        
        ciphers = std::move(o.ciphers);
        named_curve = std::move(o.named_curve);
        dhparams = std::move(o.dhparams);
        
        ssl_opts = o.ssl_opts;
        ssl_ctx_timeout = o.ssl_ctx_timeout;
        
        verify_mode = o.verify_mode;
        verify_depth = o.verify_depth;
        
        x509_verify_cb = o.x509_verify_cb;
        x509_chk_issued_cb = o.x509_chk_issued_cb;
        decrypt_cb = o.decrypt_cb;
        
        store_flags = o.store_flags;
        
        cache_type = o.cache_type;
        cache_timeout = o.cache_timeout;
        cache_size = o.cache_size;
        cache_init_cb = o.cache_init_cb;
        cache_args = o.cache_args;
        
        o.ciphers = _EVMVC_DEF_SSL_CIPHERS;
        o.named_curve = _EVMVC_DEF_SSL_NC;
        
        o.ssl_opts = _EVMVC_DEF_SSL_OPTS;
        o.ssl_ctx_timeout = 0;
        
        o.verify_mode = _EVMVC_DEF_SSL_VERIF_MODE;
        o.verify_depth = 0;
        
        o.x509_verify_cb = nullptr;
        o.x509_chk_issued_cb = nullptr;
        o.decrypt_cb = nullptr;
        
        o.store_flags = 0;
        
        o.cache_type = _EVMVC_DEF_SSL_CACHE_TYPE;
        o.cache_timeout = 0;
        o.cache_size = 0;
        o.cache_init_cb = nullptr;
        o.cache_args = nullptr;
        
        return *this;
    }
    
    ssl_options(const evmvc::json& cfg)
    { 
        
    }
    
    ~ssl_options()
    {
    }
    
    std::string cert_file;
    std::string cert_key_file;
    std::string cafile;
    std::string capath;
    
    std::string ciphers;
    std::string named_curve;
    std::string dhparams;
    
    long ssl_opts;
    long ssl_ctx_timeout;
    
    evmvc::ssl_verify_mode verify_mode;
    int verify_depth;

    SSL_verify_cb x509_verify_cb;
    X509_STORE_CTX_check_issued_fn x509_chk_issued_cb;
    evmvc::ssl_decrypt_cb decrypt_cb;
    
    unsigned long store_flags;
    
    evmvc::ssl_cache_type cache_type;
    long cache_timeout;
    long cache_size;
    evmvc::ssl_scache_init cache_init_cb;
    void* cache_args;
};
#undef _EVMVC_DEF_SSL_OPTS
#undef _EVMVC_DEF_SSL_VERIF_MODE
#undef _EVMVC_DEF_SSL_CACHE_TYPE


class listen_options
{
public:
    listen_options()
    {
    }
    
    listen_options(const listen_options& o)
        : address(o.address),
        port(o.port),
        ssl(o.ssl),
        backlog(o.backlog)
    {
    }

    listen_options(listen_options&& o)
        : address(std::move(o.address)),
        port(o.port),
        ssl(o.ssl),
        backlog(o.backlog)
    {
        o.port = 80;
        o.ssl = false;
        o.backlog = -1;
    }
    
    listen_options& operator=(listen_options&& o)
    {
        address = std::move(o.address);
        port = o.port;
        ssl = o.ssl;
        backlog = o.backlog;
        
        o.port = 80;
        o.ssl = false;
        o.backlog = -1;
        
        return *this;
    }
    
    std::string address = "*";
    uint16_t port = 80;
    bool ssl = false;
    int32_t backlog = -1;
};

class server_options
{
public:
    server_options()
    {
    }
    
    server_options(const server_options& o)
        : name(o.name),
        aliases(o.aliases),
        listeners(o.listeners),
        ssl(o.ssl),
        atimeo(o.atimeo),
        rtimeo(o.rtimeo),
        wtimeo(o.wtimeo)
    {
    }
    
    server_options(server_options&& o)
        : name(std::move(o.name)),
        aliases(std::move(o.aliases)),
        listeners(std::move(o.listeners)),
        ssl(std::move(o.ssl)),
        atimeo(o.atimeo),
        rtimeo(o.rtimeo),
        wtimeo(o.wtimeo)
    {
        o.atimeo = {3,0};
        o.rtimeo = {3,0};
        o.wtimeo = {3,0};
    }
    
    server_options& operator=(server_options&& o)
    {
        name = std::move(o.name);
        aliases = std::move(o.aliases);
        listeners = std::move(o.listeners);
        ssl = std::move(ssl);
        
        atimeo = o.atimeo;
        rtimeo = o.rtimeo;
        wtimeo = o.wtimeo;

        o.atimeo = {3,0};
        o.rtimeo = {3,0};
        o.wtimeo = {3,0};
        
        return *this;
    }
    
    std::string name = "";
    std::vector<std::string> aliases;
    std::vector<listen_options> listeners;
    ssl_options ssl;
    
    struct timeval atimeo = {3,0};
    struct timeval rtimeo = {3,0};
    struct timeval wtimeo = {3,0};
};

class app_options
{
public:
    app_options()
        : base_dir(bfs::current_path()),
        view_dir(base_dir / "views"),
        temp_dir(base_dir / "temp"),
        cache_dir(base_dir / "cache"),
        log_dir(base_dir / "logs"),
        use_default_logger(true),
        
        log_console_level(log_level::warning),
        log_console_enable_color(true),
        log_file_level(log_level::warning),
        log_file_max_size(1048576 * 5),
        log_file_max_files(7),
        stack_trace_enabled(false),
        worker_count(get_nprocs_conf())
    {
    }

    app_options(const bfs::path& base_directory)
        : base_dir(base_directory),
        view_dir(base_dir / "views"),
        temp_dir(base_dir / "temp"),
        cache_dir(base_dir / "cache"),
        log_dir(base_dir / "logs"),
        use_default_logger(true),
        
        log_console_level(log_level::warning),
        log_console_enable_color(true),
        log_file_level(log_level::warning),
        log_file_max_size(1048576 * 5),
        log_file_max_files(7),
        stack_trace_enabled(false),
        worker_count(get_nprocs_conf())
    {
    }
    
    app_options(const evmvc::app_options& other)
        : base_dir(other.base_dir), 
        view_dir(other.view_dir),
        temp_dir(other.temp_dir),
        cache_dir(other.cache_dir),
        log_dir(other.log_dir),
        use_default_logger(other.use_default_logger),
        
        log_console_level(other.log_console_level),
        log_console_enable_color(other.log_console_enable_color),
        log_file_level(other.log_file_level),
        log_file_max_size(other.log_file_max_size),
        log_file_max_files(other.log_file_max_files),
        stack_trace_enabled(other.stack_trace_enabled),
        worker_count(other.worker_count),
        servers(other.servers)
    {
    }
    
    app_options(evmvc::app_options&& other)
        : base_dir(std::move(other.base_dir)),
        view_dir(std::move(other.view_dir)),
        temp_dir(std::move(other.temp_dir)),
        cache_dir(std::move(other.cache_dir)),
        log_dir(std::move(other.log_dir)),
        use_default_logger(other.use_default_logger),
        
        log_console_level(other.log_console_level),
        log_console_enable_color(other.log_console_enable_color),
        log_file_level(other.log_file_level),
        log_file_max_size(other.log_file_max_size),
        log_file_max_files(other.log_file_max_files),
        stack_trace_enabled(other.stack_trace_enabled),
        worker_count(other.worker_count),
        servers(std::move(other.servers))
    {
        other.use_default_logger = true;
        other.log_console_level = log_level::warning;
        other.log_console_enable_color = true;
        other.log_file_level = log_level::warning;
        other.log_file_max_size = 1048576 * 5;
        other.log_file_max_files = 7;
        other.stack_trace_enabled = false;
        other.worker_count = get_nprocs_conf();
    }
    
    app_options& operator=(app_options&& other)
    {
        base_dir = std::move(other.base_dir);
        view_dir = std::move(other.view_dir);
        temp_dir = std::move(other.temp_dir);
        cache_dir = std::move(other.cache_dir);
        log_dir = std::move(other.log_dir);
        use_default_logger = other.use_default_logger;
        
        log_console_level = other.log_console_level;
        log_console_enable_color = other.log_console_enable_color;
        log_file_level = other.log_file_level;
        log_file_max_size = other.log_file_max_size;
        log_file_max_files = other.log_file_max_files;
        stack_trace_enabled = other.stack_trace_enabled;
        worker_count = other.worker_count;
        
        servers = std::move(other.servers);
        
        other.use_default_logger = true;
        other.log_console_level = log_level::warning;
        other.log_console_enable_color = true;
        other.log_file_level = log_level::warning;
        other.log_file_max_size = 1048576 * 5;
        other.log_file_max_files = 7;
        other.stack_trace_enabled = false;
        other.worker_count = get_nprocs_conf();
        
        return *this;
    }
    
    bfs::path base_dir;
    bfs::path view_dir;
    bfs::path temp_dir;
    bfs::path cache_dir;
    bfs::path log_dir;
    
    bool use_default_logger;
    
    log_level log_console_level;
    bool log_console_enable_color;
    
    log_level log_file_level;
    size_t log_file_max_size;
    size_t log_file_max_files;
    
    bool stack_trace_enabled;
    size_t worker_count;
    
    std::vector<server_options> servers;
};

} // ns: evmvmc
#endif //_libevmvc_configuration_h
