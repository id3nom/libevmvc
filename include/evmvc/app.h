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
#include "router.h"
#include "stack_debug.h"
#include "multipart_parser.h"

#include <sys/utsname.h>

namespace evmvc {

enum class app_state
{
    stopped,
    starting,
    running,
    stopping,
};

class app_options
{
public:
    app_options()
        : base_dir(boost::filesystem::current_path()),
        view_dir(base_dir / "views"),
        temp_dir(base_dir / "temp")
    {
    }

    app_options(const boost::filesystem::path& base_directory)
        : base_dir(base_directory),
        view_dir(base_dir / "views"),
        temp_dir(base_dir / "temp")
    {
    }
    
    app_options(const evmvc::app_options& other)
        : base_dir(other.base_dir), 
        view_dir(other.view_dir),
        temp_dir(other.temp_dir)
    {
    }
    
    app_options(evmvc::app_options&& other)
        : base_dir(std::move(other.base_dir)),
        view_dir(std::move(other.view_dir)),
        temp_dir(std::move(other.temp_dir))
    {
    }
    
    boost::filesystem::path base_dir;
    boost::filesystem::path view_dir;
    boost::filesystem::path temp_dir;
};

class app
    : public std::enable_shared_from_this<app>
{
    friend void _internal::on_app_request(evhtp_request_t* req, void* arg);
    
public:
    
    app(
        evmvc::app_options&& opts)
        : _status(app_state::stopped),
        _options(std::move(opts)),
        _router(std::make_shared<router>("/")),
        _evbase(nullptr), _evhtp(nullptr)
    {
        _router->_path = "";
        std::clog << "Starting app\n" << app::version() << std::endl;
    }
    
    ~app()
    {
        _router.reset();
        
        if(this->running())
            this->stop();
    }
    
    app_options& options(){ return _options;}
    
    app_state status()
    {
        return _status;
    }
    
    bool stopped(){ return _status == app_state::stopped;}
    bool starting(){ return _status == app_state::starting;}
    bool running(){ return _status == app_state::running;}
    bool stopping(){ return _status == app_state::stopping;}
    
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
                "   Boost v{}\n"
                "   {}\n" //openssl
                "   zlib v{}\n"
                "   libpcre v{}.{} {}\n"
                "   libevent v{}\n"
                "   libevhtp v{}\n",
                EVMVC_VERSION_NAME,
                __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
                uts.sysname, uts.release, uts.version, uts.machine,
                __DATE__,
                BOOST_LIB_VERSION,
                OPENSSL_VERSION_TEXT,
                ZLIB_VERSION,
                PCRE_MAJOR, PCRE_MINOR, EVMVC_STRING(PCRE_DATE),
                _EVENT_VERSION,
                EVHTP_VERSION
            );
        }
        
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
        
        // ensure app dir exists
        // auto fs = boost::filesystem::status(_options.base_dir);
        // if(!boost::filesystem::exists(fs) || 
        //     !boost::filesystem::is_directory(fs)){
        //
        //     if(boost::filesystem::create_directories(_options.base_dir))
        //         std::clog << fmt::format(
        //             "Creating base_dir '{}'\n", _options.base_dir.c_str()
        //         );
        // }
        if(boost::filesystem::create_directories(_options.base_dir))
            std::clog << fmt::format(
                "Creating base directory '{}'\n", _options.base_dir.c_str()
            );
        if(boost::filesystem::create_directories(_options.temp_dir))
            std::clog << fmt::format(
                "Creating temp directory '{}'\n", _options.temp_dir.c_str()
            );
        if(boost::filesystem::create_directories(_options.view_dir))
            std::clog << fmt::format(
                "Creating view directory '{}'\n", _options.view_dir.c_str()
            );
        
        _evhtp = evhtp_new(_evbase, NULL);
        //evhtp_set_gencb(_evhtp, _internal::on_app_request, this);
        evhtp_callback_t* cb = evhtp_set_glob_cb(
            _evhtp, "*", _internal::on_app_request, this
        );
        evhtp_callback_set_hook(
            cb, evhtp_hook_on_headers, (evhtp_hook)_internal::on_headers, this
        );
        
        evhtp_enable_flag(_evhtp, EVHTP_FLAG_ENABLE_ALL);
        evhtp_bind_socket(_evhtp, address.data(), port, backlog);
        
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
    
    sp_router add_route_handler(
        const evmvc::string_view& verb,
        const evmvc::string_view& route_path,
        route_handler_cb cb)
    {
        return _router->add_route_handler(verb, route_path, cb);
    }
    
    sp_router add_router(sp_router router)
    {
        return _router->add_router(router);
    }
    
