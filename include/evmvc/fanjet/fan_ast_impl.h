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

#include "fan_ast.h"


namespace evmvc { namespace fanjet { namespace ast {

inline ast::token root_node_t::parse(ast::token t)
{
    // first node is always a literal node;
    ast::literal_node l = std::make_shared<ast::literal_node_t>(
        ast::section_type::markup_html
    );
    this->add_child(l);
    return l->parse(t);
}

inline void process_fan_key(ast::token& t)
{
    
}

inline ast::token literal_node_t::parse(ast::token t)
{
    ast::token st = t;
    while(t){
        if(t->is_fan_key()){
            
        }
        t = t->next();
    }
    return t;
}

inline ast::token expr_node_t::parse(ast::token t)
{
}

inline ast::token directive_node_t::parse(ast::token t)
{
}


inline ast::token comment_node_t::parse(ast::token t)
{
}

inline ast::token output_node_t::parse(ast::token t)
{
}

inline ast::token clode_block_node_t::parse(ast::token t)
{
}

inline ast::token code_control_node_t::parse(ast::token t)
{
}

inline ast::token code_err_node_t::parse(ast::token t)
{
}

inline ast::token code_fun_node_t::parse(ast::token t)
{
}

inline ast::token code_async_node_t::parse(ast::token t)
{
}



}}}//::evmvc::fanjet::ast