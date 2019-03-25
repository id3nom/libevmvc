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

namespace evmvc { namespace fanjet { namespace ast {
enum class section_type
    : int
{
    invalid             = INT_MIN,
    root                = INT_MIN +1,
    token               = INT_MIN +2,
    expr                = INT_MIN +3,
    string              = INT_MIN +4,
    
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

inline md::string_view to_string(section_type t)
{
    switch(t){
        case section_type::invalid:
            return "invalid";
        case section_type::root:
            return "root";
        case section_type::token:
            return "token";
        case section_type::expr:
            return "expr";
        case section_type::string:
            return "string";

        case section_type::dir_ns:
            return "dir_ns";
        case section_type::dir_name:
            return "dir_name";
        case section_type::dir_layout:
            return "dir_layout";
        case section_type::dir_header:
            return "dir_header";
        case section_type::dir_inherits:
            return "dir_inherits";

        case section_type::literal:
            return "literal";

        case section_type::comment_line:
            return "comment_line";
        case section_type::comment_block:
            return "comment_block";
        case section_type::region_start:
            return "region_start";
        case section_type::region_end:
            return "region_end";

        case section_type::output_raw:
            return "output_raw";
        case section_type::output_enc:
            return "output_enc";

        case section_type::code_block:
            return "code_block";

        case section_type::code_if:
            return "code_if";
        case section_type::code_elif:
            return "code_elif";
        case section_type::code_else:
            return "code_else";
        case section_type::code_switch:
            return "code_switch";

        case section_type::code_while:
            return "code_while";
        case section_type::code_for:
            return "code_for";
        case section_type::code_do:
            return "code_do";
        case section_type::code_dowhile:
            return "code_dowhile";

        case section_type::code_try:
            return "code_try";
        case section_type::code_trycatch:
            return "code_trycatch";

        case section_type::code_funi:
            return "code_funi";
        case section_type::code_func:
            return "code_func";

        case section_type::code_funa:
            return "code_funa";
        case section_type::code_await:
            return "code_await";

        case section_type::markup_html:
            return "markup_html";
        case section_type::markup_markdown:
            return "markup_markdown";

        case section_type::sub_section:
            return "sub_section";
            
        
        default:
            return "UNKNOWN";
    }
}

}}}//::evmvc::fanjet::ast

#endif //_libevmvc_fanjet_common_h
