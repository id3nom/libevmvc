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

#ifndef _libevmvc_fanjet_view_base_h
#define _libevmvc_fanjet_view_base_h

#include "../stable_headers.h"
#include "../response.h"
#include "../view_base.h"

namespace evmvc { namespace fanjet {

class view_base
    : public evmvc::view_base
{
    friend class app;
public:
    
    view_base(
        sp_view_engine engine,
        const evmvc::response& _res)
        : evmvc::view_base(engine, _res)
    {
    }
    
    
    
    // void render(
    //     std::shared_ptr<view_base> self,
    //     md::callback::async_cb cb)
    // {
    //      
    // }
    
    
    
protected:
    
    
};

}};//evmvc::fanjet
#endif //_libevmvc_fanjet_view_base_h