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
#include "global.h"
#include "configuration.h"
#include "events.h"
#include "router.h"
#include "stack_debug.h"
#include "multipart_parser.h"

#include "worker.h"
#include "master_server.h"

namespace evmvc {

//https://chromium.googlesource.com/external/github.com/ellzey/libevhtp/+/libevhtp2/src/evhtp2/ws/

class app
    : public std::enable_shared_from_this<app>
{
    friend evhtp_res _internal::on_headers(
        evhtp_request_t* req, evhtp_headers_t* hdr, void* arg);
    friend void _internal::on_app_request(evhtp_request_t* req, void* arg);
    
public:
    
    app(struct event_base* ev_base,
        evmvc::app_options&& opts)
        : _init_rtr(false), _init_dirs(false), _status(running_state::stopped),
        _options(std::move(opts)),
        _router(),
        _ev_base(ev_base), _evhtp(nullptr)//, _ar(nullptr)
    {
        // force get_field_table initialization
        evmvc::detail::get_field_table();
        
        *evmvc::ev_base() = ev_base;
        *evmvc::thread_ev_base() = ev_base;
        
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
            _internal::default_logger() = _log;
        }else
            _log = _internal::default_logger()->add_child(
                "/"
            );
    }
    
    ~app()
    {
        evmvc::clear_events();
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
    
    const std::vector<sp_worker>& workers() const { return _workers;}
    
    running_state status() const
    {
        return _status;
    }
    bool stopped() const { return _status == running_state::stopped;}
    bool starting() const { return _status == running_state::starting;}
    bool running() const { return _status == running_state::running;}
    bool stopping() const { return _status == running_state::stopping;}
    
    bfs::path abs_path(evmvc::string_view rel_path)
    {
        std::string rp(rel_path.data(), rel_path.size());
        if(rp[0] == '~')
            return _options.base_dir / rp.substr(1);
        else if(rp[0] == '/')
            return rp;
        else
            return _options.base_dir / rp;
    }
    
    void start(bool start_event_loop = true)
    {
        if(!stopped())
            throw EVMVC_ERR(
                "app must be in stopped state to start listening again"
            );
        _status = running_state::starting;
        
        for(size_t i = 0; i < _options.worker_count; ++i){
            sp_http_worker w = std::make_shared<evmvc::http_worker>(
                this->shared_from_this(), _options
            );
            
        }
        
        for(auto w : _workers){
            int tryout = 0;
            do{
                w->channel().usock = _internal::unix_connect(
                    w->channel().usock_path.c_str(), SOCK_STREAM
                );
                if(w->channel().usock == -1){
                    _log->warn(EVMVC_ERR(
                        "unix connect err: {}", errno
                    ));
                }else
                    break;
                
                if(++tryout >= 5)
                    return _log->fatal(EVMVC_ERR(
                        "unable to connect to client: {}",
                        w->id()
                    ));
            }while(w->channel().usock == -1);
            _internal::unix_set_sock_opts(w->channel().usock);
        }
        
        for(auto s : _servers)
            s->start();
        
        if(start_event_loop){
            event_base_loop(global::ev_base(), 0);
            
            for(auto w : _workers)
                kill(w->pid(), SIGINT);
            _workers.clear();
            _servers.clear();
            
            event_base_free(global::ev_base());
        }
    }
    
    // void listen(
    //     uint16_t port = 8080,
    //     const evmvc::string_view& address = "0.0.0.0",
    //     int backlog = -1)
    // {
    //     if(!stopped())
    //         throw std::runtime_error(
    //             "app must be in stopped state to start listening again"
    //         );
    //     _status = running_state::starting;
        
    //     this->_init_directories();
        
    //     _evhtp = evhtp_new(_ev_base, NULL);
        
    //     evhtp_callback_t* cb = evhtp_set_glob_cb(
    //         _evhtp, "*", _internal::on_app_noop_request, nullptr
    //     );
    //     evhtp_callback_set_hook(
    //         cb, evhtp_hook_on_headers, (evhtp_hook)_internal::on_headers, this
    //     );
        
    //     evhtp_enable_flag(_evhtp, EVHTP_FLAG_ENABLE_ALL);
        
    //     if(_options.worker_count > 1){
    //         this->_log->info(
    //             "Creating 1 listener, and {} acceptors",
    //             _options.worker_count
    //         );
    //         evhtp_use_threads_wexit(_evhtp,
    //             // called on init
    //             [](evhtp_t* htp, evthr_t* thread, void* arg)->void{
    //                 evmvc::warn("Loading thread '{}'", ::pthread_self());
    //                 *evmvc::thread_ev_base() = evthr_get_base(thread);
    //             },
                
    //             // called on exit
    //             [](evhtp_t* htp, evthr_t* thread, void* arg)->void{
    //                 *evmvc::thread_ev_base() = nullptr;
    //                 evmvc::warn("Exiting thread '{}'", ::pthread_self());
    //             },
                
    //             _options.worker_count, NULL
    //         );
    //     }
        
    //     evhtp_bind_socket(_evhtp, address.data(), port, backlog);
        
    //     this->_log->info(
    //         "EVMVC is listening at '{}://{}:{}'\n",
    //         _options.enable_ssl ? "https" : "http",
    //         address.data(), port
    //     );
        
    //     _status = running_state::running;
    // }
    
    void stop()
    {
        if(stopped() || stopping())
            throw EVMVC_ERR(
                "app must be in running state to be able to stop it."
            );
        this->_status = running_state::stopping;
        evhtp_unbind_socket(_evhtp);
        evhtp_free(_evhtp);
        
        this->_status = running_state::stopped;
    }
    
    #pragma region
    
    sp_router all(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->all(route_path, cb, pol);
    }
    sp_router options(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->options(route_path, cb, pol);
    }
    sp_router del(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->del(route_path, cb, pol);
    }
    sp_router head(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->head(route_path, cb, pol);
    }
    sp_router get(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->get(route_path, cb, pol);
    }
    sp_router post(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->post(route_path, cb, pol);
    }
    sp_router put(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->put(route_path, cb, pol);
    }
    sp_router connect(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->connect(route_path, cb, pol);
    }
    sp_router trace(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->trace(route_path, cb, pol);
    }
    sp_router patch(
        const evmvc::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->patch(route_path, cb, pol);
    }
    
    sp_router register_route_handler(
        const evmvc::string_view& verb,
        const evmvc::string_view& route_path,
        route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->register_route_handler(verb, route_path, cb, pol);
    }
    
    
    sp_router register_policy(policies::filter_policy pol)
    {
        return _router->register_policy(pol);
    }
    
    sp_router register_policy(
        const evmvc::string_view& route_path, policies::filter_policy pol)
    {
        return _router->register_policy(route_path, pol);
    }
    
    sp_router register_policy(
        evmvc::method method,
        const evmvc::string_view& route_path,
        policies::filter_policy pol)
    {
        return _router->register_policy(method, route_path, pol);
    }
    
    sp_router register_policy(
        const evmvc::string_view& method,
        const evmvc::string_view& route_path,
        policies::filter_policy pol)
    {
        return _router->register_policy(method, route_path, pol);
    }
    
    sp_router register_router(sp_router router)
    {
        this->_init_router();
        return _router->register_router(router);
    }
    
    #pragma endregion
    
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
    running_state _status;
    app_options _options;
    sp_router _router;
    struct event_base* _ev_base;
    struct evhtp* _evhtp;
    evmvc::sp_logger _log;
    
    std::vector<evmvc::sp_worker> _workers;
    std::vector<evmvc::sp_master_server> _servers;
};//evmvc::app


sp_response response::null(
    wp_app a,
    evhtp_request_t* ev_req)
{
    auto res = _internal::create_http_response(
        a, ev_req, std::vector<std::shared_ptr<evmvc::http_param>>()
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
    evmvc::sp_response res, int status_code,
    evmvc::string_view msg)
{
    res->error(
        (evmvc::status)status_code,
        EVMVC_ERR(msg.data())
    );
}

void _internal::send_error(
    evmvc::sp_response res, int status_code,
    evmvc::cb_error err)
{
    res->error((evmvc::status)status_code, err);
}

evhtp_res _internal::on_headers(
    evhtp_request_t* req, evhtp_headers_t* hdr, void* arg)
{
    app* a = (app*)arg;
    
    // search a valid route_result
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
    
    // get client ip
    auto con_addr_in = (sockaddr_in*)req->conn->saddr;
    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &con_addr_in->sin_addr, addr, INET_ADDRSTRLEN);
    
    // stop request if no valid route found
    if(!rr){
        a->log()->fail(
            "recv: [{}] [{}{}{}] from: [{}:{}]",
            to_string((evmvc::method)req->method),
            req->uri->path->full,
            req->uri->query_raw ? "?" : "",
            req->uri->query_raw ? (const char*)req->uri->query_raw : "",
            addr, ntohs(con_addr_in->sin_port)
        );
        
        response::null(a->shared_from_this(), req)->error(
            evmvc::status::not_found,
            EVMVC_ERR(
                "Unable to find ressource at '{}'",
                req->uri->path->full
            )
        );
        return EVHTP_RES_OK;
    }
    
    a->log()->success(
        "recv: [{}] [{}{}{}] from: [{}:{}]",
        to_string((evmvc::method)req->method),
        req->uri->path->full,
        req->uri->query_raw ? "?" : "",
        req->uri->query_raw ? (const char*)req->uri->query_raw : "",
        addr, ntohs(con_addr_in->sin_port)
    );
    
    // create the response
    auto res = _internal::create_http_response(
        rr->log(), req, rr->_route, rr->params
    );
    
    evmvc::_internal::request_args* ra = 
        new evmvc::_internal::request_args();
    ra->ready = false;
    ra->rr = rr;
    ra->res = res;
    
    // update request cb.
    req->cb = on_app_request;
    req->cbarg = ra;
    
    // create validation context
    evmvc::policies::filter_rule_ctx ctx = 
        evmvc::policies::new_context(res);
    
    //res->pause();
    rr->validate_access(
        ctx,
    [a, req, hdr, rr, res, ra](const evmvc::cb_error& err){
        res->resume();
        
        if(err){
            res->error(
                evmvc::status::unauthorized,
                err
            );
            //res->resume();
            return;
        }
        
        if(evmvc::_internal::is_multipart_data(req, hdr)){
            evmvc::_internal::parse_multipart_data(
                a->log(),
                req, hdr, ra,
                a->options().temp_dir
            );
        } else {
            if(ra->ready)
                _internal::on_app_request(req, ra);
            else
                ra->ready = true;
        }
    });
    
    return EVHTP_RES_OK;
    //return EVHTP_RES_PAUSE;
}

void _internal::on_multipart_request(evhtp_request_t* req, void* arg)
{
    req->hooks->on_request_fini_arg = nullptr;
    
    auto mp = (evmvc::_internal::multipart_parser*)arg;
    EVMVC_TRACE(mp->ra->res->log(), "Multipart parser task completed!");
    mp->ra->res->req()->_load_multipart_params(mp->root);
    
    evmvc::_internal::request_args* ra = mp->ra;
    delete mp;
    ra->ready = true;
    _internal::on_app_request(req, ra);
}

void _internal::on_app_noop_request(evhtp_request_t* /*req*/, void* /*arg*/)
{
}

void _internal::on_app_request(evhtp_request_t* req, void* arg)
{
    evmvc::_internal::request_args* ra = (evmvc::_internal::request_args*)arg;
    
    if(!ra->ready){
        ra->res->pause();
        ra->ready = true;
        return;
    }
    
    sp_app a = ra->res->get_app();
    sp_route_result rr = ra->rr;
    sp_response res = ra->res;
    delete ra;
    
    try{
        rr->execute(res, [res](auto error
        ){
            if(error){
                res->error(
                    evmvc::status::internal_server_error, error
                );
                return;
            }
        });
    }catch(const std::exception& err){
        res->error(
            evmvc::status::internal_server_error,
            EVMVC_ERR(err.what())
        );
    }
}


evmvc::sp_app request::get_app() const
{
    return this->get_router()->get_app();
}
evmvc::sp_router request::get_router() const
{
    return this->get_route()->get_router();
}

// std::string request::protocol() const
// {
//     //TODO: add trust proxy options
//     sp_header h = _headers->get("X-Forwarded-Proto");
//     if(h)
//         return h->value();
//    
//     return this->get_app()->options().enable_ssl ? "https" : "http";
// }

void request::_load_multipart_params(
    std::shared_ptr<_internal::multipart_subcontent> ms)
{
    for(auto ct : ms->contents){
        if(ct->type == _internal::multipart_content_type::subcontent){
            _load_multipart_params(
                std::static_pointer_cast<
                    _internal::multipart_subcontent
                >(ct)
            );
            
        }else if(ct->type == _internal::multipart_content_type::file){
            std::shared_ptr<_internal::multipart_content_file> mcf = 
                std::static_pointer_cast<
                    _internal::multipart_content_file
                >(ct);
            if(!_files)
                _files = std::make_shared<http_files>();
            _files->_files.emplace_back(
                std::make_shared<http_file>(
                    mcf->name, mcf->filename,
                    mcf->mime_type, mcf->temp_path,
                    mcf->size
                )
            );
        }else if(ct->type == _internal::multipart_content_type::form){
            std::shared_ptr<_internal::multipart_content_form> mcf = 
                std::static_pointer_cast<
                    _internal::multipart_content_form
                >(ct);
            _body_params->emplace_back(
                std::make_shared<http_param>(
                    mcf->name, mcf->value
                )
            );
        }
    }
}


evmvc::sp_app response::get_app() const
{
    return this->get_router()->get_app();
}
evmvc::sp_router response::get_router() const
{
    return this->get_route()->get_router();
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
        evmvc::html_version()
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

sp_master_server evmvc::listener::get_server() const { return _server.lock();}
sp_app evmvc::master_server::get_app() const { return _app.lock();}

void channel::_init_master_channels()
{
    usock_path = _worker->get_app()->abs_path(
        "~/.run/evmvc." + std::to_string(_worker->id())
    ).string();
    
    fcntl(ptoc[EVMVC_PIPE_WRITE_FD], F_SETFL, O_NONBLOCK);
    close(ptoc[EVMVC_PIPE_READ_FD]);
    ptoc[EVMVC_PIPE_READ_FD] = -1;
    fcntl(ctop[EVMVC_PIPE_READ_FD], F_SETFL, O_NONBLOCK);
    close(ctop[EVMVC_PIPE_WRITE_FD]);
    ctop[EVMVC_PIPE_WRITE_FD] = -1;
}

void channel::_init_child_channels()
{
    usock_path = _worker->get_app()->abs_path(
        "~/.run/evmvc." + std::to_string(_worker->id())
    ).string();
    
    if(remove(usock_path.c_str()) == -1){
        int err = errno;
        if(err != ENOENT)
            return _worker->log()->fatal(EVMVC_ERR(
                "unable to remove unix socket '{}', err: {}", usock_path, err
            ));
    }
    
    // create the client listener.
    int lfd = _internal::unix_bind(usock_path.c_str(), SOCK_STREAM);
    if(lfd == -1)
        return _worker->log()->fatal(EVMVC_ERR(
            "unix_bind '{}', err: {}", usock_path, errno
        ));
    
    if(listen(lfd, 5) == -1)
        return _worker->log()->fatal(EVMVC_ERR(
            "listen: {}", std::to_string(errno)
        ));
    
    do{
        // this will block until the master process connect.
        usock = accept(lfd, nullptr, nullptr);
        if(usock == -1)
            _worker->log()->warn(
                EVMVC_ERR("accept err: {}", errno)
            );
    }while(usock == -1);
    _internal::unix_set_sock_opts(usock);
    
    // close the listener
    close(lfd);
    
    fcntl(ptoc[EVMVC_PIPE_READ_FD], F_SETFL, O_NONBLOCK);
    close(ptoc[EVMVC_PIPE_WRITE_FD]);
    ptoc[EVMVC_PIPE_WRITE_FD] = -1;
    
    fcntl(ctop[EVMVC_PIPE_WRITE_FD], F_SETFL, O_NONBLOCK);
    close(ctop[EVMVC_PIPE_READ_FD]);
    ctop[EVMVC_PIPE_READ_FD] = -1;
}

namespace _internal{
    void _master_listen_cb(
        struct evconnlistener*, int sock,
        sockaddr* saddr, int socklen, void* args)
    {
        evmvc::listener* l = (evmvc::listener*)args;
        
        sp_master_server s = l->get_server();
        if(!s){
            close(sock);
            return;
        }
        sp_app a = s->get_app();
        if(!a){
            close(sock);
            return;
        }
        if(a->workers().empty()){
            close(sock);
            return;
        }
        
        // send the sock via cmsg
        int data;
        struct msghdr msgh;
        struct iovec iov;
        
        union
        {
            char buf[CMSG_SPACE(sizeof(int))];
            struct cmsghdr align;
        } ctrl_msg;
        struct cmsghdr* cmsgp;
        
        msgh.msg_name = NULL;
        msgh.msg_namelen = 0;
        msgh.msg_iov = &iov;
        
        msgh.msg_iovlen = 1;
        iov.iov_base = &data;
        iov.iov_len = sizeof(int);
        data = 12345;
        
        msgh.msg_control = ctrl_msg.buf;
        msgh.msg_controllen = sizeof(ctrl_msg.buf);
        
        memset(ctrl_msg.buf, 0, sizeof(ctrl_msg.buf));
        
        cmsgp = CMSG_FIRSTHDR(&msgh);
        cmsgp->cmsg_len = CMSG_LEN(sizeof(int));
        cmsgp->cmsg_level = SOL_SOCKET;
        cmsgp->cmsg_type = SCM_RIGHTS;
        *((int*)CMSG_DATA(cmsgp)) = sock;
        
        // select prefered worker;
        sp_worker pw;
        for(auto& w : a->workers())
            if(w->type() == worker_type::http){
                if(!pw){
                    pw = w;
                    continue;
                }
                
                if(w->workload() < pw->workload())
                    pw = w;
            }
            
        if(!pw)
            return a->log()->fail(EVMVC_ERR(
                "Unable to find an available http_worker!"
            ));
        
        // if(++(*wid) >= workers().size())
        //     *wid = 0;
        // int res = sendmsg(workers()[*wid]->chan.usock, &msgh, 0);
        // if(res == -1)
        //     std::exit(-1);
        
        int res = pw->channel().sendmsg(&msgh, 0);
        if(res == -1)
            return a->log()->fatal(EVMVC_ERR(
                "sendmsg to http_worker failed: {}", errno
            ));
    }
    
    
    
    
    inline evmvc::sp_response create_http_response(
        wp_app a,
        evhtp_request_t* ev_req,
        const std::vector<std::shared_ptr<evmvc::http_param>>& params =
            std::vector<std::shared_ptr<evmvc::http_param>>()
        )
    {
        return _internal::create_http_response(
            a.lock()->log(), 
            ev_req,
            evmvc::route::null(a),
            params
        );
    }
    
    inline evmvc::sp_response create_http_response(
        sp_logger log,
        evhtp_request_t* ev_req,
        sp_route rt,
        const std::vector<std::shared_ptr<evmvc::http_param>>& params =
            std::vector<std::shared_ptr<evmvc::http_param>>()
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
