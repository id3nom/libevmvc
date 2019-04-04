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

#ifndef _libevmvc_view_base_h
#define _libevmvc_view_base_h

#include "stable_headers.h"
#include "response.h"

namespace evmvc {

class view_engine;
typedef std::shared_ptr<view_engine> sp_view_engine;

class view_base
    : public std::enable_shared_from_this<view_base>
{
public:
    view_base(
        sp_view_engine engine,
        const evmvc::sp_response& _res)
        : _engine(engine),
        res(_res),
        req(_res->req())
    {
    }
    
    sp_view_engine engine() const { return _engine;}
    
    virtual md::string_view ns() const = 0;
    virtual md::string_view path() const = 0;
    virtual md::string_view name() const = 0;
    virtual md::string_view abs_path() const = 0;
    virtual md::string_view layout() const = 0;
    
    virtual void render(
        std::shared_ptr<view_base> self,
        md::callback::async_cb cb
    ) = 0;
    
    void write_enc(md::string_view data);
    void write_raw(md::string_view data);
    //void render_partial(md::string_view path, md::callback::async_cb cb);
    void render_view(md::string_view path, md::callback::async_cb cb);
    
private:
    sp_view_engine _engine;
    
protected:
    evmvc::sp_response res;
    evmvc::sp_request req;
};

};
#endif //_libevmvc_view_base_h