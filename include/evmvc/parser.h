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

#ifndef _libevmvc_http_parser_h
#define _libevmvc_http_parser_h

#include "stable_headers.h"
#include "logging.h"

namespace evmvc {


class parser
{
public:
    parser(wp_connection conn, const sp_logger log)
        : _conn(conn), _log(log)
    {
    }
    
    sp_connection get_connection() const { return _conn.lock();}
    sp_logger log() const { return _log;}
    
    virtual void reset() = 0;
    virtual size_t parse(const char* buf, size_t len, cb_error& ec) = 0;
    
private:
    wp_connection _conn;
    sp_logger _log;
};

class http_parser
    : public parser
{
public:
    http_parser(wp_connection conn, const sp_logger& log)
        : parser(conn, log->add_child("parser"))
    {
    }
    

    void reset()
    {
        _body_nread = 0;
        
    }
    
    size_t parse(const char* /*buf*/, size_t /*len*/, cb_error& /*ec*/)
    {
        return 0;
    }
    
private:
    size_t _body_nread = 0;
    
    
};


}//::evmvc

#endif//_libevmvc_http_parser_h