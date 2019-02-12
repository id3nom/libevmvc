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
    
    app(struct event_base* ev_base,
        evmvc::app_options&& opts)
        : _init_rtr(false), _init_dirs(false), _status(app_state::stopped),
        _options(std::move(opts)),
        _router(),
        _ev_base(ev_base), _evhtp(nullptr)
    {
        // init the default logger
        if(_options.use_default_logger){
            std::vector<evmvc::sinks::sp_logger_sink> sinks;
            
            auto out_sink = std::make_shared<evmvc::sinks::console_sink>(
                _options.log_console_enable_color
            );
            out_sink->set_level(_options.log_console_level);
            sinks.emplace_back(out_sink);
            
            bfs::create_directories(_options.log_dir);
            auto file_sink = std::make_shared<evmvc::sinks::rotating_file_sink>(
                _ev_base,
                (_options.log_dir / "libevmvc.log").c_str(),
                _options.log_file_max_size,
                _options.log_file_max_files
            );
            file_sink->set_level(_options.log_file_level);
            sinks.emplace_back(file_sink);
            
            _log = std::make_shared<evmvc::logger>(
                "/", sinks.begin(), sinks.end()
            );
            _log->set_level(
                std::max(
                    _options.log_file_level, _options.log_console_level
                )
            );
        }else
            _log = _internal::default_logger()->add_child(
                "/"
            );
        
        this->_log->info("Starting app\n{}", app::version());
    }
    
    ~app()
    {
        _router.reset();
        
        if(this->running())
            this->stop();
    }
    
    app_options& options(){ return _options;}
    
    void replace_logger(evmvc::sp_logger l)
    {
        _log = l;
    }
    evmvc::sp_logger log()
    {
        return _log;
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
        uint16_t port = 8080,
        const evmvc::string_view& address = "0.0.0.0",
        int backlog = -1)
    {
        if(!stopped())
            throw std::runtime_error(
                "app must be in stopped state to start listening again"
            );
        _status = app_state::starting;
        
        this->_init_directories();
        
        _evhtp = evhtp_new(_ev_base, NULL);
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
        
        this->_log->info(
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
        this->_init_router();
        return _router->all(route_path, cb);
    }
    sp_router options(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        this->_init_router();
        return _router->options(route_path, cb);
    }
    sp_router del(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        this->_init_router();
        return _router->del(route_path, cb);
    }
    sp_router head(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        this->_init_router();
        return _router->head(route_path, cb);
    }
    sp_router get(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        this->_init_router();
        return _router->get(route_path, cb);
    }
    sp_router post(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        this->_init_router();
        return _router->post(route_path, cb);
    }
    sp_router put(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        this->_init_router();
        return _router->put(route_path, cb);
    }
    sp_router connect(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        this->_init_router();
        return _router->connect(route_path, cb);
    }
    sp_router trace(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        this->_init_router();
        return _router->trace(route_path, cb);
    }
    sp_router patch(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        this->_init_router();
        return _router->patch(route_path, cb);
    }
    
    sp_router register_route_handler(
        const evmvc::string_view& verb,
        const evmvc::string_view& route_path,
        route_handler_cb cb)
    {
        this->_init_router();
        return _router->register_route_handler(verb, route_path, cb);
    }
    
    sp_router register_router(sp_router router)
    {
        this->_init_router();
        return _router->register_router(router);
    }
    
private:
    
    inline void _init_router()
    {
        if(_init_rtr)
            return;
        _init_rtr = true;
        
        _router =
            std::make_shared<router>(
                this->shared_from_this(), "/"
            );
        _router->_path = "";
    }
    
    inline void _init_directories()
    {
        if(_init_dirs)
            return;
        _init_dirs = true;
        
        if(bfs::create_directories(_options.base_dir))
            this->_log->info(
                "Creating base directory '{}'\n", _options.base_dir.c_str()
            );
        if(bfs::create_directories(_options.temp_dir))
            this->_log->info(
                "Creating temp directory '{}'\n", _options.temp_dir.c_str()
            );
        if(bfs::create_directories(_options.view_dir))
            this->_log->info(
                "Creating view directory '{}'\n", _options.view_dir.c_str()
            );
        if(bfs::create_directories(_options.cache_dir))
            this->_log->info(
                "Creating cache directory '{}'\n", _options.cache_dir.c_str()
            );
        
        this->_init_router();
    }
    
    bool _init_rtr;
    bool _init_dirs;
    app_state _status;
    app_options _options;
    sp_router _router;
    struct event_base* _ev_base;
    struct evhtp* _evhtp;
    evmvc::sp_logger _log;
};//evmvc::app

sp_response response::null(
    wp_app a,
    evhtp_request_t* ev_req)
{
    auto res = _internal::create_http_response(
        a, ev_req, http_params()
    );
    
    return res;
}

