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
#include "utils.h"
#include "headers.h"
#include "fields.h"
#include "methods.h"
#include "cookies.h"
#include "http_param.h"

namespace evmvc {

class request;
typedef std::shared_ptr<request> sp_request;

class request
    : public std::enable_shared_from_this<request>
{
public:
    
    request(
        uint64_t id,
        const evmvc::sp_app& app,
        std::shared_ptr<spdlog::logger> log,
        evhtp_request_t* ev_req,
        const sp_http_cookies& http_cookies,
        //const param_map& p
        const std::vector<std::shared_ptr<evmvc::http_param>> p
        )
        : _id(id), _app(app), _log(log),
        _ev_req(ev_req), _cookies(http_cookies),
        _rt_params(p)
    {
    }
    
    uint64_t id() const { return _id;}
    evmvc::sp_app app() const { return _app;}
    std::shared_ptr<spdlog::logger> log() const { return _log;}
    
    evhtp_request_t* evhtp_request(){ return _ev_req;}
    http_cookies& cookies() const { return *(_cookies.get());}
    sp_http_cookies shared_cookies() const { return _cookies;}
    
    sp_http_param route_param(
        evmvc::string_view pname) const noexcept
    {
        for(auto& ele : _rt_params)
            if(strcmp(ele->name(), pname.data()) == 0)
                return ele;
        
        return sp_http_param();
        // auto p = _rt_params.find(pname.to_string());
        // if(p == _rt_params.end())
        //     return std::shared_ptr<evmvc::http_param>();
        // return p->second;
    }
    
    template<typename ParamType>
    ParamType route_param_as(
        const evmvc::string_view& pname,
        ParamType default_val = ParamType()) const
    {
        for(auto& ele : _rt_params)
            if(strcmp(ele->name(), pname.data()) == 0)
                return ele->get<ParamType>();
        
        return default_val;
        // auto p = _rt_params.find(pname.to_string());
        // if(p == _rt_params.end())
        //     return default_val;
        // return p->second->get<ParamType>();
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
    ParamType query_param_as(
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
    
    evmvc::sp_header get(evmvc::field header_name) const
    {
        return get(to_string(header_name));
    }
    
    evmvc::sp_header get(evmvc::string_view header_name) const
    {
        evhtp_kv_t* header = nullptr;
        if((header = evhtp_headers_find_header(
            _ev_req->headers_in, header_name.data()
        )) != nullptr)
            return std::make_shared<evmvc::header>(
                header->key,
                header->val
            );
        return nullptr;
    }
    
    std::vector<evmvc::sp_header> list(evmvc::field header_name) const
    {
        return list(to_string(header_name));
    }
    
    std::vector<evmvc::sp_header> list(evmvc::string_view header_name) const
    {
        std::vector<evmvc::sp_header> vals;
        
        evhtp_kv_t* kv;
        TAILQ_FOREACH(kv, _ev_req->headers_in, next){
            if(strcmp(kv->key, header_name.data()) == 0)
                vals.emplace_back(
                    std::make_shared<evmvc::header>(
                        kv->key,
                        kv->val
                    )
                );
        }
        
        return vals;
    }
    
    bool is_ajax()
    {
        return get("X-Requested-With")->compare_value("XMLHttpRequest");
    }
    
    template <typename... Args>
    void trace(evmvc::string_view fmt, const Args&... args) const
    {
        if(_log) _log->trace(fmt.data(), args...);
    }
    
    template <typename... Args>
    void debug(evmvc::string_view fmt, const Args&... args) const
    {
        if(_log) _log->debug(fmt.data(), args...);
    }
    
    template <typename... Args>
    void info(evmvc::string_view fmt, const Args&... args) const
    {
        if(_log) _log->info(fmt.data(), args...);
    }
    
    template <typename... Args>
    void warn(evmvc::string_view fmt, const Args&... args) const
    {
        if(_log) _log->warn(fmt.data(), args...);
    }
    
    template <typename... Args>
    void error(evmvc::string_view fmt, const Args&... args) const
    {
        if(_log) _log->error(fmt.data(), args...);
    }
    
    template <typename... Args>
    void critical(evmvc::string_view fmt, const Args&... args) const
    {
        if(_log) _log->critical(fmt.data(), args...);
    }

protected:
    
    uint64_t _id;
    evmvc::sp_app _app;
    std::shared_ptr<spdlog::logger> _log;
    evhtp_request_t* _ev_req;
    sp_http_cookies _cookies;
    //param_map _rt_params;
    evmvc::http_params _rt_params;
    
};



} //ns evmvc
#endif //_libevmvc_request_h
