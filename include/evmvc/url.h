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

#ifndef _libevmvc_url_h
#define _libevmvc_url_h

#include "stable_headers.h"
#include "logging.h"

namespace evmvc {

enum class url_scheme
{
    http,
    https,
    unknown,
};

evmvc::string_view to_string(evmvc::url_scheme p)
{
    switch(p){
        case evmvc::url_scheme::http:
            return "http";
        case evmvc::url_scheme::https:
            return "https";
        default:
            throw EVMVC_ERR("UNKNOWN url_scheme: '{}'", (int)p);
    }
}


#define EVMVC_URL_PARSE_SCHEME      0
#define EVMVC_URL_PARSE_AUTH        1
#define EVMVC_URL_PARSE_USERINFO    2
#define EVMVC_URL_PARSE_HOST        3
#define EVMVC_URL_PARSE_PORT        4
#define EVMVC_URL_PARSE_PATH        5
#define EVMVC_URL_PARSE_QUERY       6
#define EVMVC_URL_PARSE_FRAG        7

class url
{
public:
    url()
        : _empty(true)
    {
    }
    
    url(evmvc::string_view u)
        : _empty(u.size() == 0)
    {
        _parse(u);
    }
    
    url(const evmvc::string_view sbase, evmvc::string_view u)
        : _empty(sbase.size() == 0 && u.size() == 0)
    {
        url base(sbase);
        _parse(u);
        bool is_abs = is_absolute();

        _scheme = base._scheme;
        _scheme_string = base._scheme_string;
        
        _userinfo = base._userinfo;
        _username = base._username;
        _password = base._password;
        
        _hostname = base._hostname;
        _port = base._port;
        _port_string = base._port_string;
        
        if(_path.size() > 0 && !is_abs)
            _path = base._path + _path;
    }
    
    url(const url& base, evmvc::string_view u)
        : _empty(base.empty() && u.size() == 0)
    {
        _parse(u);
        bool is_abs = is_absolute();

        _scheme = base._scheme;
        _scheme_string = base._scheme_string;
        
        _userinfo = base._userinfo;
        _username = base._username;
        _password = base._password;
        
        _hostname = base._hostname;
        _port = base._port;
        _port_string = base._port_string;
        
        if(_path.size() > 0 && !is_abs)
            _path = base._path + _path;
    }
    
    url(const url& o)
        : _empty(o._empty),
        _scheme_string(o._scheme_string),
        _userinfo(o._userinfo),
        _username(o._username),
        _password(o._password),
        _hostname(o._hostname),
        _port(o._port),
        _port_string(o._port_string),
        _path(o._path),
        _query(o._query),
        _fragment(o._fragment)
    {
    }
    
    operator std::string() const
    {
        return to_string();
    }
    
    bool empty() const { return _empty;}
    bool is_absolute() const { return _hostname.size() > 0;}
    bool is_relative() const { return _hostname.size() == 0;}
    
    url_scheme scheme() const
    {
        if(!is_absolute())
            return url_scheme::unknown;
        return _scheme;
    }
    std::string scheme_string() const
    {
        return _scheme_string;
    }
    
    bool has_userinfo() const { return _userinfo;}
    std::string username() const { return _username;}
    std::string password() const { return _password;}
    void set_userinfo(const std::string& user, const std::string& pass)
    {
        _userinfo = true;
        _username = user;
        _password = pass;
    }
    void clear_userinfo()
    {
        _userinfo = false;
        _username = "";
        _password = "";
    }
    
    std::string userinfo() const
    {
        std::string r;
        if(_userinfo){
            r += _username;
            if(_password.size() > 0)
                r += ":" + _password;
        }
        return r;
    }
    
    std::string host() const
    {
        std::string r;
        if(_hostname.size() > 0){
            r += _hostname;
            if(_port_string.size() > 0)
                r += ":";
        }
        
        if(_port_string.size() > 0)
            r += _port_string;
        return r;
    }
    
    std::string authority() const
    {
        std::string r;
        if(_userinfo || _hostname.size() > 0 || _port_string.size() > 0){
            if(_userinfo){
                r += _username;
                if(_password.size() > 0)
                    r += ":" + _password;
                    
                if(_hostname.size() > 0 || _port_string.size() > 0)
                    r += "@";
            }
            
            if(_hostname.size() > 0){
                r += _hostname;
                if(_port_string.size() > 0)
                    r += ":";
            }
            
            if(_port_string.size() > 0)
                r += _port_string;
        }
        return r;
    }
    
    std::string hostname() const { return _hostname;}
    uint16_t port() const { return _port;}
    std::string port_string() const { return _port_string;}
    
    std::string path() const { return is_absolute() ? "/" + _path : _path;}
    std::string query() const { return _query;}
    std::string fragment() const { return _fragment;}
    
