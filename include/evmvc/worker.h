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

#ifndef _libevmvc_worker_h
#define _libevmvc_worker_h

#include "stable_headers.h"
#include "logging.h"
#include "configuration.h"
#include "unix_socket_utils.h"
#include "child_server.h"
#include "connection.h"

#define EVMVC_PIPE_WRITE_FD 1
#define EVMVC_PIPE_READ_FD 0

namespace evmvc {


enum class channel_type
{
    unknown,
    master,
    child
};

class channel
{
    friend class worker;
    
public:
    channel(evmvc::worker* worker)
        : _worker(worker), _type(channel_type::unknown)
    {
        pipe(ptoc);
        pipe(ctop);
    }
    
    ~channel()
    {
        close_channels();
    }
    
    evmvc::worker* worker() const { return _worker;}
    
    void close_channels()
    {
        if(_type == channel_type::master){
            _close_master_channels();
        }else if(_type == channel_type::child){
            _close_child_channels();
        }
    }
    
    ssize_t sendmsg(const msghdr *__message, int __flags)
    {
        if(_type == channel_type::master)
            return ::sendmsg(usock, __message, __flags);
        throw EVMVC_ERR("sendmsg can only be called in master process!");
    }
    
    
    
private:
    void _init();
    void _init_master_channels();
    void _init_child_channels();
    
    void _close_master_channels()
    {
        if(usock > -1 && remove(usock_path.c_str()) == -1){
            int err = errno;
            if(err != ENOENT)
                evmvc::_internal::default_logger()->error(
                    EVMVC_ERR(
                        "unable to remove unix socket, err: {}, path: '{}'",
                        err, usock_path
                    )
                );
            usock = -1;
        }
        
        if(rcmd_ev){
            event_del(rcmd_ev);
            event_free(rcmd_ev);
            rcmd_ev = nullptr;
        }
        
        if(ptoc[EVMVC_PIPE_READ_FD] > -1){
            close(ptoc[EVMVC_PIPE_READ_FD]);
            ptoc[EVMVC_PIPE_READ_FD] = -1;
        }
        if(ctop[EVMVC_PIPE_WRITE_FD] > -1){
            close(ctop[EVMVC_PIPE_WRITE_FD]);
            ctop[EVMVC_PIPE_WRITE_FD] = -1;
        }
    }
    
    void _close_child_channels()
    {
        if(rcmsg_ev){
            event_del(rcmsg_ev);
            event_free(rcmsg_ev);
            rcmsg_ev = nullptr;
        }
        
        if(usock > -1 && remove(usock_path.c_str()) == -1){
            int err = errno;
            if(err != ENOENT)
                evmvc::_internal::default_logger()->error(
                    EVMVC_ERR(
                        "unable to remove unix socket, err: {}, path: '{}'",
                        err, usock_path
                    )
                );
            usock = -1;
        }
        
        if(rcmd_ev){
            event_del(rcmd_ev);
            event_free(rcmd_ev);
            rcmd_ev = nullptr;
        }
        
        if(ptoc[EVMVC_PIPE_READ_FD] > -1){
            close(ptoc[EVMVC_PIPE_READ_FD]);
            ptoc[EVMVC_PIPE_READ_FD] = -1;
        }
        if(ctop[EVMVC_PIPE_WRITE_FD] > -1){
            close(ctop[EVMVC_PIPE_WRITE_FD]);
            ctop[EVMVC_PIPE_WRITE_FD] = -1;
        }
    }
    
    evmvc::worker* _worker;
    channel_type _type = channel_type::unknown;

public:
    int ptoc[2] = {-1,-1};
    int ctop[2] = {-1,-1};
    struct event* rcmd_ev = nullptr;
    // access control messages (ancillary data) socket
    int usock = -1;
    std::string usock_path = "";
    struct event* rcmsg_ev = nullptr;
};

enum class process_type
{
    unknown = 0,
    master,
    child,
};

enum class worker_type
{
    http = 0,
    cache = 1,
};

class worker
{
    static int nxt_id()
    {
        static int _id = -1;
        return ++_id;
    }
    
public:
    worker(const wp_app& app, const app_options& config)
        : _id(nxt_id()), _pid(-1), _ptype(process_type::unknown),
        _app(app),
        _log(app.lock()->log()->add_child("worker " + std::to_string(_id))),
        _config(config),
        _channel(std::make_unique<evmvc::channel>(this))
    {
    }
    
    ~worker()
    {
        if(running())
            stop();
    }
    
    virtual worker_type type() const = 0;
    virtual int workload() const = 0;
    
    running_state status() const
    {
        return _status;
    }
    bool stopped() const { return _status == running_state::stopped;}
    bool starting() const { return _status == running_state::starting;}
    bool running() const { return _status == running_state::running;}
    bool stopping() const { return _status == running_state::stopping;}
    
    sp_logger log() const { return _log;}
    
    int id() const { return _id;}
    int pid() { return _pid;}
    process_type proc_type() const { return _ptype;}
    
    sp_app get_app() const { return _app.lock();}
    evmvc::channel& channel() const { return *(_channel.get());}
    
