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

enum class code_section_type
{
    sub_section,
    
    dir_ns,             // @namespace ...
    dir_name,           // @name ...
    dir_layout,         // @layout ...
    dir_header,         // @header ... ;
    dir_inherits,       // @inherits ... ;
    
    literal,            // text <tag attr> ... </tag><tag/> 
    
    comment_line,       // @** ...
    comment_block,      // @* ... *@
    
    region_start,       // @region ...
    region_end,         // @endregion ...
    
    output_raw,         // @:: ... ;
    output_enc,         // @: ... ;
    
    code_block,         // @{ ... }
    
    code_if,            // @if( ... ){ ... }
    code_elif,          // else if( ... ){ ... }
    code_else,          // else{ ... }
    code_switch,        // @switch( ... ){ ... }

    code_while,         // @while( ... ){ ... }
    code_for,           // @for( ... ){ ... }
    code_do,            // @do{ ... }
    code_dowhile,       // while( ... );
    
    code_try,           // @try{ ... }
    code_trycatch,      // catch( ... ){ ... }
    
    code_funi,          // @funi{ ... }
    code_func,          // @func{ ... }
    
    code_funa,          // @<type> func_name(args...);
    code_await,         // @await type r = async_func_name(params...);
    
    markup_markdown,    // @(markdown){...} @(md){...}
    markup_html,        // @(html){...} @(htm){...}
    
};

}}//::evmvc::fanjet
#endif //_libevmvc_fanjet_common_h
