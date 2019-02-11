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

#ifndef _libevmvc_app_h
#define _libevmvc_app_h

#include "stable_headers.h"
#include "app_options.h"
#include "router.h"
#include "stack_debug.h"
#include "multipart_parser.h"

#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define EVMVC_PCRE_DATE EVMVC_STRING(PCRE_DATE)

namespace evmvc {

enum class app_state
{
    stopped,
    starting,
    running,
    stopping,
};

//https://chromium.googlesource.com/external/github.com/ellzey/libevhtp/+/libevhtp2/src/evhtp2/ws/

class app
    : public std::enable_shared_from_this<app>
{
    friend void _internal::on_app_request(evhtp_request_t* req, void* arg);
    
public:
    
    app(
        evmvc::app_options&& opts)
        : _status(app_state::stopped),
        _options(std::move(opts)),
        _router(),
        _evbase(nullptr), _evhtp(nullptr)
    {
        _router =
            std::make_shared<router>(
                this, "/"
            );
        
        _router->_path = "";
        
        // init the default logger
        if(_options.use_default_logger){
            //https://github.com/gabime/spdlog
            
            std::vector<spdlog::sink_ptr> sinks;
            auto stdout_sink =
                std::make_shared<spdlog::sinks::stdout_sink_mt>();
            if(_options.log_console_enable_color)
                sinks.emplace_back(
                    std::make_shared<spdlog::sinks::ansicolor_sink>(
                        stdout_sink
                    )
                );
            else
                sinks.emplace_back(stdout_sink);
            
            sinks[sinks.size()-1]->set_level(
                _options.log_console_level
            );
            
            boost::filesystem::create_directories(_options.log_dir);
            sinks.emplace_back(
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    (_options.log_dir / "libevmvc").c_str(), "log",
                    _options.log_file_max_size,
                    _options.log_file_max_files
                )
            );
            
            sinks[sinks.size()-1]->set_level(
                _options.log_file_level
            );
            
            _logger = std::make_shared<spdlog::logger>(
                "libevmvc", sinks.begin(), sinks.end()
            );
            _logger->flush_on(spdlog::level::warn);
            _logger->set_level(
                std::min(
                    _options.log_file_level, _options.log_console_level
                )
            );
        }
        
        this->info("Starting app\n{}", app::version());
    }
    
    ~app()
    {
        if(_logger)
            _logger->flush();
        
        _router.reset();
        
        if(this->running())
            this->stop();
    }
    
    app_options& options(){ return _options;}
    
    void replace_logger(std::shared_ptr<spdlog::logger> logger)
    {
        _logger = logger;
    }
    std::shared_ptr<spdlog::logger> log()
    {
        return _logger;
    }
    
    app_state status() const
    {
        return _status;
    }
    
    bool stopped() const { return _status == app_state::stopped;}
    bool starting() const { return _status == app_state::starting;}
    bool running() const { return _status == app_state::running;}
    bool stopping() const { return _status == app_state::stopping;}
    
    static std::string version()
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
                "    libevent v{}\n"
                "    libicu v{}\n"
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
    
    static std::string html_version()
    {
        static std::string ver =
            evmvc::replace_substring_copy(
                evmvc::replace_substring_copy(version(), "\n", "<br/>"),
                " ", "&nbsp;"
            );
        return ver;
    }
    
    void listen(
        event_base* evbase,
        uint16_t port = 8080,
        const evmvc::string_view& address = "0.0.0.0",
        int backlog = -1)
    {
        if(!stopped())
            throw std::runtime_error(
                "app must be in stopped state to start listening again"
            );
        
        _evbase = evbase;
        listen(port, address);
    }
    
    void listen(
        uint16_t port = 8080,
        const evmvc::string_view& address = "0.0.0.0",
        int backlog = -1)
    {
        if(!stopped())
            throw std::runtime_error(
                "app must be in stopped state to start listening again"
            );
        _status = app_state::starting;
        
        this->_init();
        
        _evhtp = evhtp_new(_evbase, NULL);
        //evhtp_set_gencb(_evhtp, _internal::on_app_request, this);
        evhtp_callback_t* cb = evhtp_set_glob_cb(
            _evhtp, "*", _internal::on_app_request, this
        );
        evhtp_callback_set_hook(
            cb, evhtp_hook_on_headers, (evhtp_hook)_internal::on_headers, this
        );
        
        evhtp_enable_flag(_evhtp, EVHTP_FLAG_ENABLE_ALL);
        /* create 1 listener, 4 acceptors */
        evhtp_use_threads_wexit(_evhtp, NULL, NULL, 8, NULL);
        
        evhtp_bind_socket(_evhtp, address.data(), port, backlog);
        
        this->info(
            "EVMVC is listening at '{}://{}:{}'\n",
            _options.secure ? "https" : "http",
            address.data(), port
        );
        
        _status = app_state::running;
    }
    