private:
    
    app_state _status;
    app_options _options;
    sp_router _router;
    struct event_base* _evbase;
    struct evhtp* _evhtp;
    
};

void _internal::send_error(
    evmvc::app* app, evhtp_request_t *req, int status_code,
    evmvc::string_view msg)
{
    evmvc::sp_http_cookies c = std::make_shared<evmvc::http_cookies>(req);
    evmvc::response res(app->shared_from_this(), req, c);
    
    res.error(
        (evmvc::status)status_code,
        EVMVC_ERR(msg.data())
    );
}

void _internal::send_error(
    evmvc::app* app, evhtp_request_t *req, int status_code,
    evmvc::cb_error err)
{
    evmvc::sp_http_cookies c = std::make_shared<evmvc::http_cookies>(req);
    evmvc::response res(app->shared_from_this(), req, c);
    
    res.error((evmvc::status)status_code, err);
}


evhtp_res _internal::on_headers(
    evhtp_request_t* req, evhtp_headers_t* hdr, void* arg)
{
    if(evmvc::_internal::is_multipart_data(req, hdr))
        return evmvc::_internal::parse_multipart_data(
            req, hdr, (app*)arg,
            ((app*)arg)->options().temp_dir
        );
    return EVHTP_RES_OK;
}

void _internal::on_multipart_request(evhtp_request_t* req, void* arg)
{
    std::clog << "on_multipart_request\n";
    auto mp = (evmvc::_internal::multipart_parser*)arg;
    _internal::on_app_request(req, mp->app);
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
        sv = evmvc::method_to_string(v);
    
    auto rr = a->_router->resolve_url(v, req->uri->path->full);
    if(!rr && v == evmvc::method::head)
        rr = a->_router->resolve_url(evmvc::method::get, req->uri->path->full);
    
    evmvc::sp_http_cookies c = std::make_shared<evmvc::http_cookies>(req);
    evmvc::response res(a->shared_from_this(), req, c);
    
    try{
        if(!rr){
            res.error(
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
        [&rr, &req, &res](auto error){
            if(error){
                res.error(evmvc::status::internal_server_error, error);
                return;
            }
        });
    }catch(const std::exception& err){
        
        char* what = evhttp_htmlescape(err.what());
        std::string err_msg = fmt::format(
            "<!DOCTYPE html><html><head><title>"
            "LIBEVMVC Error</title></head><body>"
            "<table>\n<tr><td style='background-color:red;font-size:1.1em;'>"
                "<b>Error summary</b>"
            "</td></tr>\n"
            "<tr><td style='color:red;'><b>Error {}, {}</b></td></tr>\n"
            "<tr><td style='color:red;'>{}</td></tr>\n",
            (int16_t)evmvc::status::internal_server_error,
            evmvc::statuses::status(
                (int16_t)evmvc::status::internal_server_error
            ).data(),
            what
        );
        free(what);
        
        try{
            auto se = dynamic_cast<const evmvc::stacked_error&>(err);
            
            char* file = evhttp_htmlescape(se.file().data());
            char* func = evhttp_htmlescape(se.func().data());
            char* stack = evhttp_htmlescape(se.stack().c_str());
            
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
                file, se.line(), func, stack
            );
            
            free(file);
            free(func);
            free(stack);
            
        }catch(...){}
        
        err_msg += fmt::format(
            "<tr><td style='"
            "border-top: 1px solid black; font-size:0.9em'>{}"
            "</td></tr>\n",
            app::version()
        );
        
        err_msg += "</table></body></html>";
        
        res.status(evmvc::status::internal_server_error).html(err_msg);
    }
}

void response::error(evmvc::status err_status, const cb_error& err)
{
    char* what = evhttp_htmlescape(err.c_str());
    std::string err_msg = fmt::format(
        "<!DOCTYPE html><html><head><title>"
        "LIBEVMVC Error</title></head><body>"
        "<table>\n<tr><td style='background-color:red;font-size:1.1em;'>"
            "<b>Error summary</b>"
        "</td></tr>\n"
        "<tr><td style='color:red;'><b>Error {}, {}</b></td></tr>\n"
        "<tr><td style='color:red;'>{}</td></tr>\n",
        (int16_t)evmvc::status::internal_server_error,
        evmvc::statuses::status(
            (int16_t)evmvc::status::internal_server_error
        ).data(),
        what
    );
    free(what);
    
    if(err.has_stack()){
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
        app::version()
    );
    
    err_msg += "</table></body></html>";
    
    this->status(err_status).html(err_msg);
}

} // ns evmvc
#endif //_libevmvc_app_h
