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

#ifndef _libevmvc_request_h
#define _libevmvc_request_h

#include "stable_headers.h"
#include "logging.h"
#include "utils.h"
#include "headers.h"
#include "fields.h"
#include "methods.h"
#include "cookies.h"
#include "http_param.h"
#include "files.h"
//#include "multipart_parser.h"

namespace evmvc {

class request
    : public std::enable_shared_from_this<request>
{
    friend void _internal::on_multipart_request(
        evhtp_request_t* req, void* arg);
    
public:
    request(
        uint64_t id,
        const evmvc::sp_route& rt,
        evmvc::sp_logger log,
        evhtp_request_t* ev_req,
        const sp_http_cookies& http_cookies,
        const std::vector<std::shared_ptr<evmvc::http_param>> p/*,
        _internal::multipart_parser* mp*/
        )
        : _id(id), _rt(rt),
        _log(log->add_child(
            "req-" + evmvc::num_to_str(id, false) +
            ev_req->uri->path->full
        )),
        _ev_req(ev_req),
        _headers(std::make_shared<evmvc::request_headers>(
            ev_req->headers_in
        )),
        _cookies(http_cookies),
        _rt_params(p), _body_params(), _files()
    {
        EVMVC_DEF_TRACE(
            "request '{}' created", this->id()
        );
        
        if(_log->should_log(log_level::trace)){
            std::string hdrs;
            evhtp_kv_t * kv;
            TAILQ_FOREACH(kv, ev_req->headers_in, next){
                hdrs += kv->key;
                hdrs += ": ";
                hdrs += kv->val;
                hdrs += "\n";
            }
            EVMVC_TRACE(_log, hdrs);
        }

        // if(!mp)
        //     return;
        // _load_multipart_params(mp->root);
    }
    
    ~request()
    {
        EVMVC_DEF_TRACE(
            "request '{}' released", this->id()
        );
    }

    uint64_t id() const { return _id;}
    evmvc::sp_app get_app() const;
    evmvc::sp_router get_router()const;
    evmvc::sp_route get_route()const { return _rt;}
    evmvc::sp_logger log() const { return _log;}

    evhtp_request_t* evhtp_request(){ return _ev_req;}
    evmvc::request_headers& headers() const { return *(_headers.get());}
    http_cookies& cookies() const { return *(_cookies.get());}
    http_files& files() const { return *(_files.get());}

    sp_http_param route_param(
        evmvc::string_view pname) const noexcept
    {
        for(auto& ele : _rt_params)
            if(strcasecmp(ele->name(), pname.data()) == 0)
                return ele;

        return sp_http_param();
    }

    template<typename ParamType>
    ParamType get_route_param(
        const evmvc::string_view& pname,
        ParamType default_val = ParamType()) const
    {
        for(auto& ele : _rt_params)
            if(strcasecmp(ele->name(), pname.data()) == 0)
                return ele->get<ParamType>();

        return default_val;
    }

    std::shared_ptr<evmvc::http_param> query_param(
        evmvc::string_view pname) const noexcept
    {
        if(!_ev_req->uri->query)
            return sp_http_param();

        auto p = _ev_req->uri->query->tqh_first;
        if(p == nullptr)
            return std::shared_ptr<evmvc::http_param>();

        while(p != nullptr){
            if(strcmp(p->key, pname.data()) == 0){
                char* pval = evhttp_decode_uri(p->val);
                std::string sval(pval);
                free(pval);
                return std::make_shared<evmvc::http_param>(
                    p->key, sval
                );
            }
            if(p == *_ev_req->uri->query->tqh_last)
                break;
            ++p;
        }

        return sp_http_param();
    }

    template<typename ParamType>
    ParamType get_query_param(
        const evmvc::string_view& pname,
        ParamType default_val = ParamType()) const
    {
        auto p = query_param(pname);
        if(p)
            return p->get<ParamType>();
        return default_val;
    }

    evmvc::method method()
    {
        return (evmvc::method)_ev_req->method;
    }

    bool is_ajax()
    {
        return this->headers().get("X-Requested-With")
            ->compare_value("XMLHttpRequest");
    }

    sp_http_param body_param(
        evmvc::string_view pname) const noexcept
    {
        for(auto& ele : _body_params)
            if(strcasecmp(ele->name(), pname.data()) == 0)
                return ele;

        return sp_http_param();
    }

    template<typename ParamType>
    ParamType get_body_param(
        const evmvc::string_view& pname,
        ParamType default_val = ParamType()) const
    {
        for(auto& ele : _body_params)
            if(strcasecmp(ele->name(), pname.data()) == 0)
                return ele->get<ParamType>();

        return default_val;
    }


protected:

    void _load_multipart_params(
        std::shared_ptr<_internal::multipart_subcontent> ms);

    uint64_t _id;
    evmvc::sp_route _rt;
    evmvc::sp_logger _log;
    evhtp_request_t* _ev_req;
    evmvc::sp_request_headers _headers;
    sp_http_cookies _cookies;
    //param_map _rt_params;
    evmvc::http_params _rt_params;
    evmvc::http_params _body_params;
    evmvc::sp_http_files _files;
};



} //ns evmvc
#endif //_libevmvc_request_h
