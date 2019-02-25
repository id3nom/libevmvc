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

namespace evmvc {
namespace _internal {
    void sig_received(int sig)
    {
        if(sig == SIGINT){
            //_run = false;
            event_base_loopbreak(global::ev_base());
        }
    }

    
    void on_http_worker_accept(int fd, short events, void* arg);
}//::_internal

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
evmvc::string_view to_string(worker_type t)
{
    switch(t){
        case worker_type::http:     return "http";
        case worker_type::cache:    return "cache";
        default:
            throw EVMVC_ERR("UNKNOWN worker_type: '{}'", (int)t);
    }
}

class worker
{
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
            log->add_child(
                "worker/" +
                to_string(wtype).to_string() + "/" +
                std::to_string(_id)
            )
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
    
    wp_app _app;
    app_options _config;
    worker_type _wtype;
    int _id;
    sp_logger _log;
    
    int _pid;
    process_type _ptype;
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
        default:
            break;
    }
}




class http_worker
    : public std::enable_shared_from_this<http_worker>,
    public worker
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
    
    void start(int pid)
    {
        worker::start(pid);
        if(_ptype == process_type::master)
            return;
        
        signal(SIGINT, _internal::sig_received);
        
        // will send a SIGINT when the parent dies
        prctl(PR_SET_PDEATHSIG, SIGINT);
        
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
    
private:
    void _revc_sock(size_t srv_id, int iproto, int sock_fd)
    {
        sp_child_server srv = find_server_by_id(srv_id);
        if(!srv)
            _log->fatal(EVMVC_ERR(
                "Invalid server id: '{}'", srv_id
            ));
        conn_protocol proto = (conn_protocol)iproto;
        
        sp_connection c = std::make_shared<connection>(
            this->_log,
            this->shared_from_this(),
            srv,
            sock_fd,
            proto
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


// request

sp_connection request::connection() const { return _conn.lock();}
bool request::secure() const { return _conn.lock()->secure();}

evmvc::conn_protocol request::protocol() const
{
    //TODO: add trust proxy options
    sp_header h = _headers->get("X-Forwarded-Proto");
    if(h){
        if(!strcasecmp(h->value(), "https"))
            return evmvc::conn_protocol::https;
        else if(!strcasecmp(h->value(), "http"))
            return evmvc::conn_protocol::http;
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

// response

sp_connection response::connection() const { return _conn.lock();}
bool response::secure() const { return _conn.lock()->secure();}

void response::send_file(
    const bfs::path& filepath,
    const evmvc::string_view& enc, 
    async_cb cb)
{
    this->resume();
    
    FILE* file_desc = nullptr;
    struct evmvc::_internal::file_reply* reply = nullptr;
    
    // open up the file
    file_desc = fopen(filepath.c_str(), "r");
    BOOST_ASSERT(file_desc != nullptr);
    
    // create internal file_reply struct
    reply = new evmvc::_internal::file_reply({
        this->shared_from_this(),
        _ev_req,
        file_desc,
        evbuffer_new(),
        nullptr,
        0,
        cb,
        this->_log
    });
    
    /* here we set a connection hook of the type `evhtp_hook_on_write`
    *
    * this will execute the function `http__send_chunk_` each time
    * all data has been written to the client.
    */
    evhtp_connection_set_hook(
        _ev_req->conn, evhtp_hook_on_write,
        (evhtp_hook)evmvc::_internal::send_file_chunk,
        reply
    );
    
    /* set a hook to be called when the client disconnects */
    evhtp_connection_set_hook(
        _ev_req->conn, evhtp_hook_on_connection_fini,
        (evhtp_hook)evmvc::_internal::send_file_fini,
        reply
    );
    
    // set file content-type
    auto mime_type = evmvc::mime::get_type(filepath.extension().c_str());
    if(evmvc::mime::compressible(mime_type)){
        EVMVC_DBG(this->_log, "file is compressible");
        
        sp_header hdr = _req->headers().get(
            evmvc::field::accept_encoding
        );
        
        if(hdr){
            auto encs = hdr->accept_encodings();
            int wsize = EVMVC_COMPRESSION_NOT_SUPPORTED;
            if(encs.size() == 0)
                wsize = EVMVC_ZLIB_GZIP_WSIZE;
            for(const auto& cenc : encs){
                if(cenc.type == encoding_type::gzip){
                    wsize = EVMVC_ZLIB_GZIP_WSIZE;
                    break;
                }
                if(cenc.type == encoding_type::deflate){
                    wsize = EVMVC_ZLIB_DEFLATE_WSIZE;
                    break;
                }
                if(cenc.type == encoding_type::star){
                    wsize = EVMVC_ZLIB_GZIP_WSIZE;
                    break;
                }
            }
            
            if(wsize != EVMVC_COMPRESSION_NOT_SUPPORTED){
                reply->zs = (z_stream*)malloc(sizeof(*reply->zs));
                memset(reply->zs, 0, sizeof(*reply->zs));
                
                int compression_level = Z_DEFAULT_COMPRESSION;
                if(deflateInit2(
                    reply->zs,
                    compression_level,
                    Z_DEFLATED,
                    wsize, //MOD_GZIP_ZLIB_WINDOWSIZE + 16,
                    EVMVC_ZLIB_MEM_LEVEL,// MOD_GZIP_ZLIB_CFACTOR,
                    EVMVC_ZLIB_STRATEGY// Z_DEFAULT_STRATEGY
                    ) != Z_OK
                ){
                    throw EVMVC_ERR("deflateInit2 failed!");
                }
                
                this->headers().set(
                    evmvc::field::content_encoding, 
                    wsize == EVMVC_ZLIB_GZIP_WSIZE ? "gzip" : "deflate"
                );
            }
        }
    }
    
    //TODO: get file encoding
    this->encoding(enc == "" ? "utf-8" : enc).type(
        filepath.extension().c_str()
    );
    this->_prepare_headers();
    _started = true;
    
    /* we do not have to start sending data from the file from here -
    * this function will write data to the client, thus when finished,
    * will call our `http__send_chunk_` callback.
    */
    evhtp_send_reply_chunk_start(_ev_req, EVHTP_RES_OK);
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
            
            int sock = *((int*)CMSG_DATA(cmsgp));
            w->_revc_sock(data.srv_id, data.iproto, sock);
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
    
    
    void on_connection_resume(int /*fd*/, short /*events*/, void* arg)
    {
        connection* c = (connection*)arg;
        EVMVC_DBG(c->_log, "resuming");
        
        c->unset_conn_flag(conn_flags::paused);
        
        if(c->_req){
            //TODO: set status to ok.
        }
        
        if(c->flag_is(conn_flags::wait_release)){
            c->close();
            return;
        }
        
        if(evbuffer_get_length(c->bev_out())){
            EVMVC_DBG(c->_log, "SET WAITING");

            c->set_conn_flag(conn_flags::waiting);
            if(!(bufferevent_get_enabled(c->_bev) & EV_WRITE)){
                EVMVC_DBG(c->_log, "ENABLING EV_WRITE");
                bufferevent_enable(c->_bev, EV_WRITE);
            }
            
        }else{
            EVMVC_DBG(c->_log, "SET READING");
            if(!(bufferevent_get_enabled(c->_bev) & EV_READ)){
                EVMVC_DBG(c->_log, "ENABLING EV_READ | EV_WRITE");
                bufferevent_enable(c->_bev, EV_READ | EV_WRITE);
            }
            
            if(evbuffer_get_length(c->bev_in())){
                EVMVC_DBG(c->_log, "data available calling on_connection_read");
                on_connection_read(c->_bev, arg);
            }
        }
    }
    
    void on_connection_read(struct bufferevent* /*bev*/, void* arg)
    {
        connection* c = (connection*)arg;
        
        size_t ilen = evbuffer_get_length(c->bev_in());
        if(ilen == 0)
            return;
        
        if(c->flag_is(conn_flags::paused))
            return EVMVC_DBG(c->log(), "Connection is paused, do nothing!");
        
        void* buf = evbuffer_pullup(c->bev_in(), ilen);
        
        cb_error ec;
        size_t nread = c->_parser->parse((const char*)buf, ilen, ec);
        if(nread > 0)
            evbuffer_drain(c->bev_in(), nread);
        
        if(ec){
            c->log()->error("Parse error:\n{}", ec);
            c->set_conn_flag(conn_flags::error);
            return;
        }
        
        if(c->_req){
        //     if(c->_req->status() == request_state::req_too_long){
        //         c->set_conn_flag(conn_flags::error);
        //         return;
        //     }
        }
        if(c->_res && c->_res->paused())
            return;
        
        if(nread < ilen){
            c->resume();
        }
    }
    
    void on_connection_write(struct bufferevent* /*bev*/, void* arg)
    {
        connection* c = (connection*)arg;
        
        if(c->flag_is(conn_flags::paused))
            return;
        
        if(c->flag_is(conn_flags::waiting)){
            c->unset_conn_flag(conn_flags::waiting);
            if(!(bufferevent_get_enabled(c->_bev) & EV_READ))
                bufferevent_enable(c->_bev, EV_READ);
            
            if(evbuffer_get_length(c->bev_in())){
                on_connection_read(c->_bev, arg);
                return;
            }
        }
        
        if(!c->_res->ended() || evbuffer_get_length(c->bev_out()))
            return;
        
        if(c->flag_is(conn_flags::keepalive)){
            c->_req.reset();
            c->_res.reset();
            c->_parser->reset();
        }else{
            c->close();
        }
    }
    
    void on_connection_event(
        struct bufferevent* /*bev*/, short events, void* arg)
    {
        connection* c = (connection*)arg;
        
        EVMVC_DBG(c->_log,
            "events: {}{}{}{}",
            events & BEV_EVENT_CONNECTED ? "connected " : "",
            events & BEV_EVENT_ERROR     ? "error "     : "",
            events & BEV_EVENT_TIMEOUT   ? "timeout "   : "",
            events & BEV_EVENT_EOF       ? "eof"       : ""
        );
        
        if((events & BEV_EVENT_CONNECTED)){
            EVMVC_DBG(c->_log, "CONNECTED");
            return;
        }
        
        if(c->_ssl && !(events & BEV_EVENT_EOF)){
            // ssl error
            c->set_conn_flag(conn_flags::error);
        }
        
        if(events == (BEV_EVENT_EOF | BEV_EVENT_READING)){
            EVMVC_DBG(c->_log, "EOF | READING");
            if(errno == EAGAIN){
                EVMVC_DBG(c->_log, "errno EAGAIN");
                
                if(!(bufferevent_get_enabled(c->_bev) & EV_READ))
                    bufferevent_enable(c->_bev, EV_READ);
                errno = 0;
                return;
            }
        }
        
        c->set_conn_flag(conn_flags::error);
        c->unset_conn_flag(conn_flags::connected);
        
        if(c->flag_is(conn_flags::paused))
            c->set_conn_flag(conn_flags::wait_release);
        else
            c->close();
    }
    
    
}//::_internal

};//::evmvc
#endif//_libevmvc_worker_h