    void stop()
    {
        if(stopped() || stopping())
            throw std::runtime_error(
                "app must be in running state to be able to stop it."
            );
        this->_status = app_state::stopping;
        evhtp_unbind_socket(_evhtp);
        evhtp_free(_evhtp);
        this->_status = app_state::stopped;
    }
    
    sp_router all(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->all(route_path, cb);
    }
    sp_router options(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->options(route_path, cb);
    }
    sp_router del(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->del(route_path, cb);
    }
    sp_router head(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->head(route_path, cb);
    }
    sp_router get(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->get(route_path, cb);
    }
    sp_router post(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->post(route_path, cb);
    }
    sp_router put(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->put(route_path, cb);
    }
    sp_router connect(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->connect(route_path, cb);
    }
    sp_router trace(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->trace(route_path, cb);
    }
    sp_router patch(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->patch(route_path, cb);
    }
    
    sp_router register_route_handler(
        const evmvc::string_view& verb,
        const evmvc::string_view& route_path,
        route_handler_cb cb)
    {
        return _router->register_route_handler(verb, route_path, cb);
    }
    
    sp_router register_router(sp_router router)
    {
        return _router->register_router(router);
    }
    
private:
    
    void _init()
    {
        if(boost::filesystem::create_directories(_options.base_dir))
            this->info(
                "Creating base directory '{}'\n", _options.base_dir.c_str()
            );
        if(boost::filesystem::create_directories(_options.temp_dir))
            this->info(
                "Creating temp directory '{}'\n", _options.temp_dir.c_str()
            );
        if(boost::filesystem::create_directories(_options.view_dir))
            this->info(
                "Creating view directory '{}'\n", _options.view_dir.c_str()
            );
        if(boost::filesystem::create_directories(_options.cache_dir))
            this->info(
                "Creating cache directory '{}'\n", _options.cache_dir.c_str()
            );
    }
    
    app_state _status;
    app_options _options;
    sp_router _router;
    struct event_base* _evbase;
    struct evhtp* _evhtp;
    std::shared_ptr<spdlog::logger> _logger;
};

std::shared_ptr<spdlog::logger> route::log() const
{
    return _app.lock()->log();
}
evmvc::sp_app router::app() const
{
    return _app.lock();
}
std::shared_ptr<spdlog::logger> router::log() const
{
    return _app.lock()->log();
}


void _internal::send_error(
    evmvc::app* app, evhtp_request_t *req, int status_code,
    evmvc::string_view msg)
{
    evmvc::sp_http_cookies c = std::make_shared<evmvc::http_cookies>(
        app->log(), req
    );
    evmvc::response res(
        app->shared_from_this(), app->log(),
        req, c
    );
    
    res.error(
        (evmvc::status)status_code,
        EVMVC_ERR(msg.data())
    );
}

void _internal::send_error(
    evmvc::app* app, evhtp_request_t *req, int status_code,
    evmvc::cb_error err)
{
    evmvc::sp_http_cookies c = std::make_shared<evmvc::http_cookies>(
        app->log(), req
    );
    evmvc::response res(
        app->shared_from_this(), app->log(), req, c
    );
    
    res.error((evmvc::status)status_code, err);
}


evhtp_res _internal::on_headers(
    evhtp_request_t* req, evhtp_headers_t* hdr, void* arg)
{
    if(evmvc::_internal::is_multipart_data(req, hdr))
        return evmvc::_internal::parse_multipart_data(
            ((app*)arg)->log(),
            req, hdr, (app*)arg,
            ((app*)arg)->options().temp_dir
        );
    return EVHTP_RES_OK;
}

void _internal::on_multipart_request(evhtp_request_t* req, void* arg)
{
    auto mp = (evmvc::_internal::multipart_parser*)arg;
    _internal::on_app_request(req, mp->app);
    mp->app->trace("_internal::on_multipart_request");
}

