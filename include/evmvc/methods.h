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

#ifndef _libevmvc_methods_h
#define _libevmvc_methods_h

#include "stable_headers.h"

namespace evmvc {

enum class method
    : unsigned int
{
    get,
    head,
    post,
    put,
    del,
    options,
    trace,
    connect,
    patch,
    
    unknown,
};

inline md::string_view to_string(evmvc::method v)
{
    switch(v){
        case evmvc::method::get:
            return "GET";
        case evmvc::method::head:
            return "HEAD";
        case evmvc::method::post:
            return "POST";
        case evmvc::method::put:
            return "PUT";
        case evmvc::method::del:
            return "DELETE";
        case evmvc::method::options:
            return "OPTIONS";
        case evmvc::method::trace:
            return "TRACE";
        case evmvc::method::connect:
            return "CONNECT";
        case evmvc::method::patch:
            return "PATCH";
        default:
            return "UNKNOWN";
    }
}

inline evmvc::method parse_method(md::string_view s)
{
    if(s.size() == 3){
        if(!strncasecmp("get", s.data(), 3))
            return evmvc::method::get;
        if(!strncasecmp("put", s.data(), 3))
            return evmvc::method::put;
    }else if(s.size() == 4){
        if(!strncasecmp("head", s.data(), 4))
            return evmvc::method::head;
        if(!strncasecmp("post", s.data(), 4))
            return evmvc::method::post;
    }else if(s.size() == 5){
        if(!strncasecmp("patch", s.data(), 5))
            return evmvc::method::patch;
        if(!strncasecmp("trace", s.data(), 5))
            return evmvc::method::trace;
    }else if(s.size() == 6){
        if(!strncasecmp("delete", s.data(), 6))
            return evmvc::method::del;
    }else if(s.size() == 7){
        if(!strncasecmp("options", s.data(), 7))
            return evmvc::method::options;
        if(!strncasecmp("connect", s.data(), 7))
            return evmvc::method::connect;
    }
    
    return evmvc::method::unknown;
}

inline evmvc::method parse_method_reversed(md::string_view s)
{
    if(s.size() == 0)
        return evmvc::method::unknown;
    
    const char* ch = s.data() + (s.size()-1);
    
    // GET
    if(*ch == 'G' || *ch == 'g'){
        if(s.size() >= 3 && !strncasecmp("teg", ch-2, 3))
            return evmvc::method::get;
        
    // POST, PUT, PATCH
    }else if(*ch == 'P' || *ch == 'p'){
        if(s.size() >= 4 && !strncasecmp("tsop", ch-3, 4))
            return evmvc::method::post;
        else if(s.size() >= 3 && !strncasecmp("tup", ch-2, 3))
            return evmvc::method::put;
        else if(s.size() >= 5 && !strncasecmp("hctap", ch-4, 5))
            return evmvc::method::patch;
        
    // CONNECT
    }else if(*ch == 'C' || *ch == 'c'){
        if(s.size() >= 7 && !strncasecmp("tcennoc", ch-6, 7))
            return evmvc::method::connect;
        
    // HEAD
    }else if(*ch == 'H' || *ch == 'h'){
        if(s.size() >= 4 && !strncasecmp("deah", ch-3, 4))
            return evmvc::method::head;
        
    // DELETE
    }else if(*ch == 'D' || *ch == 'd'){
        if(s.size() >= 7 && !strncasecmp("eteled", ch-6, 7))
            return evmvc::method::del;
        
    // TRACE
    }else if(*ch == 'T' || *ch == 't'){
        if(s.size() >= 5 && !strncasecmp("ecart", ch-4, 5))
            return evmvc::method::trace;
        
    // OPTIONS
    }else if(*ch == 'O' || *ch == 'o'){
        if(s.size() >= 7 && !strncasecmp("snoitpo", ch-6, 7))
            return evmvc::method::options;
        
    }
    
    return evmvc::method::unknown;
}


} //ns evmvc
#endif //_libevmvc_methods_h
