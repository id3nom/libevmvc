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

namespace evmvc {

class connection;
typedef std::shared_ptr<connection> sp_connection;

enum class conn_flags
{
    none            = 0,
    error           = (1 << 1),
    paused          = (1 << 2),
    connected       = (1 << 3),
    waiting         = (1 << 4),
    keepalive       = (1 << 5),
    server_via_sni  = (1 << 6)
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
    }
}


class connection
{
    friend int _internal::_ssl_sni_servername(SSL* s, int *al, void *arg);
    
    static int nxt_id()
    {
        static int _id = -1;
        return ++_id;
    }
    
public:
    connection(
        wp_http_worker worker,
        sp_child_server server,
        int sock_fd, evmvc::conn_protocol p)
        : _worker(worker),
        _server(server),
        _sock_fd(sock_fd),
        _protocol(p)
    {
    }
    
    ~connection()
    {
    }
    
    sp_http_worker get_worker() const;
    sp_child_server server() const { return _server;}
    evmvc::conn_protocol protocol() const { return _protocol;}
    
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
    
    void set_timeouts(const struct timeval* rto, const struct timeval* wto)
    {
        if(rto)
            _rtimeo = *rto;
        if(wto)
            _wtimeo = *wto;
    }
    
private:
    
    wp_http_worker _worker;
    sp_child_server _server;
    int _sock_fd;
    evmvc::conn_protocol _protocol;
    
    conn_flags _flags = conn_flags::none;
    
    SSL* _ssl = nullptr;
    
    struct event* _resume_ev = nullptr;
    struct bufferevent* _bev = nullptr;
    
    struct timeval _rtimeo = {0,0};
    struct timeval _wtimeo = {0,0};
};

}//::evmvc
#endif//_libevmvc_connection_h
