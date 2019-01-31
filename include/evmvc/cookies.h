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

#ifndef _libevmvc_cookies_h
#define _libevmvc_cookies_h

#include "stable_headers.h"
#include "utils.h"
#include "fields.h"

#include "date/date.h"

#include <unordered_map>
#include <boost/optional.hpp>

namespace evmvc {

class request;
class response;

class http_cookies;
typedef std::shared_ptr<http_cookies> sp_http_cookies;

class http_cookies
{
    friend void _miscs::on_app_request(evhtp_request_t* req, void* arg);
    friend class request;
    friend class response;
    
    typedef std::map<
        evmvc::string_view, evmvc::string_view, evmvc::string_view_cmp
        > cookie_map;
    
public:
    enum class same_site
    {
        none,
        strict,
        lax
    };
    
    enum class encoding
    {
        clear = 0,
        base64 = 1,
    };
    
    class options
    {
    public:
        options() = default;
        
        // date::sys_seconds expires =
        //     date::sys_days{date::year(1969)/31/12} +
        //     std::chrono::hours{23} + std::chrono::minutes{59} +
        //     std::chrono::seconds{59};
        boost::optional<date::sys_seconds> expires = {};
        
        int max_age = -1;
        evmvc::string_view domain = "";
        evmvc::string_view path = "";
        bool secure = false;
        bool http_only = true;
        same_site site = same_site::none;
        encoding enc = encoding::base64;
    };
    
    
    http_cookies() = delete;
    http_cookies(evhtp_request_t* ev_req)
        : _ev_req(ev_req), _init(false), _cookies(), _locked(false)
    {
    }
    
    bool exists(evmvc::string_view name) const
    {
        _init_get();
        auto it = _cookies.find(name);
        return it != _cookies.end();
    }
    
    template<typename CookieType,
        typename std::enable_if<
            !(std::is_same<int16_t, CookieType>::value ||
            std::is_same<int32_t, CookieType>::value ||
            std::is_same<int64_t, CookieType>::value ||

            std::is_same<float, CookieType>::value ||
            std::is_same<double, CookieType>::value)
        , int32_t>::type = -1
    >
    inline CookieType get(
        evmvc::string_view name,
        http_cookies::encoding /*enc = http_cookies::encoding::base64*/) const
    {
        std::stringstream ss;
        ss << "Parsing from cookie '" << name.data()
            << "' value to " << typeid(CookieType).name()
            << " is not supported";
        throw std::runtime_error(ss.str().c_str());
    }
    
    template<typename CookieType,
        typename std::enable_if<
            !(std::is_same<int16_t, CookieType>::value ||
            std::is_same<int32_t, CookieType>::value ||
            std::is_same<int64_t, CookieType>::value ||

            std::is_same<float, CookieType>::value ||
            std::is_same<double, CookieType>::value)
        , int32_t>::type = -1
    >
    inline CookieType get(
        evmvc::string_view name,
        const CookieType& def_val,
        http_cookies::encoding /*enc = http_cookies::encoding::clear*/) const
    {
        std::stringstream ss;
        ss << "Parsing from cookie '" << name.data()
            << "' value to " << typeid(CookieType).name()
            << " is not supported";
        throw std::runtime_error(ss.str().c_str());
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<int16_t, CookieType>::value ||
            std::is_same<int32_t, CookieType>::value ||
            std::is_same<int64_t, CookieType>::value ||

            std::is_same<float, CookieType>::value ||
            std::is_same<double, CookieType>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        evmvc::string_view name,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return evmvc::str_to_num<CookieType>(
            _get_raw(name, enc)
        );
    }

    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<int16_t, CookieType>::value ||
            std::is_same<int32_t, CookieType>::value ||
            std::is_same<int64_t, CookieType>::value ||

