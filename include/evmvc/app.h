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

#include "worker.h"
#include "master_server.h"

#include <sys/wait.h>

namespace evmvc {

class app
    : public std::enable_shared_from_this<app>
{
    friend class http_parser;
    
public:
    
    app(struct event_base* ev_base,
        evmvc::app_options&& opts)
        : _init_rtr(false), _init_dirs(false), _status(running_state::stopped),
        _options(std::move(opts)),
        _router(),
        _ev_verif_childs(nullptr),
        _app_data(std::make_shared<evmvc::response_data_map_t>())
    {
        // force get_field_table initialization
        evmvc::detail::get_field_table();

        // set event_base
        evmvc::global::set_event_base(ev_base);
        
        // init the default logger
        if(_options.use_default_logger){
            std::vector<md::log::sinks::sp_logger_sink> sinks;
            
            auto out_sink = std::make_shared<md::log::sinks::console_sink>(
                _options.log_console_enable_color
            );
            out_sink->set_level(_options.log_console_level);
            sinks.emplace_back(out_sink);
            
            bfs::create_directories(_options.log_dir);
            auto file_sink = 
                std::make_shared<md::log::sinks::rotating_file_sink>(
                    evmvc::global::ev_base(),
                    (_options.log_dir / "libevmvc.log").c_str(),
                    _options.log_file_max_size,
                    _options.log_file_max_files
                );
            file_sink->set_level(_options.log_file_level);
            sinks.emplace_back(file_sink);
            
            _log = std::make_shared<md::log::logger>(
                "/", sinks.begin(), sinks.end()
            );
            _log->set_level(
                std::max(
                    _options.log_file_level, _options.log_console_level
                )
            );
            md::log::default_logger() = _log;
        }else
            _log = md::log::default_logger()->add_child(
                "/"
            );
    }
    
    ~app()
    {
        if(_ev_verif_childs)
            event_free(_ev_verif_childs);
        _ev_verif_childs = nullptr;
        
        evmvc::clear_events();
        _router.reset();
        
        if(this->running())
            this->stop();
        
        // force console buffer flush
        std::cout << std::endl;
    }
    
    app_options& options(){ return _options;}
    
    void replace_logger(md::log::sp_logger l)
    {
        _log = l;
    }
    md::log::sp_logger log()
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
    
    /*
    * convert a relative path to absolute representation
    * if path starts with '~/' then it's append to base_dir
    * if path starts with '$/' then it's append to cache_dir
    * if path starts with '!/' then it's append to temp_dir
    * if path starts with '@/' then it's append to view_dir
    */
    bfs::path abs_path(md::string_view rel_path)
    {
        std::string rp(rel_path.data(), rel_path.size());
        if(boost::algorithm::starts_with(rp, "~/"))
            return _options.base_dir / rp.substr(2);
        else if(boost::algorithm::starts_with(rp, "$/"))
            return _options.cache_dir / rp.substr(2);
        else if(boost::algorithm::starts_with(rp, "!/"))
            return _options.temp_dir / rp.substr(2);
        else if(boost::algorithm::starts_with(rp, "@/"))
            return _options.view_dir / rp.substr(2);
        else if(rp[0] == '/')
            return rp;
        else
            return _options.base_dir / rp;
    }
    
    void set_callbacks(
        md::callback::value_cb<evmvc::process_type> started_cb, 
        md::callback::value_cb<evmvc::process_type> stopped_cb)
    {
        _started_cb = started_cb;
        _stopped_cb = stopped_cb;
    }
    
    void initialize()
    {
        _init_directories();
    }
    
    int start(int argc, char** argv, bool start_event_loop = false)
    {
        _argc = argc;
        _argv = argv;
        
        if(!stopped())
            throw MD_ERR(
                "app must be in stopped state to start listening again"
            );
        _status = running_state::starting;
        
        this->initialize();
        
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
                w->set_callbacks(_started_cb, _stopped_cb);
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
                    _log->info(MD_ERR(
                        "unix connect err: {}", errno
                    ));
                }else
                    break;
                
                if(++tryout >= 5){
                    _log->fatal(MD_ERR(
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
        
        _status = running_state::running;
        
        // enable child respawn on release build
        // enable master shutdown on debug build
        // TODO: only respawn a limited number of time in a specified period.
        _ev_verif_childs = event_new(
            global::ev_base(),
            -1, EV_READ | EV_PERSIST,
            app::_verify_child_processes,
            this
        );
        timeval tv = md::date::ms_to_timeval(1000);
        event_add(_ev_verif_childs, &tv);
        
        if(_started_cb)
            set_timeout([self = this->shared_from_this()](auto ew){
                self->_started_cb(nullptr, process_type::master);
            }, 0);
        
        if(start_event_loop){
            event_base_loop(global::ev_base(), 0);
            if(running())
                stop();
        }
        
        return 0;
    }
    
    void stop(md::callback::async_cb cb = nullptr, bool free_ev_base = false)
    {
        if(auto w = worker::active_worker()){
            if(w->is_child()){
                w->close_service();
                if(cb)
                    cb(nullptr);
                return;
            }
        }
        
        if(stopped() || stopping())
            throw MD_ERR(
                "app must be in running state to be able to stop it."
            );
        this->_status = running_state::stopping;
        
        std::clog << "Stopping service" << std::endl;
        _log->info("Stopping service");
        
        for(auto w : _workers)
            if(w->running())
                w->stop();
        
        this->_workers.clear();
        this->_servers.clear();
        this->_router.reset();
        
        if(free_ev_base)
            event_base_free(global::ev_base());
        
        std::clog << "Service stopped" << std::endl;
        this->_log->info("Service stopped");
        
        this->_status = running_state::stopped;
        
        if(_stopped_cb)
            _stopped_cb(nullptr, process_type::master);
        
        if(cb)
            cb(nullptr);
        
        // struct timeval tvn;
        // gettimeofday(&tvn, nullptr);
        // auto to = ms_to_timeval(3000 + timeval_to_ms(tvn));
        // set_interval(
        // [self = this->shared_from_this(), cb, free_ev_base, to]
        // (auto ev){
        //     bool stopped = true;
        //     for(auto w : self->_workers)
        //         if(!w->stopped())
        //             stopped = false;
        //    
        //     struct timeval tvn;
        //     gettimeofday(&tvn, nullptr);
        //     if(!stopped && timercmp(&tvn, &to, <))
        //         return;
        //    
        //     clear_timeout(ev);
        //    
        //     for(auto w : self->_workers)
        //         if(!w->stopped())
        //             kill(w->pid(), SIGKILL);
        //    
        //     self->_workers.clear();
        //     self->_servers.clear();
        //    
        //     if(free_ev_base)
        //         event_base_free(global::ev_base());
        //    
        //     std::clog << "Service stopped" << std::endl;
        //     self->_log->info("Service stopped");
        //    
        //     self->_status = running_state::stopped;
        //    
        //     if(cb)
        //         cb(nullptr);
        // }, 50);
    }
    
    // ==============
    // == app data ==
    // ==============
    
    evmvc::response_data_map app_data() const
    {
        return _app_data;
    }
    
    void clear_data(md::string_view name)
    {
        auto it = _app_data->find(name.to_string());
        if(it == _app_data->end())
            return;
        _app_data->erase(it);
    }
    
    template<typename T>
    void set_data(md::string_view name, T data)
    {
        auto it = _app_data->find(name.to_string());
        if(it == _app_data->end()){
            _app_data->emplace(
                std::make_pair(
                    name.to_string(),
                    response_data<T>(
                        new response_data_t<T>(data)
                    )
                )
            );
        }else{
            it->second = response_data<T>(
                new response_data_t<T>(data)
            );
        }
    }
    
    template<typename T>
    T get_data(md::string_view name, const T& def_val)
    {
        auto it = _app_data->find(name.to_string());
        if(it == _app_data->end())
            return def_val;
        return std::static_pointer_cast<response_data_t<T>>(
            it->second
        )->value();
    }
    
    template<typename T>
    T get_data(md::string_view name)
    {
        auto it = _app_data->find(name.to_string());
        if(it == _app_data->end())
            throw MD_ERR("Data '{}' not found!", name);
        return std::static_pointer_cast<response_data_t<T>>(
            it->second
        )->value();
    }
    
    // ====================
    // == default router ==
    // ====================
    
    sp_router find_router(md::string_view route)
    {
        if(!_init_rtr)
            throw MD_ERR(
                "evmvc::app must be initialized by calling "
                "initialize() method first!"
            );
        return _router->find_router(route);
    }
    
    sp_router use(use_handler_when w, route_handler_cb cb)
    {
        this->_init_router();
        return _router->use(w, cb);
    }
    
    sp_router all(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->all(route_path, cb, pol);
    }
    sp_router options(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->options(route_path, cb, pol);
    }
    sp_router del(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->del(route_path, cb, pol);
    }
    sp_router head(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->head(route_path, cb, pol);
    }
    sp_router get(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->get(route_path, cb, pol);
    }
    sp_router post(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->post(route_path, cb, pol);
    }
    sp_router put(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->put(route_path, cb, pol);
    }
    sp_router connect(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->connect(route_path, cb, pol);
    }
    sp_router trace(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->trace(route_path, cb, pol);
    }
    sp_router patch(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        this->_init_router();
        return _router->patch(route_path, cb, pol);
    }
    
    sp_router register_route_handler(
        const md::string_view& verb,
        const md::string_view& route_path,
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
        const md::string_view& route_path, policies::filter_policy pol)
    {
        return _router->register_policy(route_path, pol);
    }
    
    sp_router register_policy(
        evmvc::method method,
        const md::string_view& route_path,
        policies::filter_policy pol)
    {
        return _router->register_policy(method, route_path, pol);
    }
    
    sp_router register_policy(
        const md::string_view& method,
        const md::string_view& route_path,
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
    
    static void _verify_child_processes(int /*fd*/, short /*events*/, void* arg)
    {
        app* self = (app*)arg;
        
        if(self->_status != running_state::running)
            return;
        
        int wstatus = 0;
        pid_t p = waitpid(-1, &wstatus, WNOHANG | WUNTRACED);
        
        if(p <= 0)
            return;
        
        if(WIFSIGNALED(wstatus)){
            int sig = WTERMSIG(wstatus);
            if(sig == SIGINT ||
                sig == SIGKILL
            ){
                return;
            }
        }
        
        #if EVMVC_BUILD_DEBUG
            self->_log->error(MD_ERR(
                "Worker process crashed!"
            ));
            raise(SIGINT);
            return;
        #endif
        
        // retreive worker instance
        for(auto it = self->_workers.begin(); it != self->_workers.end(); ++it){
            if((*it)->pid() == p){
                //evmvc::sp_worker w = *it;
                (*it)->stop(true);
                self->_workers.erase(it);
                
                evmvc::sp_worker w = std::make_shared<evmvc::http_worker>(
                    self->shared_from_this(), self->_options, self->_log
                );
                
                // restart child proc
                int pid = fork();
                if(pid == -1){// fork failed
                    self->_log->fatal(MD_ERR(
                        "Failed to create new worker process!"
                    ));
                    return;
                }else if(pid == 0){// in child process
                    w->set_callbacks(self->_started_cb, self->_stopped_cb);
                    w->start(self->_argc, self->_argv, pid);
                    return;
                }else{// in master process
                    w->start(self->_argc, self->_argv, pid);
                }
                
                // sleep to let time for children to open the unix socket.
                usleep(30000);
                int tryout = 0;
                do{
                    w->channel().usock = _internal::unix_connect(
                        w->channel().usock_path.c_str(), SOCK_STREAM
                    );
                    if(w->channel().usock == -1){
                        self->_log->info(MD_ERR(
                            "unix connect err: {}", errno
                        ));
                    }else
                        break;
                    
                    if(++tryout >= 5){
                        self->_log->fatal(MD_ERR(
                            "unable to connect to client: {}",
                            w->id()
                        ));
                        return;
                    }
                }while(w->channel().usock == -1);
                _internal::unix_set_sock_opts(w->channel().usock);
                
                self->_workers.emplace_back(w);
                
                return;
            }
        }
        
        self->_log->warn(MD_ERR(
            "Unknown child has died, PID: '{}'", p
        ));
    }
    
    inline void _init_router()
    {
        if(_init_rtr)
            return;
        _init_rtr = true;
        
        _router =
            std::make_shared<router>(
                this->shared_from_this(), "/"
            );
        //_router->_path = "";
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
    md::log::sp_logger _log;
    
    std::vector<evmvc::sp_worker> _workers;
    std::vector<evmvc::sp_master_server> _servers;
    
    md::callback::value_cb<evmvc::process_type> _started_cb; 
    md::callback::value_cb<evmvc::process_type> _stopped_cb;
    
    event* _ev_verif_childs;
    evmvc::response_data_map _app_data;

    int _argc;
    char** _argv;
    
    
};//evmvc::app


}// ns evmvc

#include "worker_impl.h"
#include "master_server_impl.h"
#include "child_server_impl.h"
#include "connection_impl.h"
#include "parser_impl.h"
#include "request_impl.h"
#include "response_impl.h"
#include "router_impl.h"
#include "_internal_impl.h"

#endif //_libevmvc_app_h
