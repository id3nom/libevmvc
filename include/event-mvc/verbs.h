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

#ifndef _libevmvc_verbs_h
#define _libevmvc_verbs_h

#include "stable_headers.h"

extern "C" {
#define EVHTP_DISABLE_REGEX
#include <event2/http.h>
#include <evhtp/evhtp.h>
}

namespace evmvc {

enum class verb
    : unsigned int
{
    get = htp_method_GET,
    head = htp_method_HEAD,
    post = htp_method_POST,
    put = htp_method_PUT,
    del = htp_method_DELETE,
    options = htp_method_OPTIONS,
    trace = htp_method_TRACE,
    connect = htp_method_CONNECT,
    patch = htp_method_PATCH,
    
    unknown = htp_method_UNKNOWN
};

evmvc::string_view verb_to_string(evmvc::verb v)
{
    switch(v){
        case evmvc::verb::get:
            return "GET";
        case evmvc::verb::head:
            return "HEAD";
        case evmvc::verb::post:
            return "POST";
        case evmvc::verb::put:
            return "PUT";
        case evmvc::verb::del:
            return "DELETE";
        case evmvc::verb::options:
            return "OPTIONS";
        case evmvc::verb::trace:
            return "TRACE";
        case evmvc::verb::connect:
            return "CONNECT";
        case evmvc::verb::patch:
            return "PATCH";
        default:
            return "UNKNOWN";
    }
}

} //ns evmvc
#endif //_libevmvc_verbs_h
