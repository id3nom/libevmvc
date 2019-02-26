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

#include "app.h"
#include "parser.h"

namespace evmvc {

void http_parser::validate_headers()
        // wp_connection conn,
        // sp_logger log,
        // url uri,
        // http_version ver,
        // sp_header_map hdrs,
        // sp_app a)
{
    sp_connection c = this->_conn.lock();
    if(!c){
        _status = parser_state::error;
        return;
    }
    std::string base_url = c->secure() ? "https://" : "http://";
    auto it = _hdrs->find("host");
    if(it == _hdrs->end()){
        _status = parser_state::error;
        return;
    }
    base_url += evmvc::trim_copy(it->second.front());
    url uri(base_url, _uri_string);
    
    _log->success(
        "REQ received,"
        "host: '{}', method: '{}', uri: '{}'",
        "Not Found",
        _method,
        _uri_string
    );
    
    sp_app a = this->_conn.lock()->get_worker()->get_app();
    
    // search a valid route_result
    // evmvc::method v = (evmvc::method)req->method;
    // evmvc::string_view sv;
    //
    // if(v == evmvc::method::unknown)
    //     throw EVMVC_ERR("Unknown method are not implemented!");
    //     //sv = "";// req-> req.method_string();
    // else
    //     sv = evmvc::to_string(v);
    
    auto rr = a->_router->resolve_url(_method_string, uri.path());
    if(!rr && _method == evmvc::method::head)
        rr = a->_router->resolve_url(evmvc::method::get, uri.path());
    
    // get client ip
    // auto con_addr_in = (sockaddr_in*)req->conn->saddr;
    // char addr[INET_ADDRSTRLEN];
    // inet_ntop(AF_INET, &con_addr_in->sin_addr, addr, INET_ADDRSTRLEN);
    
    // stop request if no valid route found
    if(!rr){
        c->log()->fail(
            "recv: [{}] [] from: [{}:{}], err: 404",
            _method_string,
            uri.to_string(),
            c->remote_address(),
            c->remote_port()
        );
        
        _status = parser_state::error;

        
        _res = _internal::create_http_response(
            _conn, _http_ver, uri, _hdrs,
            std::make_shared<route_result>(route::null(a))
        );
        
        _res->error(
            evmvc::status::not_found,
            EVMVC_ERR(
                "Unable to find ressource at '{}'",
                req->uri->path->full
            )
        );
        
    }
    
    c->log()->success(
        "recv: [{}] [] from: [{}:{}]",
        _method_string,
        uri.to_string(),
        c->remote_address(),
        c->remote_port()
    );
    
    // create the response
    _res = _internal::create_http_response(
        _conn, _http_ver, uri, _hdrs, rr
    );
    
    // evmvc::_internal::request_args* ra = 
    //     new evmvc::_internal::request_args();
    // ra->ready = false;
    // ra->rr = rr;
    // ra->res = res;
    //
    // // update request cb.
    // req->cb = on_app_request;
    // req->cbarg = ra;
    
    // create validation context
    evmvc::policies::filter_rule_ctx ctx = 
        evmvc::policies::new_context(_res);
    
    _res->pause();
    rr->validate_access(
        ctx,
    [a, rr, _res](const evmvc::cb_error& err){
        _res->resume([a, rr, _res, va_err = err](const evmvc::cb_error& err){
            if(err)
                return _res->error(
                    evmvc::status::unauthorized,
                    err
                );
            
            if(va_err)
                _res->error(
                    evmvc::status::unauthorized,
                    va_err
                );
            
            // if(evmvc::_internal::is_multipart_data(req, hdr)){
            //     evmvc::_internal::parse_multipart_data(
            //         a->log(),
            //         req, hdr, ra,
            //         a->options().temp_dir
            //     );
            // } else {
            //     if(ra->ready)
            //         _internal::on_app_request(req, ra);
            //     else
            //         ra->ready = true;
            // }
        });
    });
}

void http_parser::init_multip()
{
    sp_app a = _conn.lock()->get_worker()->get_app();
    _mp_temp_dir = a->options().temp_dir;
    
    _mp_buf = evbuffer_new();
    
    std::string boundary = _internal::_mp_get_boundary(_log, _hdrs);
    if(boundary.size() == 0){
        _log->error(EVMVC_ERR(
            "Invalid multipart/form-data boundary"
        ));
        _status = parser_state::error;
        return;
    }
    
    auto cc = std::make_shared<_internal::multipart_subcontent>();
    cc->sub_type = _internal::multipart_subcontent_type::root;
    cc->parent = std::static_pointer_cast<_internal::multipart_subcontent>(
        cc->shared_from_this()
    );
    cc->set_boundary(boundary);
    _mp_current = _mp_root = cc;
}


}//::evmvc
