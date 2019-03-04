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
#include "url.h"
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
    friend class request;
    friend class response;
    
    typedef std::map<
        md::string_view, md::string_view, md::string_view_cmp
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
        options()
            : expires(), max_age(-1),
            domain(""), path(""),
            secure(false), http_only(true),
            site(same_site::none), enc(encoding::base64)
        {
        }
        
        options(const options& o)
            : expires(o.expires), max_age(o.max_age),
            domain(o.domain), path(o.path),
            secure(o.secure), http_only(o.http_only),
            site(o.site), enc(o.enc)
        {
        }
        
        boost::optional<date::sys_seconds> expires;
        
        int max_age;
        md::string_view domain;
        md::string_view path;
        bool secure;
        bool http_only;
        same_site site;
        encoding enc;
    };
    
    
    http_cookies() = delete;
    http_cookies(
        uint64_t id,
        md::log::sp_logger log,
        const evmvc::sp_route& rt,
        const url& uri,
        sp_header_map hdrs
        )
        : _id(id),
        _log(log->add_child(
            "cookies-" + md::num_to_str(id, false) + uri.path()
        )),
        _rt(rt),
        _in_hdrs(hdrs),
        _out_hdrs(std::make_shared<header_map>()),
        _init(false), _cookies(), _locked(false)
    {
        EVMVC_DEF_TRACE("cookies {} {:p} created", _id, (void*)this);
    }
    
    ~http_cookies()
    {
        EVMVC_DEF_TRACE("cookies {} {:p} released", _id, (void*)this);
    }
    
    uint64_t id() const { return _id;}
    evmvc::sp_route get_route()const { return _rt;}
    
    bool exists(md::string_view name) const
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
        md::string_view name,
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
        md::string_view name,
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
        md::string_view name,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return md::str_to_num<CookieType>(
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
        md::string_view name,
        const CookieType& def_val,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        if(!exists(name))
            return def_val;
        return md::str_to_num<CookieType>(
            _get_raw(name, enc)
        );
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_convertible<CookieType, md::string_view>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        md::string_view name,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return _get_raw(name, enc);
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_convertible<CookieType, md::string_view>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        md::string_view name,
        const CookieType& def_val,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return exists(name) ? _get_raw(name, enc) : def_val;
    }
    
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<bool, CookieType>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        md::string_view name,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return md::to_bool(_get_raw(name, enc));
    }

    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<bool, CookieType>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        md::string_view name,
        const CookieType& def_val,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return exists(name) ? md::to_bool(_get_raw(name, enc)) : def_val;
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<CookieType, evmvc::json>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        md::string_view name,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return evmvc::json::parse(_get_raw(name, enc));
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<CookieType, evmvc::json>::value
        , int32_t>::type = -1
    >
    inline CookieType get(
        md::string_view name,
        const CookieType& def_val,
        http_cookies::encoding enc = http_cookies::encoding::base64) const
    {
        return exists(name) ? evmvc::json::parse(_get_raw(name, enc)) : def_val;
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
        md::string_view name,
        const CookieType& val,
        const http_cookies::options& opts = {})
    {
        _set(name, md::num_to_str(val, false), opts);
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_convertible<CookieType, md::string_view>::value
        , int32_t>::type = -1
    >
    inline void set(
        md::string_view name,
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
        md::string_view name,
        const CookieType& val,
        const http_cookies::options& opts = {})
    {
        _set(name, evmvc::to_string(val), opts);
    }
    
    template<
        typename CookieType,
        typename std::enable_if<
            std::is_same<CookieType, evmvc::json>::value
        , int32_t>::type = -1
    >
    inline void set(
        md::string_view name,
        const CookieType& val,
        const http_cookies::options& opts = {})
    {
        _set(name, val.dump(), opts);
    }
    
    inline void clear(
        md::string_view name,
        const http_cookies::options& opts = {})
    {
        http_cookies::options clear_opts = opts;
        clear_opts.max_age = 0;
        _set(name, "", clear_opts);
    }
    
private:
    
    inline void _set(
        md::string_view name,
        md::string_view val,
        const http_cookies::options& opts)
    {
        std::string cv = name.to_string() + "=";
        md::string_view enc_v = 
            (opts.enc == encoding::base64 ?
                md::base64_encode(val) : 
                val);
        cv += std::string(enc_v.data(), enc_v.size()) + "; ";
        
        if(opts.max_age < 0 && opts.expires){
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
        
        auto it = _out_hdrs->find("Set-Cookie");
        if(it != _out_hdrs->end())
            it->second.emplace_back(cv);
        else
            _out_hdrs->emplace(std::make_pair(
                "Set-Cookie",
                std::vector<std::string>{cv}
            ));
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
        
        auto header = _in_hdrs->find(to_string(evmvc::field::cookie).data());
        if(header == _in_hdrs->end())
            return;
        
        std::string hv = header->second.front();
        size_t hvl = hv.size();
        
        md::string_view svk;
        md::string_view svv;
        
        ssize_t ks = 0;
        ssize_t vs = 0;
        for(size_t i = 0; i < hvl; ++i)
            if(hv[i] == '='){
                if(svk.size() > 0)
                    _cookies.emplace(svk, svv);
                
                // trim key start space
                while((size_t)ks <= i && hv[ks] == ' ')
                    ++ks;
                if((size_t)ks == i){
                    _log->warn(
                        "Invalid cookie value: '{0}'", hv
                    );
                    return;
                }
                
                svk = md::string_view(hv.c_str() + ks, i - ks);
                if(i == hvl -1){
                    _log->warn(
                        "Invalid cookie value: '{0}'", hv
                    );
                    return;
                }
                
                vs = i + 1;
            }else if(hv[i] == ';'){
                svv = md::string_view(hv.c_str() + vs, i - vs);
                if(i == hvl -1){
                    ks = -1;
                    break;
                }
                ks = i + 1;
            }
        
        if(ks > -1 && svk.size() > 0){
            svv = md::string_view(hv.c_str() + vs, hvl - vs);
            _cookies.emplace(svk, svv);
        }
    }
    
    inline std::string _get_raw(
        md::string_view name, http_cookies::encoding enc) const
    {
        _init_get();
        const auto it = _cookies.find(name);
        if(it == _cookies.end())
            throw MD_ERR(
                "Cookie '{0}' was not found!", name.data()
            );
        
        return
            enc == http_cookies::encoding::base64 ?
                md::base64_decode(
                    std::string(it->second.data(), it->second.size())
                ) :
                std::string(it->second.data(), it->second.size());
    }
    
    inline void _lock()
    {
        _locked = true;
    }
    
    uint64_t _id;
    md::log::sp_logger _log;
    evmvc::sp_route _rt;
    sp_header_map _in_hdrs;
    sp_header_map _out_hdrs;
    mutable bool _init;
    mutable cookie_map _cookies;
    bool _locked;
};

} //ns evmvc
#endif //_libevmvc_cookies_h
