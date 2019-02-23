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
#include "connection.h"

#define EVMVC_PIPE_WRITE_FD 1
#define EVMVC_PIPE_READ_FD 0

namespace evmvc {

class worker;
class http_worker;
class cache_worker;

typedef std::shared_ptr<worker> sp_worker;
typedef std::weak_ptr<worker> wp_worker;
typedef std::shared_ptr<http_worker> sp_http_worker;
typedef std::weak_ptr<http_worker> wp_http_worker;
typedef std::shared_ptr<cache_worker> sp_cache_worker;
typedef std::weak_ptr<cache_worker> wp_cache_worker;

enum class channel_type
{
    unknown,
    children,
    parent
};

class channel
{
public:
    channel(wp_http_worker worker)
        : _worker(worker)
    {
        pipe(ptoc);
        pipe(ctop);
    }
    
    ~channel()
    {
        close_channels();
    }
    
    sp_worker get_worker() const;
    
    void init(channel_type ct)
    {
        if(ct == channel_type::unknown)
            throw EVMVC_ERR("invalid channel_type 'unknown'!");
        
        _type = ct;
        switch(ct){
            case channel_type::children:
                _init_child_channels();
                break;
            case channel_type::parent:
                _init_parent_channels();
                break;
        }
    }
    
    void close_channels()
    {
        if(_type == channel_type::parent){
            _close_parent_channels();
        }else if(_type == channel_type::children){
            _close_child_channels();
        }
    }
    
private:
    void _init_parent_channels();
    void _init_child_channels();
    
    void _close_parent_channels()
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
    
    wp_http_worker _worker;
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
        : _id(nxt_id()), 
        _app(app),
        _log(app.lock()->log()->add_child("worker " + std::to_string(_id))),
        _config(config),
    {
    }
    
    virtual worker_type type() const = 0;
    
    sp_logger log() const { return _log;}
    
    int id() const { return _id;}
    sp_app get_app() const { return _app.lock();}
    channel& channel() { return _chan;}
    
    
    
private:
    int _id;
    wp_app _app;
    sp_logger _log;
    app_options _config;
    
    evmvc::channel _chan;
};


class http_worker
    : public std::enable_shared_from_this<http_worker>
{
public:
    http_worker(wp_app app, const app_options& config)
    {
    }
    
    worker_type type() const { return worker_type::http;}
    
    
private:
    void _init_ssl()
    {
        
    }
    
    std::unordered_map<int, sp_connection> _conns;
};

class cache_worker
    : public std::enable_shared_from_this<cache_worker>
{
public:
    cache_worker(wp_app app, const app_options& config)
    {
    }
    
    worker_type type() const { return worker_type::cache;}
    
    
private:
    void _init_ssl()
    {
        
    }
};


};//::evmvc
#endif//_libevmvc_worker_h