            std::is_same<float, CookieType>::value ||
            std::is_same<double, CookieType>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        evmvc::string_view name,
        const CookieType& def_val,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        if(!exists(name))
            return def_val;
        return evmvc::str_to_num<CookieType>(
            _get_raw(name, enc)
        );
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<std::string, CookieType>::value ||
            std::is_same<evmvc::string_view, CookieType>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        evmvc::string_view name,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return _get_raw(name, enc).to_string();
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_convertible<CookieType, evmvc::string_view>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        evmvc::string_view name,
        const CookieType& def_val,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return exists(name) ? _get_raw(name, enc).to_string() : def_val;
    }
    
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<bool, CookieType>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        evmvc::string_view name,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return to_bool(_get_raw(name, enc));
    }

    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<bool, CookieType>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        evmvc::string_view name,
        const CookieType& def_val,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return exists(name) ? to_bool(_get_raw(name, enc)) : def_val;
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<int16_t, CookieType>::value ||
            std::is_same<int32_t, CookieType>::value ||
            std::is_same<int64_t, CookieType>::value ||

            std::is_same<float, CookieType>::value ||
            std::is_same<double, CookieType>::value
        , int32_t>::type = -1
    >
    inline void set(
        evmvc::string_view name,
        const CookieType& val,
        const http_cookies::options& opts = {})
    {
        _set(name, evmvc::num_to_str(val, false), opts);
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_convertible<CookieType, evmvc::string_view>::value
        , int32_t>::type = -1
    >
    inline void set(
        evmvc::string_view name,
        const CookieType& val,
        const http_cookies::options& opts = {})
    {
        _set(name, val, opts);
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<bool, CookieType>::value
        , int32_t>::type = -1
    >
    inline void set(
        evmvc::string_view name,
        const CookieType& val,
        const http_cookies::options& opts = {})
    {
        _set(name, evmvc::to_string(val), opts);
    }
    
private:
    
    inline void _set(
        evmvc::string_view name,
        evmvc::string_view val,
        const http_cookies::options& opts)
    {
        std::string cv = name.to_string() + "=";
        evmvc::string_view enc_v = 
            (opts.enc == encoding::base64 ?
                evmvc::base64_encode(val) : 
                val);
        cv += std::string(enc_v.data(), enc_v.size()) + "; ";
        
        if(opts.expires){
            auto daypoint = date::floor<date::days>(*opts.expires);
            auto ymd = date::year_month_day(daypoint);
            auto tod = date::make_time(*opts.expires - daypoint);
            cv += fmt::format(
                "Expires={0}, {1:=02d} {2:=02d} {3:=04d} "
                "{4:=02d}:{5:=02d}:{6:=02d} GMT; ",
                date::format("%a", date::weekday(ymd)),
                (unsigned int)ymd.day(),
                (unsigned int)ymd.month(),
                (int)ymd.year(),
                tod.hours().count(),
                tod.minutes().count(),
                tod.seconds().count()
            );
        }
        
        if(opts.max_age > -1)
            cv += fmt::format("Max-Age={0}; ", opts.max_age);
        if(opts.domain.size() > 0)
            cv += fmt::format("Domain={0}; ", opts.domain.data());
        if(opts.path.size() > 0)
            cv += fmt::format("Path={0}; ", opts.path.data());
        if(opts.secure)
            cv += "Secure; ";
        if(opts.http_only)
            cv += "HttpOnly; ";
        if(opts.site != same_site::none)
            cv += fmt::format(
                "SameSite={0}; ",
                opts.site == same_site::strict ? "Strict" : "Lax"
            );
        
        evhtp_kv_t* header =
            evhtp_header_new("Set-Cookie", cv.c_str(), 1, 1);
        evhtp_headers_add_header(_ev_req->headers_out, header);
    }
    
    inline void _init_get() const
    {
        if(_init)
            return;
        _init_get_data();
    }
    
    void _init_get_data() const
    {
        if(_init)
            return;
        _init = true;
        
        // fetch cookie header
        evhtp_kv_t* header = evhtp_headers_find_header(
            _ev_req->headers_in, to_string(evmvc::field::cookie).data()
        );
        if(header == nullptr)
            return;
        
        evmvc::string_view svk;
        evmvc::string_view svv;
        
        size_t ks = 0;
        size_t vs = 0;
        for(size_t i = 0; i < header->vlen; ++i)
            if(header->val[i] == '='){
                if(svk.size() > 0)
                    _cookies.emplace(svk, svv);
                
                svk = evmvc::string_view(header->val + ks, i - ks);
                vs = i + 1;
            }else if(header->val[i] == ';'){
                svv = evmvc::string_view(header->val + vs, i - vs);
            }
        
        if(svk.size() > 0){
            svv = evmvc::string_view(header->val + vs, header->vlen - vs);
            _cookies.emplace(svk, svv);
        }
    }
    
    inline evmvc::string_view _get_raw(
        evmvc::string_view name, http_cookies::encoding enc) const
    {
        _init_get();
        const auto it = _cookies.find(name);
        if(it == _cookies.end())
            throw std::runtime_error(
                fmt::format("Cookie '{0}' was not found!", name.data())
            );
        
        return enc == http_cookies::encoding::base64 ?
            evmvc::base64_decode(it->second) : it->second;
    }
    
    inline void _lock()
    {
        _locked = true;
    }
    
    evhtp_request_t* _ev_req;
    mutable bool _init;
    mutable cookie_map _cookies;
    bool _locked;
};



// template<>
// inline std::string evmvc::http_cookies::get<std::string, -1>(
//     evmvc::string_view name,
//     http_cookies::encoding enc) const
// {
//     return _get_raw(name, enc).to_string();
// }
//
// template<>
// inline std::string evmvc::http_cookies::get<std::string, -1>(
//     evmvc::string_view name,
//     const std::string& def_val,
//     http_cookies::encoding enc) const
// {
//     return exists(name) ? _get_raw(name, enc).to_string() : def_val;
// }
//
// template<>
// inline bool evmvc::http_cookies::get<bool, -1>(
//     evmvc::string_view name,
//     http_cookies::encoding enc) const
// {
//     return to_bool(_get_raw(name, enc));
// }
//
// template<>
// inline bool evmvc::http_cookies::get<bool, -1>(
//     evmvc::string_view name,
//     const bool& def_val,
//     http_cookies::encoding enc) const
// {
//     return exists(name) ? to_bool(_get_raw(name, enc)) : def_val;
// }


} //ns evmvc
#endif //_libevmvc_cookies_h
