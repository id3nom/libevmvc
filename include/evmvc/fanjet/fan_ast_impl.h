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

inline void node_t::add_sibling_token(sibling_pos pos, ast::token t)
{
    auto tn = std::make_shared<ast::token_node_t>();
    tn->_token = t;
    this->add_sibling(pos, tn);
}

inline void node_t::add_token(ast::token t)
{
    auto tn = std::make_shared<ast::token_node_t>();
    tn->_token = t;
    this->add_child(tn);
}


inline void root_node_t::parse(ast::token t)
{
    // first node is always a literal node;
    ast::literal_node l = std::make_shared<ast::literal_node_t>(
        ast::section_type::literal
    );
    this->add_child(l);
    l->parse(t);
}

inline bool open_scope(ast::token& t, ast::node_t* pn)
{
    if(t->is_fan_start_key()){
        ast::node n = nullptr;
        if(t->is_fan_comment() || t->is_fan_region())
            n = std::make_shared<ast::comment_node_t>(
                t->is_fan_line_comment() ?
                    ast::section_type::comment_line :
                t->is_fan_blk_comment_open() ?
                    ast::section_type::comment_block :
                    ast::section_type::region_start
            );
        if(t->is_fan_directive())
            n = std::make_shared<ast::directive_node_t>(
                t->is_fan_namespace() ?
                    ast::section_type::dir_ns :
                t->is_fan_name() ?
                    ast::section_type::dir_name :
                t->is_fan_layout() ?
                    ast::section_type::dir_layout :
                t->is_fan_header() ?
                    ast::section_type::dir_header :
                    ast::section_type::dir_inherits
            );
        if(t->is_fan_output())
            n = std::make_shared<ast::output_node_t>(
                t->is_fan_output_raw() ?
                    ast::section_type::output_raw :
                    ast::section_type::output_enc
            );
        if(t->is_fan_code_block())
            n = std::make_shared<ast::code_block_node_t>(
                ast::section_type::code_block
            );
        if(t->is_fan_control())
            n = std::make_shared<ast::code_control_node_t>(
                t->is_fan_if() ?
                    ast::section_type::code_if :
                t->is_fan_switch() ?
                    ast::section_type::code_switch :
                t->is_fan_while() ?
                    ast::section_type::code_while :
                t->is_fan_for() ?
                    ast::section_type::code_for :
                    ast::section_type::code_do
            );
        if(t->is_fan_code_block())
            n = std::make_shared<ast::code_err_node_t>(
                ast::section_type::code_try
            );
        if(t->is_fan_funi() || t->is_fan_func())
            n = std::make_shared<ast::code_fun_node_t>(
                t->is_fan_funi() ?
                    ast::section_type::code_funi :
                    ast::section_type::code_func
            );
        if(t->is_fan_funa() || t->is_fan_await())
            n = std::make_shared<ast::code_async_node_t>(
                t->is_fan_funa() ?
                    ast::section_type::code_funa :
                    ast::section_type::code_await
            );
        
        if(t->is_parenthesis_open())
            n = std::make_shared<ast::expr_node_t>();
        
        if(t->is_fan_markup_open()){
            // find lang
            token tl = t->next();
            std::string l;
            while(tl && !tl->is_parenthesis_close())
                l += tl->text();
            md::trim(l);
            if(l != "html" && l != "htm" && l != "markdown" && l != "md")
                throw MD_ERR(
                    "invalid literal language of '{}', line: '{}'\n"
                    "available language are 'html, htm, markdown and md!'",
                    l, t->line()
                );
            n = std::make_shared<ast::output_node_t>(
                l == "html" || l == "htm" ?
                    ast::section_type::markup_html :
                l == "markdown" || l == "md" ?
                    ast::section_type::markup_markdown :
                    ast::section_type::invalid
            );
        }
        
        if(t->is_double_quote() || t->is_single_quote() || t->is_backtick()){
            n = std::make_shared<ast::string_node_t>(
                t->text()
            );
        }
        
        if(n){
            token nt = t;
            t = nt->snip();
            pn->add_child(n);
            if(t)
                n->add_sibling_token(ast::sibling_pos::prev, t);
            return true;
        }
    }
    return false;
}

inline void close_scope(ast::token& t, ast::node_t* pn)
{
    token nt = t->next();
    if(nt)
        t = nt->snip();
    else if(!t->is_root())
        t = t->root();
    
    pn->add_token(t);
    pn->parent()->parse(nt);
}

