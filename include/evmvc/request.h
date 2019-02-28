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
#include "url.h"
#include "headers.h"
#include "fields.h"
#include "methods.h"
#include "cookies.h"
#include "http_param.h"
#include "files.h"
#include "jwt.h"

namespace evmvc {


namespace policies {
class jwt_filter_rule_t;
}

class request
    : public std::enable_shared_from_this<request>
{
    friend class http_parser;
    friend class policies::jwt_filter_rule_t;
    // friend void _internal::on_multipart_request_completed(
    //     sp_request req,
    //     sp_response res,
    //     sp_app a
    // );
    
public:
    request(
        uint64_t id,
        http_version ver,
        wp_connection conn,
        evmvc::sp_logger log,
        const evmvc::sp_route& rt,
        url uri,
        evmvc::method met,
        evmvc::string_view smet,
        sp_header_map hdrs,
        const sp_http_cookies& http_cookies,
        const std::vector<std::shared_ptr<evmvc::http_param>>& p
        )
        : _id(id),
        _version(ver),
        _conn(conn),
        _log(log->add_child(
            "req-" + evmvc::num_to_str(id, false) +
            "/" + uri.to_string()
        )),
        _rt(rt),
        _uri(std::move(uri)),
        _met(met),
        _smet(smet.to_string()),
        _headers(std::make_shared<evmvc::request_headers>(hdrs)),
        _cookies(http_cookies),
        _rt_params(std::make_unique<http_params_t>(p)),
        _qry_params(std::make_unique<http_params_t>()),
        _body_params(std::make_unique<http_params_t>()),
        _files()
    {
        EVMVC_DEF_TRACE(
            "request '{}' created", this->id()
        );
        
        if(_log->should_log(log_level::trace)){
            std::string hdrs_dbg;
            for(auto& it : *hdrs.get())
                hdrs_dbg += fmt::format(
                    "{}: {}\n",
                    it.first,
                    evmvc::join(it.second, "; ")
                );
            EVMVC_TRACE(_log, hdrs_dbg);
        }
        
        // building the _qry_params object
        std::string qry = uri.query();
        if(qry.empty())
            return;
        
        ssize_t it_idx = 0;
        do{
            ssize_t nit_idx = evmvc::find_ch(
                qry.c_str(), qry.size(), '&', it_idx
            );
            
            if(nit_idx == -1)
                nit_idx = qry.size();
            
            ssize_t sep_idx = evmvc::find_ch(
                qry.c_str(), qry.size(), '=', it_idx
            );
            
            if(sep_idx > -1 && sep_idx < nit_idx){
                _qry_params->emplace_back(std::make_shared<evmvc::http_param>(
                    sep_idx == it_idx ? "" : 
                        evmvc::data_substring(qry.c_str(), it_idx, sep_idx),
                    evmvc::data_substring(qry.c_str(), sep_idx+1, nit_idx)
                ));
            }else{
                _qry_params->emplace_back(std::make_shared<evmvc::http_param>(
                    evmvc::data_substring(qry.c_str(), it_idx, nit_idx),
                    ""
                ));
            }
            
            it_idx = nit_idx +1;
        }while(it_idx > -1 && it_idx < (ssize_t)qry.size());
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
    const url& uri() const { return _uri;}
    
    std::string connection_ip() const;
    uint16_t connection_port() const;
    
    sp_connection connection() const;
    bool secure() const;
    
    std::string ip() const
    {
        //TODO: add trust proxy options
        sp_header h = _headers->get("X-Forwarded-For");
        if(!h)
            return connection_ip();
        
        std::string ips = evmvc::trim_copy(h->value());
        auto cp = ips.find(",");
        if(cp == std::string::npos)
            return ips;
        return ips.substr(0, cp);
    }
    
    std::vector<std::string> ips() const
    {
        //TODO: add trust proxy options
        sp_header h = _headers->get("X-Forwarded-For");
        if(!h)
            return {connection_ip()};
        
        std::string ips = evmvc::trim_copy(h->value());
        
        std::vector<std::string> vals;
        size_t start = 0;
        auto cp = ips.find(",");
        if(cp == std::string::npos)
            return {ips};
        while(cp != std::string::npos){
            vals.emplace_back(
                ips.substr(start, cp)
            );
            start = cp+1;
            cp = ips.find(",", start);
        }
        return vals;
    }
    
    std::string hostname() const
    {
        //TODO: add trust proxy options
        sp_header h = _headers->get("X-Forwarded-Host");
        if(!h)
            h = _headers->get("host");
        
        return h->value();
    }
    
    evmvc::url_scheme protocol() const;
    std::string protocol_string() const;
    
    evmvc::request_headers& headers() const { return *(_headers.get());}
    http_cookies& cookies() const { return *(_cookies.get());}
    http_files& files() const { return *(_files.get());}
    
    http_params_t& params() const { return *(_rt_params.get());}
    http_param& params(evmvc::string_view name) const
    {
        return *(_rt_params->get(name).get());
    }
    template<typename PARAM_T>
    PARAM_T params(evmvc::string_view name, PARAM_T def_val) const
    {
        auto p = _rt_params->get(name);
        if(p)
            return p->get<PARAM_T>();
        return def_val;
    }
    
    http_params_t& query() const { return *(_qry_params.get());}
    http_param& query(evmvc::string_view name) const
    {
        return *(_qry_params->get(name).get());
    }
    template<typename PARAM_T>
    PARAM_T query(evmvc::string_view name, PARAM_T def_val) const
    {
        auto p = _qry_params->get(name);
        if(p)
            return p->get<PARAM_T>();
        return def_val;
    }
    
    http_params_t& body() const { return *(_body_params.get());}
    http_param& body(evmvc::string_view name) const
    {
        return *(_body_params->get(name).get());
    }
    template<typename PARAM_T>
    PARAM_T body(evmvc::string_view name, PARAM_T def_val) const
    {
        auto p = _body_params->get(name);
        if(p)
            return p->get<PARAM_T>();
        return def_val;
    }
    
    evmvc::method method()
    {
        return _met;
    }
    std::string method_string()
    {
        return _smet;
    }
    
    bool xhr()
    {
        return this->headers().compare_value(
            "X-Requested-With", "XMLHttpRequest"
        );
    }
    
    const jwt::decoded_jwt& token() const
    {
        return _token;
    }

    
protected:

    void _load_multipart_params(
        std::shared_ptr<multip::multipart_subcontent> ms);
    
    uint64_t _id;
    http_version _version;
    wp_connection _conn;
    evmvc::sp_logger _log;
    evmvc::sp_route _rt;
    evmvc::url _uri;
    evmvc::method _met;
    std::string _smet;
    
    evmvc::sp_request_headers _headers;
    sp_http_cookies _cookies;
    
    evmvc::http_params _rt_params;
    evmvc::http_params _qry_params;
    evmvc::http_params _body_params;
    evmvc::sp_http_files _files;
    jwt::decoded_jwt _token;
};



} //ns evmvc
#endif //_libevmvc_request_h
