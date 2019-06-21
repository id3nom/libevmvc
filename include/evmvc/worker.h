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
#include "configuration.h"
#include "unix_socket_utils.h"
#include "child_server.h"
#include "connection.h"
#include "cmd.h"

#include <sys/prctl.h>

#define EVMVC_PIPE_WRITE_FD 1
#define EVMVC_PIPE_READ_FD 0

#define EVMVC_MAX_SSL_DATA_LEN 1024

namespace evmvc {

enum class channel_type
{
    unknown,
    master,
    child
};

typedef struct cmd_log_t
{
    const char* label;
    size_t label_size;
    const char* msg;
    size_t msg_size;
} cmd_log;

class channel
{
    friend class worker_t;
    
public:
    channel(evmvc::worker_t* worker_t)
        : _worker(worker_t), _type(channel_type::unknown)
    {
        pipe(ptoc);
        pipe(ctop);
    }
    
    ~channel()
    {
        close_channels();
    }
    
    evmvc::worker_t* worker_t() const { return _worker;}
    
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
        throw MD_ERR("sendmsg can only be called in master process!");
    }
    
    ssize_t sendcmd(const command& cmd)
    {
        return _sendcmd(cmd.id(), cmd.data(), cmd.size());
    }
    ssize_t sendcmd(shared_command cmd)
    {
        return _sendcmd(cmd->id(), cmd->data(), cmd->size());
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
                md::log::default_logger()->error(
                    MD_ERR(
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
        
        if(rcmd_buf){
            evbuffer_free(rcmd_buf);
            rcmd_buf = nullptr;
        }
        
        if(ptoc[EVMVC_PIPE_WRITE_FD] > -1){
            close(ptoc[EVMVC_PIPE_WRITE_FD]);
            ptoc[EVMVC_PIPE_WRITE_FD] = -1;
        }
        if(ctop[EVMVC_PIPE_READ_FD] > -1){
            close(ctop[EVMVC_PIPE_READ_FD]);
            ctop[EVMVC_PIPE_READ_FD] = -1;
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
                md::log::default_logger()->error(
                    MD_ERR(
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
        
        if(rcmd_buf){
            evbuffer_free(rcmd_buf);
            rcmd_buf = nullptr;
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
    
    ssize_t _sendcmd(int cmd_id, const char* payload, size_t payload_len);
    
    
    evmvc::worker_t* _worker;
    channel_type _type = channel_type::unknown;

public:
    int ptoc[2] = {-1,-1};
    int ctop[2] = {-1,-1};
    struct event* rcmd_ev = nullptr;
    struct evbuffer* rcmd_buf = nullptr;
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
inline md::string_view to_string(worker_type t)
{
    switch(t){
        case worker_type::http:     return "http";
        case worker_type::cache:    return "cache";
        default:
            throw MD_ERR("UNKNOWN worker_type: '{}'", (int)t);
    }
}

namespace sinks{
class child_sink
    : public md::log::sinks::logger_sink_t
{
public:
    child_sink(std::weak_ptr<worker_t> w)
        : md::log::sinks::logger_sink_t(md::log::log_level::info), _w(w)
    {
    }
    
    void log(
        md::string_view log_path,
        md::log::log_level lvl, md::string_view msg
    ) const;
private:
    std::weak_ptr<worker_t> _w;
};
}//::sinks

void channel_cmd_read(int fd, short events, void* arg);

typedef std::function<bool(shared_command cmd)>
    cmd_parser_fn;

worker active_worker(worker w = nullptr);
class worker_t
    : public std::enable_shared_from_this<worker_t>
{
    friend void channel_cmd_read(int fd, short events, void* arg);
    static int nxt_id()
    {
        static int _id = -1;
        return ++_id;
    }

protected:
    static void sig_received(int sig);
    
    static void _on_event_log(int severity, const char *msg)
    {
        if(severity == _EVENT_LOG_DEBUG)
            md::log::default_logger()->debug(msg);
        else
            md::log::default_logger()->error(msg);
    }
    
    static void _on_event_fatal_error(int err)
    {
        md::log::default_logger()->fatal(
            "Exiting because libevent send a fatal error: '{}'",
            err
        );
    }
    
    worker_t(const wp_app& app_t, const app_options& config,
        worker_type wtype, const md::log::logger& log)
        : _app(app_t),
        _config(config),
        _wtype(wtype),
        _id(nxt_id()),
        _log(
            log->add_child(fmt::format(
                "worker-{}-{}", to_string(wtype), _id
            ))
        ),
        _pid(-1),
        _ptype(process_type::unknown),
        _channel(std::make_unique<evmvc::channel>(this))
    {
    }
    

public:

    ~worker_t()
    {
        if(running())
            this->stop();
        _channel.release();
    }
    
    worker_type work_type() const { return _wtype;}
    virtual int workload() const = 0;
    
    running_state status() const
    {
        return _status;
    }
    bool stopped() const { return _status == running_state::stopped;}
    bool starting() const { return _status == running_state::starting;}
    bool running() const { return _status == running_state::running;}
    bool stopping() const { return _status == running_state::stopping;}
    
    md::log::logger log() const { return _log;}
    
    int id() const { return _id;}
    int pid() { return _pid;}
    process_type proc_type() const { return _ptype;}
    
    app get_app() const { return _app.lock();}
    bool is_valid() const { return (bool)_channel;}
    evmvc::channel& channel() const { return *(_channel.get());}
    
    bool is_child() const { return _ptype == process_type::child;}
    
    void set_callbacks(
        md::callback::async_item_cb<evmvc::process_type> started_cb, 
        md::callback::async_item_cb<evmvc::process_type> stopped_cb)
    {
        _started_cb = started_cb;
        _stopped_cb = stopped_cb;
    }
    
    virtual void start(int argc, char** argv, int pid)
    {
        if(!stopped())
            throw MD_ERR(
                "Worker must be in stopped state to start listening again"
            );
        _status = running_state::starting;
        
        if(pid > 0){
            _ptype = process_type::master;
            _pid = pid;
            
        }else if(pid == 0){
            struct sigaction sigint_sa;
            sigint_sa.sa_handler = worker_t::sig_received;
            sigaction(SIGINT, &sigint_sa, nullptr);
            
            sigint_sa.sa_handler = SIG_IGN;
            sigaction(SIGPIPE, &sigint_sa, nullptr);
            
            _ptype = process_type::child;
            _pid = getpid();
            
            evmvc::active_worker(this->shared_from_this());
            
            _cmd_parsers.clear();
            
            // change proctitle
            char ppname[17]{0};
            prctl(PR_GET_NAME, ppname);
            
            std::string pname = 
                fmt::format(
                    "evmvc-worker"
                );
            prctl(PR_SET_NAME, pname.c_str());
            
            pname += ":";
            pname += ppname;
            
            size_t pname_size = strlen(argv[0]);
            strncpy(argv[0], pname.c_str(), pname_size);
            
            auto self = this->shared_from_this();
            auto c_sink = std::make_shared<evmvc::sinks::child_sink>(self);
            c_sink->set_level(_log->level());
            _log->replace_sink(c_sink);
            
            // will send a SIGINT when the parent dies
            prctl(PR_SET_PDEATHSIG, SIGINT);
            
            event_set_log_callback(worker_t::_on_event_log);
            event_set_fatal_callback(worker_t::_on_event_fatal_error);
            global::ev_base(event_base_new(), false, true);
            
            if(_started_cb)
                set_timeout([self](auto ew){
                    self->_started_cb(process_type::child,
                    [self](const md::callback::cb_error& err){
                        if(err){
                            self->log()->error(err);
                            self->stop();
                            return;
                        }
                    });
                }, 0);
        }
        
        _channel->_init();
        _status = running_state::running;
    }
    
    void stop(bool force = false)
    {
        if(stopped() || stopping())
            throw MD_ERR(
                "Worker must be in running state to be able to stop it."
            );
        this->_status = running_state::stopping;
        
        if(this->is_child()){
            _log->info(
                "Closing worker: {}, force: {}",
                _id, force ? "true" : "false"
            );
            // closing worker on the master process
            //_channel->sendcmd(EVMVC_CMD_CLOSE, nullptr, 0);
            _channel.release();
            
            auto stop_evbase_loop = [force]() -> void{
                if(force)
                    event_base_loopbreak(global::ev_base());
                else
                    event_base_loopexit(global::ev_base(), nullptr);
                exit(0);
            };
            
            if(_stopped_cb)
                _stopped_cb(process_type::child,
                [stop_evbase_loop](const md::callback::cb_error& err)->void{
                    if(err){
                        std::cerr << fmt::format(
                            "{}",
                            err.c_str()
                        );
                    }
                    stop_evbase_loop();
                });
            else
                stop_evbase_loop();
            return;
        }
        
        if(!force){
            try{
                _log->info("Sending close message to worker: {}", _id);
                //_channel->sendcmd(EVMVC_CMD_CLOSE, nullptr, 0);
                _channel->sendcmd(
                    command(evmvc::CMD_CLOSE)
                );
                this->_status = running_state::stopped;
                return;
            }catch(const std::exception& err){
                _log->warn(err);
            }
        }
        if(!force)
            _log->warn(
                "Sending close message failed! closing channels"
            );
        
        _channel->close_channels();
        _channel.release();
        this->_status = running_state::stopped;
    }
    
    void close_service();
    void close_channels()
    {
        _channel->close_channels();
        _channel.release();
    }
    
    bool ping()
    {
        try{
            _channel->sendcmd(command(evmvc::CMD_PING));
            return true;
        }catch(const std::exception& err){
            
            return false;
        }
    }
    
    ssize_t send_log(
        md::log::log_level lvl, md::string_view path, md::string_view msg)
    {
        command c(evmvc::CMD_LOG);
        c.write((int)lvl);
        c.write(path);
        c.write(msg);
        return _channel->sendcmd(c);
        // size_t pl =
        //     sizeof(int) + // log level
        //     (sizeof(size_t) * 2) + // lengths of path and msg
        //     path.size() + msg.size();
        
        // char p[pl];
        // char* pp = p;
        // *((int*)pp) = (int)lvl;
        // pp += sizeof(int);
        
        // *((size_t*)pp) = path.size();
        // pp += sizeof(size_t);
        // for(size_t i = 0; i < path.size(); ++i)
        //     *((char*)pp++) = path.data()[i];

        // *((size_t*)pp) = msg.size();
        // pp += sizeof(size_t);
        // for(size_t i = 0; i < msg.size(); ++i)
        //     *((char*)pp++) = msg.data()[i];
        
        // return _channel->sendcmd(EVMVC_CMD_LOG, p, pl);
    }
    
    void emplace_parser(cmd_parser_fn fn)
    {
        _cmd_parsers.emplace_back(fn);
    }

    ssize_t send_cmd(const command& cmd)
    {
        return _channel->sendcmd(cmd);
    }
    ssize_t send_cmd(shared_command cmd)
    {
        return _channel->sendcmd(cmd);
    }
    
protected:
    virtual void parse_cmd(int cmd_id, const char* p, size_t plen);
    
    running_state _status = running_state::stopped;
    
    wp_app _app;
    app_options _config;
    worker_type _wtype;
    int _id;
    md::log::logger _log;
    
    int _pid;
    process_type _ptype;
    std::unique_ptr<evmvc::channel> _channel;

    md::callback::async_item_cb<evmvc::process_type> _started_cb;
    md::callback::async_item_cb<evmvc::process_type> _stopped_cb;
    
    std::vector<cmd_parser_fn> _cmd_parsers;
};

inline worker active_worker(worker w)
{
    static wp_worker _w;
    if(w)
        _w = w;
    return _w.lock();
}

inline void sinks::child_sink::log(
    md::string_view log_path,
    md::log::log_level lvl, md::string_view msg) const
{
    if(auto w = _w.lock())
        w->send_log(lvl, log_path, msg);
    #if EVMVC_BUILD_DEBUG
    else
        std::cerr << fmt::format(
            "\nWorker weak pointer is null, unable to send log:\n"
            "level: {}, path:{}\n{}\n",
            to_string(lvl), log_path, msg
        ) << std::endl;
    #endif
}


inline void channel::_init()
{
    channel_type ct = 
        _worker->proc_type() == process_type::master ?
            channel_type::master :
        _worker->proc_type() == process_type::child ?
            channel_type::child :
        channel_type::unknown;
    
    if(ct == channel_type::unknown)
        throw MD_ERR("invalid channel_type 'unknown'!");
    
    _type = ct;
    switch(ct){
        case channel_type::master:
            _init_master_channels();
            break;
        case channel_type::child:
            _init_child_channels();
            break;
        default:
            break;
    }
}


class http_worker_t
    : public worker_t
{
    static void on_http_worker_accept(int fd, short events, void* arg);
    
public:
    http_worker_t(const wp_app& app_t, const app_options& config,
        const md::log::logger& log)
        : worker_t(app_t, config, worker_type::http, log)
    {
    }
    
    int workload() const
    {
        //TODO: implement this...
        return -1;
    }
    
    void start(int argc, char** argv, int pid)
    {
        worker_t::start(argc, argv, pid);
        if(_ptype != process_type::child)
            return;
        
        _log->info("Starting worker, pid: {}", _pid);
        
        // init the event queue
        md::event_queue_t::reset(global::ev_base());
        
        _channel->rcmsg_ev = event_new(
            global::ev_base(), _channel->usock, EV_READ | EV_PERSIST,
            http_worker_t::on_http_worker_accept,
            this
        );
        event_add(_channel->rcmsg_ev, nullptr);
        
        for(auto& sc : _config.servers){
            auto s = std::make_shared<child_server_t>(
                this->shared_from_this(), sc, _log
            );
            s->start();
            _servers.emplace(s->id(), s);
        }
        
        event_base_loop(global::ev_base(), 0);
        event_base_free(global::ev_base());
        _log->info("Closing worker");
    }
    
    child_server find_server_by_id(size_t id)
    {
        auto it = _servers.find(id);
        if(it == _servers.end())
            return nullptr;
        return it->second;
    }
    child_server find_server_by_name(md::string_view name)
    {
        for(auto& s : _servers)
            if(!strcasecmp(s.second->name().c_str(), name.data()))
                return s.second;
        return nullptr;
    }
    child_server find_server_by_alias(md::string_view name)
    {
        for(auto& s : _servers){
            for(auto& alias : s.second->aliases()){
                if(!strcasecmp(alias.c_str(), name.data()))
                    return s.second;
            }
        }
        return nullptr;
    }
    
    void remove_connection(int cid)
    {
        auto it = _conns.find(cid);
        if(it == _conns.end())
            return;
        _conns.erase(cid);
    }
    
    http_worker shared_from_self()
    {
        return std::static_pointer_cast<http_worker_t>(
            this->shared_from_this()
        );
    }
    
    std::shared_ptr<const http_worker_t> shared_from_self() const
    {
        return std::static_pointer_cast<const http_worker_t>(
            this->shared_from_this()
        );
    }
    
private:
    void _revc_sock(size_t srv_id, int iproto, int sock_fd)
    {
        // fetch socket info
        char remote_addr[EVMVC_CTRL_MSG_MAX_ADDR_LEN]{0};
        uint16_t remote_port = 0;
        struct sockaddr saddr = {0};
        socklen_t slen;
        
        if(getpeername(sock_fd, &saddr, &slen) == -1)
            _log->error("getpeername failed, err: {}", errno);
            
        else{
            if(saddr.sa_family == AF_UNIX){
                auto addr_un = (sockaddr_un*)&saddr;
                memcpy(
                    remote_addr,
                    addr_un->sun_path,
                    sizeof(addr_un->sun_path)
                );
            
            }else if(saddr.sa_family == AF_INET){
                auto addr_in = (sockaddr_in*)&saddr;
                inet_ntop(
                    AF_INET, &addr_in->sin_addr, remote_addr, INET_ADDRSTRLEN
                );
                remote_port = ntohs(addr_in->sin_port);
                
            }else if(saddr.sa_family == AF_INET6){
                auto addr_in6 = (sockaddr_in6*)&saddr;
                inet_ntop(
                    AF_INET6,
                    &addr_in6->sin6_addr,
                    remote_addr,
                    INET6_ADDRSTRLEN
                );
                remote_port = ntohs(addr_in6->sin6_port);
            }
        }
        
        
        child_server srv = find_server_by_id(srv_id);
        if(!srv)
            _log->fatal(MD_ERR(
                "Invalid server id: '{}'", srv_id
            ));
        url_scheme proto = (url_scheme)iproto;
        
        sp_connection c = std::make_shared<connection>(
            this->_log,
            this->shared_from_self(),
            srv,
            sock_fd,
            proto,
            remote_addr,
            remote_port
        );
        c->initialize();
        _conns.emplace(c->id(), c);
    }
    
private:
    std::unordered_map<int, sp_connection> _conns;
    std::unordered_map<size_t, child_server> _servers;
};

class cache_worker
    : public std::enable_shared_from_this<cache_worker>,
    public worker_t
{
public:
    cache_worker(const wp_app& app_t, const app_options& config,
        const md::log::logger& log)
        : worker_t(app_t, config, worker_type::cache, log)
    {
    }
    
    int workload() const { return -1;}
    
private:
};






namespace _internal {
    
    // struct shared_ssl_sess
    // {
    //     //struct ebmb_node key;
    //     unsigned char key_data[SSL_MAX_SSL_SESSION_ID_LENGTH];
    //     int data_len;
    //     unsigned char data[EVMVC_MAX_SSL_DATA_LEN];
        
    //     struct shared_ssl_sess* p;
    //     struct shared_ssl_sess* n;
    // };
    
    inline int _ssl_sni_servername(SSL* s, int *al, void *arg)
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
        child_server_t* cs = (child_server_t*)SSL_CTX_get_app_data(ctx);
        if(!strcasecmp(cs->name().c_str(), name))
            return SSL_TLSEXT_ERR_OK;
        
        // find the requested server
        child_server rs = w->find_server_by_name(name);
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

    inline int _ssl_new_cache_entry(SSL* /*ssl*/, SSL_SESSION* sess)
    {
        //TODO: implement shared mem
        // int len = i2d_SSL_SESSION(sess, NULL);
        // unsigned char* buf = new unsigned char[len];
        // unsigned char* p = buf;
        // const unsigned char* cp = buf;
        // len = i2d_SSL_SESSION(sess, &p);
        
        // unsigned int sid_len;
        // const unsigned char* sid = SSL_SESSION_get_id(sess, &sid_len);
        
        return 0;
    }
    
    inline SSL_SESSION* _ssl_get_cache_entry(
        SSL* /*ssl*/, const unsigned char* /*sid*/,
        int /*sid_len*/, int* /*copy*/)
    {
        //TODO: implement shared mem
        return nullptr;
    }
    
    inline void _ssl_remove_cache_entry(SSL_CTX* /*ctx*/, SSL_SESSION* /*sess*/)
    {
        //TODO: implement shared mem
        return;
    }
    
    
    
}//::_internal

};//::evmvc
#endif//_libevmvc_worker_h
