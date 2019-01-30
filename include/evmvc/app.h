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
#include "router.h"

extern "C" {
#define EVHTP_DISABLE_REGEX
#include <event2/http.h>
#include <evhtp/evhtp.h>
}

namespace evmvc {

enum class app_state
{
    stopped,
    starting,
    running,
    stopping,
};

void _on_app_request(evhtp_request_t* req, void* arg);

class app
    : public std::enable_shared_from_this<app>
{
    friend void _on_app_request(evhtp_request_t* req, void* arg);
    
public:
    app(
        const evmvc::string_view& root_dir)
        : _status(app_state::stopped), _root_dir(root_dir),
        _router(std::make_shared<router>("/")),
        _evbase(nullptr), _evhtp(nullptr)
    {
    }
    
    ~app()
    {
    }
    
    app_state status()
    {
        return _status;
    }
    
    bool stopped(){ return _status == app_state::stopped;}
    bool starting(){ return _status == app_state::starting;}
    bool running(){ return _status == app_state::running;}
    bool stopping(){ return _status == app_state::stopping;}
    
    void listen(
        event_base* evbase,
        uint16_t port = 8080,
        const evmvc::string_view& address = "0.0.0.0",
        int backlog = -1)
    {
        if(!stopped())
            throw std::runtime_error(
                "app must be in stopped state to start listening again"
            );
        
        _evbase = evbase;
        listen(port, address);
    }
    
    void listen(
        uint16_t port = 8080,
        const evmvc::string_view& address = "0.0.0.0",
        int backlog = -1)
    {
        if(!stopped())
            throw std::runtime_error(
                "app must be in stopped state to start listening again"
            );
        _status = app_state::starting;
        
        _evhtp = evhtp_new(_evbase, NULL);
        evhtp_set_gencb(_evhtp, _on_app_request, this);
        evhtp_enable_flag(_evhtp, EVHTP_FLAG_ENABLE_ALL);
        
        evhtp_bind_socket(_evhtp, address.data(), port, backlog);
        
        _status = app_state::running;
    }
    
    void stop()
    {
        
    }
    
    sp_router all(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->all(route_path, cb);
    }
    sp_router options(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->options(route_path, cb);
    }
    sp_router del(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->del(route_path, cb);
    }
    sp_router head(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->head(route_path, cb);
    }
    sp_router get(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->get(route_path, cb);
    }
    sp_router post(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->post(route_path, cb);
    }
    sp_router put(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->put(route_path, cb);
    }
    sp_router connect(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->connect(route_path, cb);
    }
    sp_router trace(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->trace(route_path, cb);
    }
    sp_router patch(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return _router->patch(route_path, cb);
    }
    // sp_router purge(
    //     const evmvc::string_view& route_path, route_handler_cb cb)
    // {
    //     return _router->purge(route_path, cb);
    // }
    // sp_router link(
    //     const evmvc::string_view& route_path, route_handler_cb cb)
    // {
    //     return _router->link(route_path, cb);
    // }
    // sp_router unlink(
    //     const evmvc::string_view& route_path, route_handler_cb cb)
    // {
    //     return _router->unlink(route_path, cb);
    // }
    
    sp_router add_route_handler(
        const evmvc::string_view& verb,
        const evmvc::string_view& route_path,
        route_handler_cb cb)
    {
        return _router->add_route_handler(verb, route_path, cb);
    }
    
    sp_router add_router(sp_router router)
    {
        return _router->add_router(router);
    }
    
private:
    
    app_state _status;
    std::string _root_dir;
    sp_router _router;
    struct event_base* _evbase;
    struct evhtp* _evhtp;
    
};

void _on_app_request(evhtp_request_t* req, void* arg)
{
    app* a = (app*)arg;
    
    evmvc::method v = (evmvc::method)req->method;
    evmvc::string_view sv;
    
    if(v == evmvc::method::unknown)
        sv = "";// req-> req.method_string();
    else
        sv = evmvc::method_to_string(v);
    
    char* dp = evhttp_decode_uri(req->uri->path->full);
    
    auto rr = a->_router->resolve_url(v, dp);
    if(!rr && v == evmvc::method::head)
        rr = a->_router->resolve_url(evmvc::method::get, dp);
    free(dp);
    
    evmvc::response res(req);
    if(!rr){
        res.send_status(evmvc::status::not_found);
        //res.send_bad_request("Invalid route");
        return;
    }
    
    rr->execute(req, res,
    [&rr, &req, &res](auto error){
        if(error){
            res.send_status(evmvc::status::internal_server_error);
            //res.send_bad_request(error.c_str());
            return;
        }
    });
}


} // ns evmvc
#endif //_libevmvc_app_h
