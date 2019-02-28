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
//#include "multipart_parser.h"

#include "worker.h"
#include "master_server.h"

namespace evmvc {

//https://chromium.googlesource.com/external/github.com/ellzey/libevhtp/+/libevhtp2/src/evhtp2/ws/

class app
    : public std::enable_shared_from_this<app>
{
    // friend evhtp_res _internal::on_headers(
    //     evhtp_request_t* req, evhtp_headers_t* hdr, void* arg);
    // friend void _internal::on_app_request(evhtp_request_t* req, void* arg);
    friend class http_parser;
    
public:
    
    app(struct event_base* ev_base,
        evmvc::app_options&& opts)
        : _init_rtr(false), _init_dirs(false), _status(running_state::stopped),
        _options(std::move(opts)),
        _router()//,
        //_evhtp(nullptr)//, _ar(nullptr)
    {
        // force get_field_table initialization
        evmvc::detail::get_field_table();
        
        evmvc::global::set_event_base(ev_base);
        
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
                evmvc::global::ev_base(),
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
        
        // force console buffer flush
        std::cout << std::endl;
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
    
    int start(int argc, char** argv, bool start_event_loop = true)
    {
        if(!stopped())
            throw EVMVC_ERR(
                "app must be in stopped state to start listening again"
            );
        _status = running_state::starting;
        
        _init_directories();
        
        std::vector<sp_http_worker> twks;
        for(size_t i = 0; i < _options.worker_count; ++i){
            sp_http_worker w = std::make_shared<evmvc::http_worker>(
                this->shared_from_this(), _options, this->_log
            );
            
            int pid = fork();
            if(pid == -1){// fork failed
                _log->fatal(
                    "Unable to create new worker process!"
                );
                return -1;
            }else if(pid == 0){// in child process
                w->start(argc, argv, pid);
                return 1;
            }else{// in master process
                w->start(argc, argv, pid);
                twks.emplace_back(w);
            }
        }
        
        // sleep to let time for children to open the unix socket.
        usleep(30000);
        for(auto w : twks){
            _workers.emplace_back(w);
            int tryout = 0;
            do{
                w->channel().usock = _internal::unix_connect(
                    w->channel().usock_path.c_str(), SOCK_STREAM
                );
                if(w->channel().usock == -1){
                    _log->info(EVMVC_ERR(
                        "unix connect err: {}", errno
                    ));
                }else
                    break;
                
                if(++tryout >= 5){
                    _log->fatal(EVMVC_ERR(
                        "unable to connect to client: {}",
                        w->id()
                    ));
                    return -1;
                }
            }while(w->channel().usock == -1);
            _internal::unix_set_sock_opts(w->channel().usock);
        }
        
        for(auto sc : _options.servers){
            auto s = std::make_shared<master_server>(
                this->shared_from_this(), sc, _log
            );
            s->start();
            _servers.emplace_back(s);
        }
        
        if(start_event_loop){
            event_base_loop(global::ev_base(), 0);
            if(running())
                stop();
        }
        
        return 0;
    }
    
    void stop(bool free_ev_base = false)
    {
        if(stopped() || stopping())
            throw EVMVC_ERR(
                "app must be in running state to be able to stop it."
            );
        this->_status = running_state::stopping;
        
        std::clog << "Stopping service" << std::endl;
        _log->info("Stopping service");
        
        for(auto w : _workers)
            if(w->running())
                w->stop();
        
        struct timeval tvn;
        gettimeofday(&tvn, nullptr);
        auto to = ms_to_timeval(3000 + timeval_to_ms(tvn));
        
        while(true){
            event_base_loop(global::ev_base(), EVLOOP_ONCE);
            bool stopped = true;
            for(auto w : _workers)
                if(!w->stopped())
                    stopped = false;
            if(stopped)
                break;
            
            gettimeofday(&tvn, nullptr);
            if(timercmp(&tvn, &to, >))
                break;
        }
        
        for(auto w : _workers)
            if(!w->stopped())
                kill(w->pid(), SIGKILL);
        
        _workers.clear();
        _servers.clear();
        
        if(free_ev_base)
            event_base_free(global::ev_base());
        
        std::clog << "Service stopped" << std::endl;
        _log->info("Service stopped");
        
        this->_status = running_state::stopped;
    }
    
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
        if(bfs::create_directories(_options.run_dir))
            this->_log->info(
                "Creating run directory '{}'\n", _options.run_dir.c_str()
            );
        
        this->_init_router();
    }
    
    bool _init_rtr;
    bool _init_dirs;
    running_state _status;
    app_options _options;
    sp_router _router;
    //struct evhtp* _evhtp;
    evmvc::sp_logger _log;
    
    std::vector<evmvc::sp_worker> _workers;
    std::vector<evmvc::sp_master_server> _servers;
};//evmvc::app


}// ns evmvc

#include "worker_impl.h"
#include "master_server_impl.h"
#include "connection_impl.h"
#include "parser_impl.h"
#include "request_impl.h"
#include "response_impl.h"
#include "router_impl.h"
#include "_internal_impl.h"

#endif //_libevmvc_app_h
