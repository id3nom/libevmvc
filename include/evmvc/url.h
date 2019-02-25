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
        : _valid(false)
    {
    }
    
    url(evmvc::string_view u)
    {
        _parse(u);
    }
    
    url(const url& base, evmvc::string_view u)
    {
        _parse(u);
        _scheme = base._scheme;
        _scheme_string = base._scheme_string;
        
        _userinfo = base._userinfo;
        _username = base._username;
        _password = base._password;
        
        _hostname = base._hostname;
        _port = base._port;
        
        if(_path.size() > 0 && !_absolute)
            _path = base._path + _path;
        
        _absolute = base._absolute;
    }
    
    url(const url& o)
        : _valid(o._valid),
        _absolute(o._absolute),
        _scheme_string(o._scheme_string),
        _userinfo(o._userinfo),
        _username(o._username),
        _password(o._password),
        _hostname(o._hostname),
        _port(o._port),
        _path(o._path),
        _query(o._query),
        _fragment(o._fragment)
    {
    }
    
    bool valid() const { return _valid;}
    bool is_absolute() const { return _absolute;}
    bool is_relative() const { return !_absolute;}
    
    url_scheme scheme() const
    {
        if(!_absolute)
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
    
    std::string hostname() const { return _hostname;}
    uint16_t port() const { return _port;}
    
    std::string path() const { return _path;}
    std::string query() const { return _query;}
    std::string fragment() const { return _fragment;}
    
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
                                std::string sport = data_substring(
                                    data, spos, fidx
                                );
                                _port = evmvc::str_to_num<uint16_t>(sport);
                                spos += fidx +1;
                                section = EVMVC_URL_PARSE_FRAG;
                                break;
                            }
                            if(qidx > -1 &&
                                (pidx == -1 || pidx > qidx)
                            ){
                                std::string sport = data_substring(
                                    data, spos, qidx
                                );
                                _port = evmvc::str_to_num<uint16_t>(sport);
                                spos += qidx +1;
                                section = EVMVC_URL_PARSE_QUERY;
                                break;
                            }
                            if(pidx > -1){
                                std::string sport = data_substring(
                                    data, spos, pidx
                                );
                                _port = evmvc::str_to_num<uint16_t>(sport);
                                spos += pidx +1;
                                section = EVMVC_URL_PARSE_PATH;
                                break;
                            }
                            
                            {
                                std::string sport = data_substring(
                                    data, spos, len
                                );
                                _port = evmvc::str_to_num<uint16_t>(sport);
                                spos += sport.size();
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
    
    bool _valid = false;
    bool _absolute = false;
    
    std::string _scheme_string = "unknown";
    url_scheme _scheme = url_scheme::unknown;
    
    bool _userinfo = false;
    std::string _username;
    std::string _password;
    
    std::string _hostname;
    uint16_t _port = 0;
    
    std::string _path;
    std::string _query;
    std::string _fragment;
};

#undef EVMVC_URL_PARSE_SCHEME
#undef EVMVC_URL_PARSE_AUTH
#undef EVMVC_URL_PARSE_USERINFO
#undef EVMVC_URL_PARSE_HOST
#undef EVMVC_URL_PARSE_PORT
#undef EVMVC_URL_PARSE_PATH
#undef EVMVC_URL_PARSE_QUERY
#undef EVMVC_URL_PARSE_FRAG


}//::evmvc
#endif//_libevmvc_url_h