void _internal::on_app_request(evhtp_request_t* req, void* arg)
{
    app* a = (app*)arg;
    
    evmvc::method v = (evmvc::method)req->method;
    evmvc::string_view sv;
    
    if(v == evmvc::method::unknown)
        throw EVMVC_ERR("Unknown method are not implemented!");
        //sv = "";// req-> req.method_string();
    else
        sv = evmvc::to_string(v);
    
    auto rr = a->_router->resolve_url(v, req->uri->path->full);
    if(!rr && v == evmvc::method::head)
        rr = a->_router->resolve_url(evmvc::method::get, req->uri->path->full);
    
    evmvc::sp_http_cookies c = std::make_shared<evmvc::http_cookies>(
        a->log(), req
    );
    evmvc::sp_response res = std::make_shared<evmvc::response>(
        a->shared_from_this(), a->log(), req, c
    );
    
    try{
        if(!rr){
            res->error(
                evmvc::status::not_found,
                EVMVC_ERR(
                    "Unable to find ressource at '{}'",
                    req->uri->path->full
                )
            );
            //res.send_status(evmvc::status::not_found);
            return;
        }
        
        rr->execute(req, res,
        [&rr, req, res](auto error){
            if(error){
                res->error(evmvc::status::internal_server_error, error);
                return;
            }
        });
    }catch(const std::exception& err){
        res->error(evmvc::status::internal_server_error, err);
    }
}

void response::error(evmvc::status err_status, const cb_error& err)
{
    auto con_addr_in = (sockaddr_in*)this->_ev_req->conn->saddr;
    // char* ip_addr = inet_ntoa(con_addr_in->sin_addr);
    
    std::string log_val = fmt::format(
        "[{}] [{}] [{}{}{}] [Status {} {}, {}]\n{}",
        inet_ntoa(con_addr_in->sin_addr),
        to_string((evmvc::method)this->_ev_req->method).data(),
        
        this->_ev_req->uri->path->full,
        this->_ev_req->uri->query_raw ? "?" : "",
        this->_ev_req->uri->query_raw ?
            (const char*)this->_ev_req->uri->query_raw : 
            "",
        
        (int16_t)err_status,
        evmvc::statuses::category(err_status).data(),
        evmvc::statuses::status(err_status).data(),
        err.c_str()
    );
    
    char* what = evhttp_htmlescape(err.c_str());
    std::string err_msg = fmt::format(
        "<!DOCTYPE html><html><head><title>"
        "LIBEVMVC Error</title></head><body>"
        "<table>\n<tr><td style='background-color:{0};font-size:1.1em;'>"
            "<b>Error summary</b>"
        "</td></tr>\n"
        "<tr><td style='color:{0};'><b>Error {1}, {2}</b></td></tr>\n"
        "<tr><td style='color:{0};'>{3}</td></tr>\n",
        evmvc::statuses::color(err_status).data(),
        (int16_t)err_status,
        evmvc::statuses::status(err_status).data(),
        what
    );
    free(what);
    
    if(this->_app->options().stack_trace_enabled && err.has_stack()){
        log_val += fmt::format(
            "\nAdditional info\n\n{}:{}\n{}\n\n{}\n",
            err.file(), err.line(), err.func(), err.stack()
        );
        
        char* file = evhttp_htmlescape(err.file().c_str());
        char* func = evhttp_htmlescape(err.func().c_str());
        char* stack = evhttp_htmlescape(err.stack().c_str());
        
        err_msg += fmt::format(
            "<tr><td>&nbsp;</td></tr>\n"
            "<tr><td style='"
                "border-bottom: 1px solid black;"
                "font-size:1.1em;"
                "'>"
                "<b>Additional info</b>"
            "</td></tr>\n"
            "<tr><td><pre>"
            "\n\n{}:{}\n{}\n\n{}"
            "</pre></td></tr>\n",
            file, err.line(), func, stack
        );
        
        free(file);
        free(func);
        free(stack);
    }
    
    err_msg += fmt::format(
        "<tr><td style='"
        "border-top: 1px solid black; font-size:0.9em'>{}"
        "</td></tr>\n",
        app::html_version()
    );
    
    err_msg += "</table></body></html>";
    
    if((int16_t)err_status < 400)
        this->_app->info(log_val);
    else if((int16_t)err_status < 500)
        this->_app->warn(log_val);
    else
        this->_app->error(log_val);
    
    this->status(err_status).html(err_msg);
}

} // ns evmvc
#endif //_libevmvc_app_h