    std::string to_string() const
    {
        std::string surl;
        if(_scheme_string.size())
            surl += _scheme_string + ":";
        if(_userinfo || _hostname.size() > 0 || _port_string.size() > 0){
            surl += "//";
            if(_userinfo){
                surl += _username;
                if(_password.size() > 0)
                    surl += ":" + _password;
                    
                if(_hostname.size() > 0 || _port_string.size() > 0)
                    surl += "@";
            }
            
            if(_hostname.size() > 0){
                surl += _hostname;
                if(_port_string.size() > 0)
                    surl += ":";
            }
            
            if(_port_string.size() > 0)
                surl += _port_string;
            
            if(_path.size() > 0)
                surl += "/";
        }
        
        if(_path.size() > 0)
            surl += _path;
        
        if(_query.size() > 0)
            surl += "?" + _query;
        
        if(_fragment.size() > 0)
            surl += "?" + _fragment;
        
        return surl;
    }
    
private:
    void _parse(evmvc::string_view u)
    {
        if(u.size() == 0)
            return;
        
        const char* data = u.data();
        ssize_t len = u.size();
        
        int section = EVMVC_URL_PARSE_SCHEME;
        if(data[0] == '/' && len > 1 && data[1] == '/')
            section = EVMVC_URL_PARSE_AUTH;
        else if(data[0] == '/')
            section = EVMVC_URL_PARSE_PATH;
        else if(data[0] == '?')
            section = EVMVC_URL_PARSE_QUERY;
        else if(data[0] == '#')
            section = EVMVC_URL_PARSE_FRAG;
        
        int sub_sec = -1;
        
        size_t spos = 0;
        while(spos < (size_t)len){
            switch(section){
                case EVMVC_URL_PARSE_SCHEME:{
                    ssize_t idx1 = evmvc::find_ch(data, len, ':', spos);
                    // ssize_t idx2 = evmvc::find_ch(data, len, '/', idx1+1);
                    // ssize_t idx3 = evmvc::find_ch(data, len, '/', idx2+2);
                    
                    // if(idx1 > -1 && idx1 != idx2-1 && idx2 != idx3-1)
                    if(idx1 == -1)
                        throw EVMVC_ERR("Invalid uri: '{}'", u);
                    
                    _scheme_string = data_substring(data, spos, idx1);
                    boost::to_lower(_scheme_string);
                    
                    if(_scheme_string == "http")
                        _scheme = url_scheme::http;
                    else if(_scheme_string == "https")
                        _scheme = url_scheme::https;
                    else
                        _scheme = url_scheme::unknown;
                    
                    ssize_t idx2 = evmvc::find_ch(data, len, '/', idx1+1);
                    if(idx2 == idx1 +1){
                        ssize_t idx3 = evmvc::find_ch(data, len, '/', idx2+1);
                        if(idx3 == idx2 +1)
                            section = EVMVC_URL_PARSE_AUTH;
                        else
                            section = EVMVC_URL_PARSE_PATH;
                    }else if(len > idx1 +1){
                        if(data[idx1 +1] == '?')
                            section = EVMVC_URL_PARSE_QUERY;
                        else if(data[idx1 +1] == '#')
                            section = EVMVC_URL_PARSE_FRAG;
                        else
                            section = EVMVC_URL_PARSE_PATH;
                    }
                    
                    //spos += idx3 +1;
                    spos += idx1 +1;
                    break;
                }
                case EVMVC_URL_PARSE_AUTH:{
                    if(sub_sec == -1){
                        ssize_t idx = evmvc::find_ch(data, len, '@', spos);
                        if(idx > -1){
                            _userinfo = true;
                            sub_sec = EVMVC_URL_PARSE_USERINFO;
                        }else{
                            _userinfo = false;
                            sub_sec = EVMVC_URL_PARSE_HOST;
                        }
                    }
                    
                    switch(sub_sec){
                        case EVMVC_URL_PARSE_USERINFO:{
                            ssize_t at_idx = evmvc::find_ch(
                                data, len, '@', spos
                            );
                            ssize_t idx = evmvc::find_ch(data, len, ':', spos);
                            if(idx > 1 && idx < at_idx){
                                _username = data_substring(data, spos, idx);
                                _password = data_substring(data, idx+1, at_idx);
                            }else{
                                _username = data_substring(data, spos, at_idx);
                                _password = "";
                            }
                            
                            sub_sec = EVMVC_URL_PARSE_HOST;
                            spos += at_idx +1;
                            break;
                        }
                        case EVMVC_URL_PARSE_HOST:{
                            ssize_t pidx = evmvc::find_ch(
                                data, len, ':', spos
                            );
                            if(pidx > -1){
                                _hostname = data_substring(data, spos, pidx);
                                spos += pidx +1;
                                sub_sec = EVMVC_URL_PARSE_PORT;
                                break;
                            }
                            
                            pidx = evmvc::find_ch(
                                data, len, '/', spos
                            );
                            ssize_t qidx = evmvc::find_ch(
                                data, len, '?', spos
                            );
                            ssize_t fidx = evmvc::find_ch(
                                data, len, '#', spos
                            );
                            
                            if(fidx > -1 && 
                                (qidx == -1 || qidx > fidx) &&
                                (pidx == -1 || pidx > fidx)
                            ){
                                _hostname = data_substring(data, spos, fidx);
                                spos += fidx +1;
                                section = EVMVC_URL_PARSE_FRAG;
                                break;
                            }
                            if(qidx > -1 &&
                                (pidx == -1 || pidx > qidx)
                            ){
                                _hostname = data_substring(data, spos, qidx);
                                spos += qidx +1;
                                section = EVMVC_URL_PARSE_QUERY;
                                break;
                            }else if(pidx > -1){
                                _hostname = data_substring(data, spos, pidx);
                                spos += pidx +1;
                                section = EVMVC_URL_PARSE_PATH;
                                break;
                            }
                            _hostname = data_substring(data, spos, len);
                            spos += _hostname.size();
                            break;
                        }
                        case EVMVC_URL_PARSE_PORT:{
                            ssize_t pidx = evmvc::find_ch(
                                data, len, '/', spos
                            );
                            ssize_t qidx = evmvc::find_ch(
                                data, len, '?', spos
                            );
                            ssize_t fidx = evmvc::find_ch(
                                data, len, '#', spos
                            );
                            
                            if(fidx > -1 && 
                                (qidx == -1 || qidx > fidx) &&
                                (pidx == -1 || pidx > fidx)
                            ){
                                _port_string = data_substring(
                                    data, spos, fidx
                                );
                                _port = evmvc::str_to_num<uint16_t>(
                                    _port_string
                                );
                                spos += fidx +1;
                                section = EVMVC_URL_PARSE_FRAG;
                                break;
                            }
                            if(qidx > -1 &&
                                (pidx == -1 || pidx > qidx)
                            ){
                                _port_string = data_substring(
                                    data, spos, qidx
                                );
                                _port = evmvc::str_to_num<uint16_t>(
                                    _port_string
                                );
                                spos += qidx +1;
                                section = EVMVC_URL_PARSE_QUERY;
                                break;
                            }
                            if(pidx > -1){
                                _port_string = data_substring(
                                    data, spos, pidx
                                );
                                _port = evmvc::str_to_num<uint16_t>(
                                    _port_string
                                );
                                spos += pidx +1;
                                section = EVMVC_URL_PARSE_PATH;
                                break;
                            }
                            
                            {
                                _port_string = data_substring(
                                    data, spos, len
                                );
                                _port = evmvc::str_to_num<uint16_t>(
                                    _port_string
                                );
                                spos += _port_string.size();
                                break;
                            }
                        }
                        default:
                            throw EVMVC_ERR(
                                "Invalid parsing sub section: '{}'", sub_sec
                            );
                    }
                    
                    break;
                }
                case EVMVC_URL_PARSE_PATH:{
                    ssize_t qidx = evmvc::find_ch(
                        data, len, '?', spos
                    );
                    ssize_t fidx = evmvc::find_ch(
                        data, len, '#', spos
                    );
                    
                    if(fidx > -1 && 
                        (qidx == -1 || qidx > fidx)
                    ){
                        _path = data_substring(
                            data, spos, fidx
                        );
                        spos += fidx +1;
                        section = EVMVC_URL_PARSE_FRAG;
                        break;
                    }
                    if(qidx > -1){
                        _path = data_substring(
                            data, spos, qidx
                        );
                        spos += qidx +1;
                        section = EVMVC_URL_PARSE_QUERY;
                        break;
                    }
                    
                    {
                        _path = data_substring(
                            data, spos, len
                        );
                        spos += _path.size();
                    }
                    
                    break;
                }
                case EVMVC_URL_PARSE_QUERY:{
                    ssize_t fidx = evmvc::find_ch(
                        data, len, '#', spos
                    );
                    
                    if(fidx > -1){
                        _query = data_substring(
                            data, spos, fidx
                        );
                        spos += fidx +1;
                        section = EVMVC_URL_PARSE_FRAG;
                        break;
                    }
                    {
                        _query = data_substring(
                            data, spos, len
                        );
                        spos += _query.size();
                    }
                    
                    break;
                }
                case EVMVC_URL_PARSE_FRAG:{
                    _fragment = data_substring(
                        data, spos, len
                    );
                    spos += _fragment.size();
                    break;
                }
                default:
                    throw EVMVC_ERR("Invalid parsing section: '{}'", section);
            }
        }
    }
    
    bool _empty = false;
    
    std::string _scheme_string = "unknown";
    url_scheme _scheme = url_scheme::unknown;
    
    bool _userinfo = false;
    std::string _username;
    std::string _password;
    
    std::string _hostname;
    uint16_t _port = 0;
    std::string _port_string;
    
    std::string _path;
    std::string _query;
    std::string _fragment;
};

}//::evmvc
#endif//_libevmvc_url_h
