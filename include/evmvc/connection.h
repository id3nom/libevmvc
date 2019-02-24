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

#ifndef _libevmvc_connection_h
#define _libevmvc_connection_h

#include "stable_headers.h"
#include "logging.h"
#include "child_server.h"
#include "parser.h"
#include "request.h"
#include "response.h"

namespace evmvc {
namespace _internal {

    void on_connection_resume(int fd, short events, void* arg);
    void on_connection_read(struct bufferevent* bev, void* arg);
    void on_connection_write(struct bufferevent* bev, void* arg);
    void on_connection_event(struct bufferevent* bev, short events, void* arg);
    
}//::_internal

enum class conn_flags
{
    none            = 0,
    error           = (1 << 1),
    paused          = (1 << 2),
    connected       = (1 << 3),
    waiting         = (1 << 4),
    keepalive       = (1 << 5),
    server_via_sni  = (1 << 6),
    wait_release    = (1 << 7),
};
EVMVC_ENUM_FLAGS(evmvc::conn_flags);

enum class conn_protocol
{
    http,
    https
};

evmvc::string_view to_string(evmvc::conn_protocol p)
{
    switch(p){
        case evmvc::conn_protocol::http:
            return "http";
        case evmvc::conn_protocol::https:
            return "https";
        default:
            throw EVMVC_ERR("UNKNOWN conn_protocol: '{}'", (int)p);
    }
}



class connection
    : public std::enable_shared_from_this<connection>
{
    friend int _internal::_ssl_sni_servername(SSL* s, int *al, void *arg);
    friend void _internal::on_connection_resume(
        int fd, short events, void* arg);
    friend void _internal::on_connection_read(
        struct bufferevent* bev, void* arg);
    friend void _internal::on_connection_write(
        struct bufferevent* bev, void* arg);
    friend void _internal::on_connection_event(
        struct bufferevent* bev, short events, void* arg);
    
    static int nxt_id()
    {
        static int _id = -1;
        return ++_id;
    }
    
public:
    connection(
        const sp_logger& log,
        wp_http_worker worker,
        sp_child_server server,
        int sock_fd, evmvc::conn_protocol p)
        :
        _id(nxt_id()),
        _log(
            log->add_child(
                "conn/" + to_string(p).to_string() +
                "/sock:" + std::to_string(sock_fd)
            )
        ),
        _worker(worker),
        _server(server),
        _sock_fd(sock_fd),
        _protocol(p)
    {
    }
    
    ~connection()
    {
        close();
    }
    
    void initialize()
    {
        log()->debug("Initializing connection");
        
        switch(_protocol){
            case conn_protocol::http:
            case conn_protocol::https:
                _parser = std::make_shared<http_parser>(
                    this->shared_from_this(), _log
                );
                break;
            default:
                throw EVMVC_ERR("Unkonwn protocol: '{}'", (int)_protocol);
        }
        
        if(_protocol == conn_protocol::https){
            _ssl = SSL_new(_server->ssl_ctx());
            _bev = bufferevent_openssl_socket_new(
                global::ev_base(),
                _sock_fd,
                _ssl,
                BUFFEREVENT_SSL_ACCEPTING,
                BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS
            );
            
        }else{
            _bev = bufferevent_socket_new(
                global::ev_base(), 
                _sock_fd,
                BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS
            );
            
        }
        
        struct timeval rto = rtimeo();
        struct timeval wto = wtimeo();
        bufferevent_set_timeouts(_bev, &rto, &wto);
        
        _resume_ev = event_new(
            global::ev_base(), -1, EV_READ | EV_PERSIST,
            _internal::on_connection_resume,
            this
        );
        event_add(_resume_ev, nullptr);
        
        bufferevent_setcb(_bev,
            _internal::on_connection_read,
            _internal::on_connection_write,
            _internal::on_connection_event,
            this
        );
        bufferevent_enable(_bev, EV_READ);
    }
    
    int id() const { return _id;}
    const sp_logger& log() const { return _log;}
    sp_http_worker get_worker() const;
    sp_child_server server() const { return _server;}
    evmvc::conn_protocol protocol() const { return _protocol;}
    
    bool flag_is(conn_flags flag)
    {
        return EVMVC_TEST_FLAG(_flags, flag);
    }
    conn_flags& flags() { return _flags;}
    conn_flags flags() const { return _flags;}
    void set_conn_flag(conn_flags flag)
    {
        _flags |= flag;
    }
    void unset_conn_flag(conn_flags flag)
    {
        _flags &= ~flag;
    }
    
    bool secure() const { return _ssl;}
    
    bufferevent* bev() const
    {
        return _bev;
    }
    evbuffer* bev_in() const
    {
        return bufferevent_get_input(_bev);
    }
    evbuffer* bev_out() const
    {
        return bufferevent_get_output(_bev);
    }
    
    struct timeval atimeo() const
    {
        if(_atimeo.tv_sec > 0 || _atimeo.tv_usec > 0)
            return _atimeo;
        else if(_server->atimeo().tv_sec > 0 || _server->atimeo().tv_usec > 0)
            return _server->atimeo();
        else
            return {3, 0};
    }
    struct timeval rtimeo() const
    {
        if(_rtimeo.tv_sec > 0 || _rtimeo.tv_usec > 0)
            return _rtimeo;
        else if(_server->rtimeo().tv_sec > 0 || _server->rtimeo().tv_usec > 0)
            return _server->rtimeo();
        else
            return {3, 0};
    }
    struct timeval wtimeo() const
    {
        if(_wtimeo.tv_sec > 0 || _wtimeo.tv_usec > 0)
            return _wtimeo;
        else if(_server->wtimeo().tv_sec > 0 || _server->wtimeo().tv_usec > 0)
            return _server->wtimeo();
        else
            return {3, 0};
    }
    
    void set_timeouts(
        const struct timeval* ato,
        const struct timeval* rto,
        const struct timeval* wto)
    {
        if(ato)
            _atimeo = *ato;
        if(rto)
            _rtimeo = *rto;
        if(wto)
            _wtimeo = *wto;
    }
    
    void resume()
    {
        if(!flag_is(conn_flags::paused)){
            _log->error("Resuming unpaused connection!");
            set_conn_flag(conn_flags::error);
            return;
        }
        
        unset_conn_flag(conn_flags::paused);
        event_active(_resume_ev, EV_WRITE, 1);
    }
    
    void close();
    
private:
    int _id;
    sp_logger _log;
    wp_http_worker _worker;
    sp_child_server _server;
    int _sock_fd;
    evmvc::conn_protocol _protocol;
    
    conn_flags _flags = conn_flags::none;
    
    SSL* _ssl = nullptr;
    
    struct event* _resume_ev = nullptr;
    struct bufferevent* _bev = nullptr;
    struct evbuffer* _scratch_buf = nullptr;
    
    struct timeval _atimeo = {0,0};
    struct timeval _rtimeo = {0,0};
    struct timeval _wtimeo = {0,0};
    
    sp_parser _parser = nullptr;
    
    sp_request _req = nullptr;
    sp_response _res = nullptr;
    
    size_t _body_nread = 0;
};


}//::evmvc
#endif//_libevmvc_connection_h
