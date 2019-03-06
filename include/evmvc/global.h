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

#ifndef _libevmvc_global_h
#define _libevmvc_global_h

#include "stable_headers.h"

namespace evmvc { namespace global {

inline struct event_base* ev_base(
    struct event_base* reset = nullptr,
    bool free_on_reset = false,
    bool debug = false)
{
    static struct event_base* _ev_base = nullptr;
    if(reset || _ev_base == nullptr){
        if(_ev_base && free_on_reset)
            event_base_free(_ev_base);
        
        if(reset)
            _ev_base = reset;
        else{
            if(debug)
                event_enable_debug_mode();
            _ev_base = event_base_new();
        }
    }
    
    return _ev_base;
}

inline void set_event_base(struct event_base* evb)
{
    if(!evb)
        throw MD_ERR("event_base must be valid!");
    evmvc::global::ev_base(evb, true);
}

}}//::evmvc::global
#endif//_libevmvc_global_h
