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

#ifndef _libevmvc_child_server_h
#define _libevmvc_child_server_h

#include "stable_headers.h"
#include "utils.h"
#include "configuration.h"

namespace evmvc {
namespace _internal {
    int _ssl_new_cache_entry(SSL* ssl, SSL_SESSION* sess);
    SSL_SESSION* _ssl_get_cache_entry(
        SSL* ssl, const unsigned char* sid, int sid_len, int* copy
    );
    void _ssl_remove_cache_entry(SSL_CTX* ctx, SSL_SESSION* sess);
    
    int _ssl_sni_servername(SSL* s, int *al, void *arg);
    //int _ssl_client_hello(SSL* s, int *al, void *arg);
    
    
}//::_internal

class child_server
    : public std::enable_shared_from_this<child_server>
{
    static int nxt_id()
    {
        static int _id = -1;
        return ++_id;
    }
public:
    child_server(
        wp_worker worker,
        const server_options& config,
        const md::log::sp_logger& log)
        : 
        _worker(worker),
        _id(std::hash<std::string>{}(config.name)),
        _config(config),
        _log(log->add_child(
                "child server-" + config.name
        ))
    {
        EVMVC_DEF_TRACE("child_server {:p} created", (void*)this);
    }
    
    ~child_server()
    {
        EVMVC_DEF_TRACE("child_server {:p} released", (void*)this);
    }
    
    sp_worker get_worker() const;
    size_t id() const { return _id;}
    std::string name() const { return _config.name;}
    const std::vector<std::string>& aliases() const { return _config.aliases;}
    
    running_state status() const
    {
        return _status;
    }
    bool stopped() const { return _status == running_state::stopped;}
    bool starting() const { return _status == running_state::starting;}
    bool running() const { return _status == running_state::running;}
    bool stopping() const { return _status == running_state::stopping;}
    
    const server_options& config() const { return _config;}
    md::log::sp_logger log() const { return _log;}
    SSL_CTX* ssl_ctx() const { return _ssl_ctx;}
    
    const struct timeval& atimeo() const { return _config.atimeo;}
    struct timeval& atimeo() { return _config.atimeo;}
    const struct timeval& rtimeo() const { return _config.rtimeo;}
    struct timeval& rtimeo() { return _config.rtimeo;}
    const struct timeval& wtimeo() const { return _config.wtimeo;}
    struct timeval& wtimeo() { return _config.wtimeo;}
    
    void start()
    {
        if(!stopped())
            throw MD_ERR(
                "Server must be in stopped state to start listening again"
            );
        _status = running_state::starting;
        
        for(auto& l : _config.listeners)
            if(l.ssl){
                _init_ssl_ctx();
                break;
            }
        
        _status = running_state::running;
    }
    
    void stop()
    {
        if(stopped() || stopping())
            throw MD_ERR(
                "Child server must be in running state to be able to stop it."
            );
        this->_status = running_state::stopping;
        
        if(_ssl_ctx)
            SSL_CTX_free(_ssl_ctx);
        _ssl_ctx = nullptr;
        
        this->_status = running_state::stopped;
    }
    
    
private:
    
    void _init_ssl_ctx();
    
    evmvc::running_state _status = running_state::stopped;
    wp_worker _worker;
    size_t _id;
    server_options _config;
    md::log::sp_logger _log;
    
    SSL_CTX* _ssl_ctx = nullptr;
};



}//::evmvc
#endif//_libevmvc_child_server_h
