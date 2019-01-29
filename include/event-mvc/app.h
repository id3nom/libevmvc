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
#include "stable_headers.h"
#include "router.h"

extern "C" {
#define EVHTP_DISABLE_REGEX
#include <event2/http.h>
#include <evhtp/evhtp.h>
}

namespace evmvc {

class app
    : public std::enable_shared_from_this<app>
{
    template<class SesDerived, class ParserType, class ReqType>
    friend class http_session_reader;
    
    template<class Derived>
    friend class http_session;
    
public:
    app(
        const evmvc::string_view& root_dir)
        : _root_dir(root_dir), _router(std::make_shared<router>("/"))//,
        //_ssl_ctx(boost::asio::ssl::context::sslv23)
    {
    }
    
    ~app()
    {
    }
    
    void listen(
        //boost::asio::io_context& ioc,
        uint16_t port = 8080, const evmvc::string_view& address = "0.0.0.0")
    {
        evhtp_t* htp;
        evhtp_request_t* req;
        // auto const _address = boost::asio::ip::make_address(address.data());
        
        // // Create and launch a listening port
        // _listener = std::make_shared<listener>(
        //     this->shared_from_this(),
        //     ioc,
        //     _ssl_ctx,
        //     boost::asio::ip::tcp::endpoint{_address, port}
        // );
        
        // _listener->run();
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
    
    /*
    template<class Body, class Allocator, class Send>
    void _handle_request(
        // http::request<Body, http::basic_fields<Allocator>>&& req,
        // Send&& send
    )
    {
        http::verb v = req.method();
        evmvc::string_view sv;
        
        if(v == http::verb::unknown)
            sv = req.method_string();
        else
            sv = http::to_string(v);
        
        auto rr = _router->resolve_url(v, req.target());
        
        if(!rr && v == http::verb::head)
            rr = _router->resolve_url(http::verb::get, req.target());
        
        evmvc::response<Body, Allocator, Send> res(req, send);
        
        if(!rr){
            res.send_bad_request("Invalid route");
            return;
        }
        
        rr->execute(req, res,
        [&rr, &req, &res](auto error){
            if(error){
                res.send_bad_request(error.message());
                return;
            }
        });
    }
     */
    
    std::string _root_dir;
    sp_router _router;
    //sp_listener _listener;
    //boost::asio::ssl::context _ssl_ctx;
};



} // ns evmvc
#endif //_libevmvc_app_h
