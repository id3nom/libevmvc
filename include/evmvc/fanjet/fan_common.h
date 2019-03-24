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

#ifndef _libevmvc_fanjet_common_h
#define _libevmvc_fanjet_common_h

#include "../stable_headers.h"

namespace evmvc { namespace fanjet {

namespace ast {
enum class section_type
    : int
{
    invalid             = INT_MIN,
    root                = INT_MIN -1,
    token               = INT_MIN -2,
    expr                = INT_MIN -3,
    string              = INT_MIN -4,
    
    dir_ns              = 1,            // @namespace ...
    dir_name            = (1 << 1),     // @name ...
    dir_layout          = (1 << 2),     // @layout ...
    dir_header          = (1 << 3),     // @header ... ;
    dir_inherits        = (1 << 4),     // @inherits ... ;
    
    literal             = (1 << 5),     // text <tag attr> ... </tag><tag/> 
    
    comment_line        = (1 << 6),     // @** ...
    comment_block       = (1 << 7),     // @* ... *@
    
    region_start        = (1 << 8),     // @region ...
    region_end          = (1 << 9),     // @endregion ...
    
    output_raw          = (1 << 10),    // @:: ... ;
    output_enc          = (1 << 11),    // @: ... ;
    
    code_block          = (1 << 12),    // @{ ... }
    
    code_if             = (1 << 13),    // @if( ... ){ ... }
    code_elif           = (1 << 14),    // else if( ... ){ ... }
    code_else           = (1 << 15),    // else{ ... }
    code_switch         = (1 << 16),    // @switch( ... ){ ... }

    code_while          = (1 << 17),    // @while( ... ){ ... }
    code_for            = (1 << 18),    // @for( ... ){ ... }
    code_do             = (1 << 19),    // @do{ ... }
    code_dowhile        = (1 << 20),    // while( ... );
    
    code_try            = (1 << 21),    // @try{ ... }
    code_trycatch       = (1 << 22),    // catch( ... ){ ... }
    
    code_funi           = (1 << 23),    // @funi{ ... }
    code_func           = (1 << 24),    // @func{ ... }
    
    code_funa           = (1 << 25),    // @<type> func_name(args...);
    code_await          = (1 << 26),    // @await type r = async_func_name(params...);
    
    markup_markdown     = (1 << 27),    // @(markdown){...} @(md){...}
    markup_html         = (1 << 28),    // @(html){...} @(htm){...}
    
    sub_section         = (1 << 29),    // 
};
MD_ENUM_FLAGS(section_type)
}//::evmvc::fanjet::ast

}}//::evmvc::fanjet
#endif //_libevmvc_fanjet_common_h
