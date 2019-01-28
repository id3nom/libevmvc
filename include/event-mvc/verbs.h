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

#ifndef _libevent_mvc_verbs_h
#define _libevent_mvc_verbs_h

#include "stable_headers.h"

extern "C" {
#include <event2/http.h>
}

namespace event { namespace mvc {

enum class verb
{
    get = EVHTTP_REQ_GET,
    post = EVHTTP_REQ_POST,
    head = EVHTTP_REQ_HEAD,
    put = EVHTTP_REQ_PUT,
    del = EVHTTP_REQ_DELETE,
    options = EVHTTP_REQ_OPTIONS,
    trace = EVHTTP_REQ_TRACE,
    connect = EVHTTP_REQ_CONNECT,
    patch = EVHTTP_REQ_PATCH,
};

mvc::string_view verb_to_string(mvc::verb v)
{
    switch(v){
        case mvc::verb::get:
            return "GET";
        case mvc::verb::post:
            return "POST";
        case mvc::verb::head:
            return "HEAD";
        case mvc::verb::put:
            return "PUT";
        case mvc::verb::del:
            return "DELETE";
        case mvc::verb::options:
            return "OPTIONS";
        case mvc::verb::trace:
            return "TRACE";
        case mvc::verb::connect:
            return "CONNECT";
        case mvc::verb::patch:
            return "PATCH";
        default:
            return "UNKNOWN";
    }
}

}} // ns event::mvc
#endif //_libevent_mvc_verbs_h