    virtual void start(int pid)
    {
        if(!stopped())
            throw EVMVC_ERR(
                "Worker must be in stopped state to start listening again"
            );
        _status = running_state::starting;
        
        if(pid > 0){
            _ptype = process_type::master;
            _pid = pid;
            
        }else if(pid == 0){
            _ptype = process_type::child;
            _pid = getpid();
        }
        
        _channel->_init();
        _status = running_state::running;
    }
    
    void stop()
    {
        if(stopped() || stopping())
            throw EVMVC_ERR(
                "Server must be in running state to be able to stop it."
            );
        this->_status = running_state::stopping;
        
        _channel.release();
        
        this->_status = running_state::stopped;
    }
    
protected:
    running_state _status = running_state::stopped;
    int _id;
    int _pid;
    process_type _ptype;
    wp_app _app;
    sp_logger _log;
    app_options _config;
    
    std::unique_ptr<evmvc::channel> _channel;
};

void channel::_init()
{
    channel_type ct = 
        _worker->proc_type() == process_type::master ?
            channel_type::master :
        _worker->proc_type() == process_type::child ?
            channel_type::child :
        channel_type::unknown;
    
    if(ct == channel_type::unknown)
        throw EVMVC_ERR("invalid channel_type 'unknown'!");
    
    _type = ct;
    switch(ct){
        case channel_type::master:
            _init_master_channels();
            break;
        case channel_type::child:
            _init_child_channels();
            break;
    }
}


class http_worker
    : public std::enable_shared_from_this<http_worker>,
    public worker
{
public:
    http_worker(const wp_app& app, const app_options& config)
        : worker(app, config)
    {
    }
    
    worker_type type() const { return worker_type::http;}
    
    int workload() const
    {
        
    }
    
    sp_child_server find_server_by_name(evmvc::string_view name)
    {
        for(auto& s : _servers)
            if(!strcasecmp(s.second->name().c_str(), name.data()))
                return s.second;
        return nullptr;
    }
    sp_child_server find_server_by_alias(evmvc::string_view name)
    {
        for(auto& s : _servers){
            for(auto& alias : s.second->aliases()){
                if(!strcasecmp(alias.c_str(), name.data()))
                    return s.second;
            }
        }
        return nullptr;
    }
    
private:
    
    
    std::unordered_map<int, sp_connection> _conns;
    std::unordered_map<int, sp_child_server> _servers;
};

class cache_worker
    : public std::enable_shared_from_this<cache_worker>,
    public worker
{
public:
    cache_worker(const wp_app& app, const app_options& config)
        : worker(app, config)
    {
    }
    
    worker_type type() const { return worker_type::cache;}
    
    
private:
    void _init_ssl()
    {
        
    }
};


sp_http_worker connection::get_worker() const
{
    return _worker.lock();
}

namespace _internal {
    
    int _ssl_sni_servername(SSL* s, int *al, void *arg)
    {
        const char* name = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);
        if(!name)
            return SSL_TLSEXT_ERR_NOACK;
        
        evmvc::connection* c = (evmvc::connection*)SSL_get_app_data(s);
        if(!c)
            return SSL_TLSEXT_ERR_NOACK;
        
        auto w = c->get_worker();
        if(!w)
            return SSL_TLSEXT_ERR_NOACK;
        
        // verify if the current ctx is valid
        SSL_CTX* ctx = SSL_get_SSL_CTX(s);
        child_server* cs = (child_server*)SSL_CTX_get_app_data(ctx);
        if(!strcasecmp(cs->name().c_str(), name))
            return SSL_TLSEXT_ERR_OK;
        
        // find the requested server
        sp_child_server rs = w->find_server_by_name(name);
        if(!rs){
            rs = w->find_server_by_alias(name);
            if(!rs)
                return SSL_TLSEXT_ERR_NOACK;
        }
        
        c->set_conn_flag(conn_flags::server_via_sni);
        c->_server = rs;
        
        SSL_set_SSL_CTX(s, rs->ssl_ctx());
        SSL_set_options(s, SSL_CTX_get_options(ctx));
        
        if(SSL_get_verify_mode(s) == SSL_VERIFY_NONE ||
            SSL_num_renegotiations(s) == 0
        ){
            SSL_set_verify(
                s, 
                SSL_CTX_get_verify_mode(ctx),
                SSL_CTX_get_verify_callback(ctx)
            );
        }
        
        return SSL_TLSEXT_ERR_OK;
    }
    // int _ssl_client_hello(SSL* s, int *al, void *arg)
    // {
    // }

    int _ssl_new_cache_entry(SSL* /*ssl*/, SSL_SESSION* /*sess*/)
    {
        //TODO: implement shared mem
        return 0;
    }
    
    SSL_SESSION* _ssl_get_cache_entry(
        SSL* /*ssl*/, const unsigned char* /*sid*/,
        int /*sid_len*/, int* /*copy*/)
    {
        //TODO: implement shared mem
        return nullptr;
    }
    
    void _ssl_remove_cache_entry(SSL_CTX* /*ctx*/, SSL_SESSION* /*sess*/)
    {
        //TODO: implement shared mem
        return;
    }
    
    
}//::_internal

};//::evmvc
#endif//_libevmvc_worker_h
