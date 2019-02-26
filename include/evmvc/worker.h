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

#include <sys/prctl.h>

#define EVMVC_PIPE_WRITE_FD 1
#define EVMVC_PIPE_READ_FD 0

#define EVMVC_CMD_PING 0
#define EVMVC_CMD_PONG 1
#define EVMVC_CMD_LOG 2


namespace evmvc {
namespace _internal {
    void sig_received(int sig)
    {
        if(sig == SIGINT){
            //_run = false;
            event_base_loopbreak(global::ev_base());
        }
    }
    
    void _on_event_log(int severity, const char *msg)
    {
        if(severity == _EVENT_LOG_DEBUG)
            evmvc::_internal::default_logger()->debug(msg);
        else
            evmvc::_internal::default_logger()->error(msg);
    }
    
    void _on_event_fatal_error(int err)
    {
        evmvc::_internal::default_logger()->fatal(
            "Exiting because libevent send a fatal error: '{}'",
            err
        );
    }
    
    
    void on_http_worker_accept(int fd, short events, void* arg);
    
}//::_internal

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
    
    
    ssize_t sendcmd(int cmd_id, const char* payload, size_t payload_len)
    {
        // #if EVMVC_BUILD_DEBUG
        // std::clog << fmt::format(
        //     "Sending command, cmd_id: {}, size: {}\n", cmd_id, payload_len
        // ) << std::endl;
        // #endif
        
        int fd = _type == channel_type::child ?
            ctop[EVMVC_PIPE_WRITE_FD] : ptoc[EVMVC_PIPE_WRITE_FD];
        
        ssize_t tn = 0;
        ssize_t n = _internal::writen(
            fd,
            &cmd_id,
            sizeof(int)
        );
        if(n == -1){
            std::cerr << fmt::format(
                "Unable to send cmd, err: '{}'\n", errno
            );
            return -1;
        }
        tn += n;
        n = _internal::writen(
            fd,
            &payload_len,
            sizeof(size_t)
        );
        if(n == -1){
            std::cerr << fmt::format(
                "Unable to send cmd, err: '{}'\n", errno
            );
            return -1;
        }
        tn += n;
        if(payload_len > 0){
            n = _internal::writen(
                fd,
                payload,
                payload_len
            );
            if(n == -1){
                std::cerr << fmt::format(
                    "Unable to send cmd, err: '{}'\n", errno
                );
                return -1;
            }
            tn += n;
        }
        
        // send the command
        fsync(fd);
        
        return tn;
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
    
    evmvc::worker* _worker;
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
evmvc::string_view to_string(worker_type t)
{
    switch(t){
        case worker_type::http:     return "http";
        case worker_type::cache:    return "cache";
        default:
            throw EVMVC_ERR("UNKNOWN worker_type: '{}'", (int)t);
    }
}

namespace sinks{
class child_sink
    : public logger_sink
{
public:
    child_sink(std::weak_ptr<worker> w)
        : logger_sink(log_level::info), _w(w)
    {
    }
    
    void log(
        evmvc::string_view log_path,
        log_level lvl, evmvc::string_view msg
    ) const;
private:
    std::weak_ptr<worker> _w;
};
}//::sinks

void channel_cmd_read(int fd, short events, void* arg);

class worker
    : public std::enable_shared_from_this<worker>
{
    friend void channel_cmd_read(int fd, short events, void* arg);
    static int nxt_id()
    {
        static int _id = -1;
        return ++_id;
    }
    
protected:
    worker(const wp_app& app, const app_options& config,
        worker_type wtype, const sp_logger& log)
        : _app(app),
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
    
    ~worker()
    {
        if(running())
            stop();
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
    
    sp_logger log() const { return _log;}
    
    int id() const { return _id;}
    int pid() { return _pid;}
    process_type proc_type() const { return _ptype;}
    
    sp_app get_app() const { return _app.lock();}
    evmvc::channel& channel() const { return *(_channel.get());}
    
    virtual void start(int argc, char** argv, int pid)
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
            signal(SIGINT, _internal::sig_received);

            _ptype = process_type::child;
            _pid = getpid();
            
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
            
            auto c_sink = std::make_shared<evmvc::sinks::child_sink>(
                this->shared_from_this()
            );
            c_sink->set_level(_log->level());
            _log->replace_sink(c_sink);
            //_log->info("Sink level set to '{}'", to_string(_log->level()));
            std::clog << fmt::format(
                "Sink level set to '{}'", to_string(_log->level())
            ) << std::endl;
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
    
    
    bool ping()
    {
        try{
            _channel->sendcmd(EVMVC_CMD_PING, nullptr, 0);
            return true;
        }catch(const std::exception& err){
            
            return false;
        }
    }
    
    ssize_t send_log(
        evmvc::log_level lvl, evmvc::string_view path, evmvc::string_view msg)
    {
        size_t pl =
            sizeof(int) + // log level
            (sizeof(size_t) * 2) + // lengths of path and msg
            path.size() + msg.size();
        
        char p[pl];
        char* pp = p;
        *((int*)pp) = (int)lvl;
        pp += sizeof(int);
        
        *((size_t*)pp) = path.size();
        pp += sizeof(size_t);
        for(size_t i = 0; i < path.size(); ++i)
            *((char*)pp++) = path.data()[i];

        *((size_t*)pp) = msg.size();
        pp += sizeof(size_t);
        for(size_t i = 0; i < msg.size(); ++i)
            *((char*)pp++) = msg.data()[i];
        
        return _channel->sendcmd(EVMVC_CMD_LOG, p, pl);
    }

protected:
    virtual void parse_cmd(int cmd_id, const char* p, size_t plen)
    {
        switch(cmd_id){
            case EVMVC_CMD_PING:{
                EVMVC_DBG(_log, "CMD PING recv");
                _channel->sendcmd(EVMVC_CMD_PONG, nullptr, 0);
                break;
            }
            case EVMVC_CMD_PONG:{
                EVMVC_DBG(_log, "CMD PONG recv");
                
                break;
            }
            case EVMVC_CMD_LOG:{
                EVMVC_DBG(_log, "CMD LOG recv");
                log_level lvl = (log_level)(*(int*)p);
                p += sizeof(int);
                
                size_t log_path_size = *(size_t*)p;
                p += sizeof(size_t);
                const char* log_path = p;
                p += log_path_size;
                
                size_t log_msg_size = *(size_t*)p;
                p += sizeof(size_t);
                const char* log_msg = p;
                p += log_msg_size;
                
                _log->log(
                    evmvc::string_view(log_path, log_path_size),
                    lvl,
                    evmvc::string_view(log_msg, log_msg_size)
                );
                
                break;
            }
            default:
                if(plen)
                    _log->warn(
                        "unknown cmd_id: '{}', payload len: '{}'\n"
                        "payload: '{}'",
                        cmd_id, plen,
                        evmvc::base64_encode(evmvc::string_view(p, plen))
                    );
                else
                    _log->warn(
                        "unknown cmd_id: '{}', payload len: '{}'",
                        cmd_id, plen
                    );
                break;
        }
    }
    
    running_state _status = running_state::stopped;
    
    wp_app _app;
    app_options _config;
    worker_type _wtype;
    int _id;
    sp_logger _log;
    
    int _pid;
    process_type _ptype;
    std::unique_ptr<evmvc::channel> _channel;
};

void sinks::child_sink::log(
    evmvc::string_view log_path,
    log_level lvl, evmvc::string_view msg) const
{
    if(auto w = _w.lock())
        w->send_log(lvl, log_path, msg);
    #if EVMVC_BUILD_DEBUG
    else
        std::cerr << fmt::format(
            "\nWorker weak pointer is null, unable to send log:\n"
            "level: {}, path:{}\nmsg:{}\n",
            (int)lvl, log_path, msg
        ) << std::endl;
    #endif
}


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
        default:
            break;
    }
}


class http_worker
    : public worker
{
    friend void _internal::on_http_worker_accept(
        int fd, short events, void* arg
    );
    
public:
    http_worker(const wp_app& app, const app_options& config,
        const sp_logger& log)
        : worker(app, config, worker_type::http, log)
    {
    }
    
    int workload() const
    {
        //TODO: implement this...
        return -1;
    }
    
    void start(int argc, char** argv, int pid)
    {
        worker::start(argc, argv, pid);
        if(_ptype == process_type::master)
            return;
        
        _log->info("Starting worker");
        
        // will send a SIGINT when the parent dies
        prctl(PR_SET_PDEATHSIG, SIGINT);
        
        event_set_log_callback(_internal::_on_event_log);
        event_set_fatal_callback(_internal::_on_event_fatal_error);
        global::ev_base(event_base_new(), false, true);
        
        _channel->rcmsg_ev = event_new(
            global::ev_base(), _channel->usock, EV_READ | EV_PERSIST,
            _internal::on_http_worker_accept,
            this
        );
        event_add(_channel->rcmsg_ev, nullptr);
        
        for(auto& sc : _config.servers){
            auto s = std::make_shared<child_server>(sc, _log);
            s->start();
            _servers.emplace(s->id(), s);
        }
        
        event_base_loop(global::ev_base(), 0);
        event_base_free(global::ev_base());
        _log->info("Closing worker");
    }
    
    sp_child_server find_server_by_id(size_t id)
    {
        auto it = _servers.find(id);
        if(it == _servers.end())
            return nullptr;
        return it->second;
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
    
    void remove_connection(int cid)
    {
        auto it = _conns.find(cid);
        if(it == _conns.end())
            return;
        _conns.erase(cid);
    }
    
    sp_http_worker shared_from_self()
    {
        return std::static_pointer_cast<http_worker>(
            this->shared_from_this()
        );
    }

    std::shared_ptr<const http_worker> shared_from_self() const
    {
        return std::static_pointer_cast<const http_worker>(
            this->shared_from_this()
        );
    }
    
private:
    void _revc_sock(size_t srv_id, int iproto,
        const char* remote_addr, uint16_t remote_port, int sock_fd)
    {
        sp_child_server srv = find_server_by_id(srv_id);
        if(!srv)
            _log->fatal(EVMVC_ERR(
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
    
    std::unordered_map<int, sp_connection> _conns;
    std::unordered_map<size_t, sp_child_server> _servers;
};

class cache_worker
    : public std::enable_shared_from_this<cache_worker>,
    public worker
{
public:
    cache_worker(const wp_app& app, const app_options& config,
        const sp_logger& log)
        : worker(app, config, worker_type::cache, log)
    {
    }
    
    int workload() const { return -1;}
    
private:
};



// connection

sp_http_worker connection::get_worker() const
{
    return _worker.lock();
}

void connection::close()
{
    if(_closed)
        return;
    _closed = true;
    EVMVC_TRACE(this->_log, "closing\n{}", this->debug_string());
    
    _req.reset();
    _res.reset();
    _parser.reset();
    if(_scratch_buf){
        evbuffer_free(_scratch_buf);
        _scratch_buf = nullptr;
    }
    if(_resume_ev){
        event_free(_resume_ev);
        _resume_ev = nullptr;
    }
    if(_bev){
        if(_ssl){
            SSL_set_shutdown(_ssl, SSL_RECEIVED_SHUTDOWN);
            SSL_shutdown(_ssl);
            _ssl = nullptr;
        }
        bufferevent_free(_bev);
        _bev = nullptr;
    }
    
    if(auto w = _worker.lock())
        w->remove_connection(this->_id);
}

// parser
struct bufferevent* http_parser::bev() const
{
    sp_connection c = get_connection();
    return c->bev();
}


// request

sp_connection request::connection() const { return _conn.lock();}
bool request::secure() const { return _conn.lock()->secure();}

evmvc::url_scheme request::protocol() const
{
    //TODO: add trust proxy options
    sp_header h = _headers->get("X-Forwarded-Proto");
    if(h){
        if(!strcasecmp(h->value(), "https"))
            return evmvc::url_scheme::https;
        else if(!strcasecmp(h->value(), "http"))
            return evmvc::url_scheme::http;
        else
            throw EVMVC_ERR(
                "Invalid 'X-Forwarded-Proto': '{}'", h->value()
            );
    }
    
    return _conn.lock()->protocol();
}
std::string request::protocol_string() const
{
    //TODO: add trust proxy options
    sp_header h = _headers->get("X-Forwarded-Proto");
    if(h)
        return boost::to_lower_copy(
            std::string(h->value())
        );
    
    return to_string(_conn.lock()->protocol()).to_string();
}





namespace _internal {
    void on_http_worker_accept(int fd, short events, void* arg)
    {
        http_worker* w = (http_worker*)arg;
        
        while(true){
            //int data;
            ctrl_msg_data data;
            struct msghdr msgh;
            struct iovec iov;
            
            union
            {
                char buf[CMSG_SPACE(sizeof(ctrl_msg_data))];
                struct cmsghdr align;
            } ctrl_msg;
            struct cmsghdr* cmsgp;
            
            msgh.msg_name = NULL;
            msgh.msg_namelen = 0;
            
            msgh.msg_iov = &iov;
            msgh.msg_iovlen = 1;
            iov.iov_base = &data;
            iov.iov_len = sizeof(ctrl_msg_data);
            
            msgh.msg_control = ctrl_msg.buf;
            msgh.msg_controllen = sizeof(ctrl_msg.buf);
            
            int nr = recvmsg(fd, &msgh, 0);
            if(nr == 0)
                return;
            if(nr == -1){
                int terrno = errno;
                if(terrno == EAGAIN || terrno == EWOULDBLOCK)
                    return;
                
                w->_log->fatal(EVMVC_ERR(
                    "recvmsg: " + std::to_string(terrno)
                ));
            }
            
            if(nr > 0)
                w->_log->debug(
                    "received data, srv_id: {}, iproto: {}",
                    data.srv_id, data.iproto
                );
                //fprintf(stderr, "Received data = %d\n", data);
            
            cmsgp = CMSG_FIRSTHDR(&msgh);
            /* Check the validity of the 'cmsghdr' */
            if (cmsgp == NULL || cmsgp->cmsg_len != CMSG_LEN(sizeof(int)))
                w->_log->fatal(EVMVC_ERR("bad cmsg header / message length"));
            if (cmsgp->cmsg_level != SOL_SOCKET)
                w->_log->fatal(EVMVC_ERR("cmsg_level != SOL_SOCKET"));
            if (cmsgp->cmsg_type != SCM_RIGHTS)
                w->_log->fatal(EVMVC_ERR("cmsg_type != SCM_RIGHTS"));
            
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wstrict-aliasing"
            int sock = *((int*)CMSG_DATA(cmsgp));
            #pragma GCC diagnostic pop
            w->_revc_sock(
                data.srv_id, data.iproto, data.addr, data.port, sock
            );
        }
    }
    
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