inline void literal_node_t::parse(ast::token t)
{
    while(t){
        if(open_scope(t, this))
            return;
        
        if(this->sec_type() != section_type::literal){
            if(t->is_curly_brace_close()){
                close_scope(t, this);
                return;
            }
            
            if(!t->next())
                throw MD_ERR(
                    "Missing closing brace for node: '{}', line: '{}'",
                    to_string(this->node_type()), t->root()->line()
                );
        }
        
        if(t) t = t->next();
    }
}

inline void expr_node_t::parse(ast::token t)
{
    while(t){
        if(open_scope(t, this))
            return;
        
        if(t->is_parenthesis_close()){
            close_scope(t, this);
            return;
        }
        if(t) t = t->next();
    }
}

inline void string_node_t::parse(ast::token t)
{
    while(t){
        ast::token nt = t->next();
        
        if(this->_enclosing_char == "\""){
            if(nt && nt->text() == "\""){
                t = nt->next();
                continue;
            }
            
            close_scope(t, this);
            return;
            
        } else if(this->_enclosing_char == "'"){
            if(nt && nt->text() == "'"){
                t = nt->next();
                continue;
            }
            
            close_scope(t, this);
            return;
        
        } else if(this->_enclosing_char == "`"){
            if(nt && nt->text() == "`"){
                t = nt->next();
                continue;
            }
            
            close_scope(t, this);
            return;
            
        }
        
        // only verify open_scope if w'ere in a literal section
        if(this->parent() && this->parent()->node_type() == node_type::literal)
            if(open_scope(t, this))
                return;
        
        if(t)
            t = t->next();
    }
}

inline void directive_node_t::parse(ast::token t)
{
    while(t){
        switch(this->sec_type()){
            case ast::section_type::dir_ns:
            case ast::section_type::dir_name:
            case ast::section_type::dir_layout:
                if(t->is_eol()){
                    close_scope(t, this);
                    return;
                }
                break;
            
            case ast::section_type::dir_header:
            case ast::section_type::dir_inherits:
                if(t->is_semicolon()){
                    close_scope(t, this);
                    return;
                }
                if(open_scope(t, this))
                    return;
                
                break;
            default:
                break;
        }
        
        if(t)
            t = t->next();
    }
}


inline void comment_node_t::parse(ast::token t)
{
    while(t){
        switch(this->sec_type()){
            case ast::section_type::comment_line:
            case ast::section_type::region_start:
            case ast::section_type::region_end:
                if(t->is_eol()){
                    close_scope(t, this);
                    return;
                }
                break;
            
            case ast::section_type::comment_block:
                if(t->is_fan_blk_comment_close()){
                    close_scope(t, this);
                    return;
                }
                break;
            default:
                break;
        }
        
        if(t)
            t = t->next();
    }
}

inline void output_node_t::parse(ast::token t)
{
    while(t){
        if(t->is_semicolon()){
            close_scope(t, this);
            return;
        }
        
        if(t)
            t = t->next();
    }
}

inline void code_block_node_t::parse(ast::token t)
{
    while(t){
        if(open_scope(t, this))
            return;
        
        if(t->is_curly_brace_close()){
            close_scope(t, this);
            return;
        }
        
        if(t)
            t = t->next();
    }
}

inline void code_control_node_t::parse(ast::token t)
{
    while(t){
        switch(this->sec_type()){
            case ast::section_type::code_if:
                //  if
                //      (expr)
                //      {
                //      }
                //      else
                //      if
                //      (expr)
                //      {
                //      }
                //      else
                //      {
                //      }
                
                break;
            
            case ast::section_type::code_switch:
                //  switch
                //      (expr)
                //      {
                //      }
                
                break;
            
            case ast::section_type::code_while:
                //  while
                //      (expr)
                //      {
                //      }
                
                break;
            
            case ast::section_type::code_for:
                //  for
                //      (expr)
                //      {
                //      }
                
                break;
            
            case ast::section_type::code_do:
                //  do
                //  {
                //  }
                //  while
                //  (expr)
                //  ;
                
                break;
        }
        
        if(open_scope(t, this))
            return;
        
        if(t->is_curly_brace_close()){
            close_scope(t, this);
            return;
        }
        
        if(t)
            t = t->next();
    }
}

inline void code_err_node_t::parse(ast::token t)
{
}

inline void code_fun_node_t::parse(ast::token t)
{
}

inline void code_async_node_t::parse(ast::token t)
{
}



}}}//::evmvc::fanjet::ast