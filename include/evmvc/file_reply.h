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

#ifndef _libevmvc_file_reply_h
#define _libevmvc_file_reply_h

#include "stable_headers.h"
#include "utils.h"

namespace evmvc {

class file_reply {
public:
    file_reply(
        response _res,
        wp_connection _conn,
        FILE* _file_desc,
        md::callback::async_cb _cb,
        md::log::logger _log)
        :
        res(_res),
        conn(_conn),
        file_desc(_file_desc),
        buffer(evbuffer_new()),
        zs(nullptr),
        zs_size(0),
        cb(_cb),
        log(_log)
    {
        EVMVC_DEF_TRACE("file_reply {:p} created", (void*)this);
    }
    ~file_reply()
    {
        if(this->zs){
            deflateEnd(this->zs);
            free(this->zs);
            this->zs = nullptr;
        }
        
        fclose(this->file_desc);
        evbuffer_free(this->buffer);
        EVMVC_DEF_TRACE("file_reply {:p} released", (void*)this);
    }
    response res;
    wp_connection conn;
    FILE* file_desc;
    struct evbuffer* buffer;
    z_stream* zs;
    uLong zs_size;
    md::callback::async_cb cb;
    md::log::logger log;
};
typedef std::shared_ptr<file_reply> shared_file_reply;

}//::evmvc
#endif//_libevmvc_file_reply_h