evmvc::sp_logger route::log() const
{
    if(!_log)
        _log = _rtr.lock()->log()->add_child(this->_rp);
    return _log;
}

router::router(evmvc::wp_app app)
    : _app(app), 
    _path(_norm_path("")),
    _log(_app.lock()->log()->add_child(_path)),
    _parent(nullptr)
    /*,
    _match_first(boost::indeterminate),
    _match_strict(boost::indeterminate),
    _match_case(boost::indeterminate)
    */
{
}

router::router(evmvc::wp_app app, const evmvc::string_view& path)
    : _app(app),
    _path(_norm_path(path)),
    _log(_app.lock()->log()->add_child(_path)),
    _parent(nullptr)
    /*,
    _match_first(boost::indeterminate),
    _match_strict(boost::indeterminate),
    _match_case(boost::indeterminate)
    */
{
}



void _internal::send_error(
    evmvc::app* app, evhtp_request_t *req, int status_code,
    evmvc::string_view msg)
{
    auto res = _internal::create_http_response(
        app->shared_from_this(), req, http_params()
    );
    
    res->error(
        (evmvc::status)status_code,
        EVMVC_ERR(msg.data())
    );
}

void _internal::send_error(
    evmvc::app* app, evhtp_request_t *req, int status_code,
    evmvc::cb_error err)
{
    auto res = _internal::create_http_response(
        app->shared_from_this(), req, http_params()
    );
    
    res->error((evmvc::status)status_code, err);
}

evhtp_res _internal::on_headers(
    evhtp_request_t* req, evhtp_headers_t* hdr, void* arg)
{
    app* a = (app*)arg;
    
    if(evmvc::_internal::is_multipart_data(req, hdr))
        return evmvc::_internal::parse_multipart_data(
            a->log(),
            req, hdr, a,
            a->options().temp_dir
        );
    return EVHTP_RES_OK;
}

void _internal::on_multipart_request(evhtp_request_t* req, void* arg)
{
    auto mp = (evmvc::_internal::multipart_parser*)arg;
    _internal::on_app_request(req, mp->app);
    mp->app->log()->trace("_internal::on_multipart_request");
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
    
    try{
        if(!rr){
            response::null(a->shared_from_this(), req)->error(
                evmvc::status::not_found,
                EVMVC_ERR(
                    "Unable to find ressource at '{}'",
                    req->uri->path->full
                )
            );
            return;
        }
        
        rr->execute(req,
        [a, &rr, req/*, res*/](auto error){
            if(error){
                response::null(a->shared_from_this(), req)->error(
                    evmvc::status::internal_server_error, error
                );
                return;
            }
        });
    }catch(const std::exception& err){
        response::null(a->shared_from_this(), req)->error(
            evmvc::status::internal_server_error,
            EVMVC_ERR(err.what())
        );
    }
}

void response::error(evmvc::status err_status, const cb_error& err)
{
    auto con_addr_in = (sockaddr_in*)this->_ev_req->conn->saddr;
    
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
    
    evmvc::sp_app a = this->get_route()->get_router()->get_app();
    if(a->options()
        .stack_trace_enabled && err.has_stack()
    ){
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
        this->log()->info(log_val);
    else if((int16_t)err_status < 500)
        this->log()->warn(log_val);
    else
        this->log()->error(log_val);
    
    this->status(err_status).html(err_msg);
}

sp_route& route::null(wp_app a)
{
    static sp_route rt = std::make_shared<route>(
        router::null(a), "null"
    );
    
    return rt;
}

sp_router& router::null(wp_app a)
{
    static sp_router rtr = std::make_shared<router>(
        a.lock(), "null"
    );
    
    return rtr;
}


namespace _internal{
    inline evmvc::sp_response create_http_response(
        wp_app a,
        evhtp_request_t* ev_req,
        const evmvc::http_params& params = http_params()
        )
    {
        return _internal::create_http_response(
            a.lock()->log(), ev_req, evmvc::route::null(a), params
        );
    }
    
    inline evmvc::sp_response create_http_response(
        sp_logger log,
        evhtp_request_t* ev_req,
        sp_route rt,
        const evmvc::http_params& params = http_params()
        )
    {
        static uint64_t cur_id = 0;
        uint64_t rid = ++cur_id;
        
        evmvc::sp_http_cookies cks = std::make_shared<evmvc::http_cookies>(
            rid, rt, log, ev_req
        );
        evmvc::sp_response res = std::make_shared<evmvc::response>(
            rid, rt, log, ev_req, cks
        );
        evmvc::sp_request req = std::make_shared<evmvc::request>(
            rid, rt, log, ev_req, cks, params
        );
        res->_req = req;
        return res;
    }
}// ns: evmvc::internal
}// ns evmvc
#endif //_libevmvc_app_h
