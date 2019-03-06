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

namespace evmvc { namespace _internal{

inline void send_error(
    evmvc::sp_response res, int status_code,
    md::string_view msg)
{
    res->error(
        (evmvc::status)status_code,
        MD_ERR(msg.data())
    );
}

inline void send_error(
    evmvc::sp_response res, int status_code,
    md::callback::cb_error err)
{
    res->error((evmvc::status)status_code, err);
}


inline evmvc::sp_response create_http_response(
    wp_connection conn,
    http_version ver,
    url uri,
    sp_header_map hdrs,
    sp_route_result rr)
{
    auto c = conn.lock();
    if(!c)
        throw MD_ERR("Failed to lock the connection weak ptr!");
    
    static uint64_t cur_id = 0;
    uint64_t rid = ++cur_id;
    
    md::log::sp_logger log = c->log()->add_child(rr->log()->path());
    evmvc::sp_http_cookies cks = std::make_shared<evmvc::http_cookies>(
        rid, log, rr->_route, uri, hdrs
    );
    /*
        uint64_t id,
        http_version ver,
        wp_connection conn,
        md::log::sp_logger log,
        const evmvc::sp_route& rt,
        url uri,
        evmvc::method met,
        md::string_view smet,
        sp_header_map hdrs,
        const sp_http_cookies& http_cookies,
        const std::vector<std::shared_ptr<evmvc::http_param>>& p
    */
    evmvc::sp_request req = std::make_shared<evmvc::request>(
        rid, ver, conn, log, rr->_route, uri,
        c->parser()->method(), c->parser()->method_string(),
        hdrs, cks, rr->params
    );
    evmvc::sp_response res = std::make_shared<evmvc::response>(
        rid, req, conn, log, rr->_route, uri, cks
    );
    
    return res;
}
    
    
}}//::evmvc::internal
