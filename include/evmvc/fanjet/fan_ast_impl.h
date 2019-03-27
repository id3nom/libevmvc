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
    if(!t || t->empty(true))
        return;
    
    auto tn = token_node(new ast::token_node_t());
    tn->_token = t;
    this->add_sibling(pos, tn);
}

inline void node_t::add_token(ast::token t)
{
    if(!t || t->empty(true))
        return;
    
    auto tn = token_node(new ast::token_node_t());
    tn->_token = t;
    this->add_child(tn);
}


inline void root_node_t::parse(ast::token t)
{
    // first node is always a literal node;
    ast::literal_node l = literal_node(new literal_node_t(
        ast::section_type::literal
    ));
    this->add_child(l);
    l->parse(t);
}

inline bool open_scope(ast::token& t, ast::node_t* pn)
{
    if(t->is_fan_start_key() ||
        
        t->is_double_quote() || t->is_single_quote() || t->is_backtick() ||
        t->is_parenthesis_open() || t->is_curly_brace_open() ||
        
        t->is_cpp_try() || t->is_cpp_return() ||

        t->is_cpp_if() || t->is_cpp_switch() ||
        t->is_cpp_for() || t->is_cpp_do() || t->is_cpp_while()
        
    ){
        size_t end_hup = 1;
        ast::node n = nullptr;
        if(t->is_fan_comment() || t->is_fan_region())
            n = comment_node(new comment_node_t(
                t->is_fan_line_comment() ?
                    ast::section_type::comment_line :
                t->is_fan_blk_comment_open() ?
                    ast::section_type::comment_block :
                    ast::section_type::region_start
            ));
        else if(t->is_fan_directive())
            n = directive_node(new directive_node_t(
                t->is_fan_namespace() ?
                    ast::section_type::dir_ns :
                t->is_fan_name() ?
                    ast::section_type::dir_name :
                t->is_fan_layout() ?
                    ast::section_type::dir_layout :
                t->is_fan_header() ?
                    ast::section_type::dir_header :
                    ast::section_type::dir_inherits
            ));
        else if(t->is_fan_output())
            n = output_node(new output_node_t(
                t->is_fan_output_raw() ?
                    ast::section_type::output_raw :
                    ast::section_type::output_enc
            ));
        else if(t->is_fan_code_block())
            n = code_block_node(new code_block_node_t());
        else if(t->is_fan_control())
            n = code_control_node(new code_control_node_t(
                t->is_fan_if() ?
                    ast::section_type::code_if :
                t->is_fan_switch() ?
                    ast::section_type::code_switch :
                t->is_fan_while() ?
                    ast::section_type::code_while :
                t->is_fan_for() ?
                    ast::section_type::code_for :
                    ast::section_type::code_do
            ));
        else if(t->is_fan_try() || 
            (t->is_cpp_try() && pn->node_type() != node_type::literal)
        )
            n = code_err_node(new code_err_node_t(
                ast::section_type::code_try
            ));
        else if(t->is_fan_funi() || t->is_fan_func())
            n = code_fun_node(new code_fun_node_t(
                t->is_fan_funi() ?
                    ast::section_type::code_funi :
                    ast::section_type::code_func
            ));
        else if(t->is_fan_funa() || t->is_fan_await())
            n = code_async_node(new code_async_node_t(
                t->is_fan_funa() ?
                    ast::section_type::code_funa :
                    ast::section_type::code_await
            ));
            
        else if(pn->node_type() != node_type::literal &&
            (
                t->is_cpp_if() ||
                t->is_cpp_switch() ||
                t->is_cpp_for() ||
                t->is_cpp_do() ||
                (t->is_cpp_while() && 
                    pn->node_type() != node_type::code_control)
            )
        )
            n = code_control_node(new code_control_node_t(
                t->is_cpp_if() ?
                    ast::section_type::code_if :
                t->is_cpp_switch() ?
                    ast::section_type::code_switch :
                t->is_cpp_while() ?
                    ast::section_type::code_while :
                t->is_cpp_for() ?
                    ast::section_type::code_for :
                    ast::section_type::code_do
            ));
        
        else if(t->is_parenthesis_open() &&
            pn->node_type() != node_type::literal
        )
            n = expr_node(new expr_node_t(
                expr_type::parens
            ));
        else if(pn->node_type() != node_type::literal &&
            (
                t->is_cpp_return() ||
                t->is_cpp_throw()
            )
        )
            n = expr_node(new expr_node_t(
                expr_type::semicol
            ));
        
        else if(t->is_curly_brace_open() &&
            pn->node_type() != node_type::literal
        )
            n = code_block_node(new code_block_node_t());
        
        else if(t->is_fan_markup_open()){
            // find lang
            token tl = t->next();
            std::string l;
            while(tl && !tl->is_parenthesis_close()){
                ++end_hup;
                l += tl->text();
                tl = tl->next();
            }
            md::trim(l);
            if(l != "html" && l != "htm" && l != "markdown" && l != "md")
                throw MD_ERR(
                    "invalid literal language of '{}', line: '{}', col: '{}'\n"
                    "available language are 'html, htm, markdown and md!'",
                    l, t->line(), t->col()
                );
            n = literal_node(new literal_node_t(
                l == "html" || l == "htm" ?
                    ast::section_type::markup_html :
                l == "markdown" || l == "md" ?
                    ast::section_type::markup_markdown :
                    ast::section_type::invalid
            ));
            while(tl && !tl->is_curly_brace_open()){
                ++end_hup;
                tl = tl->next();
            }
            ++end_hup;
        }
        
        else if(
            t->is_double_quote() || t->is_single_quote() || t->is_backtick()
        ){
            n = string_node(new string_node_t(
                t->text()
            ));
        }
        
        if(n){
            token nt = t;
            t = nt->snip();
            pn->add_child(n);
            if(t)
                n->add_sibling_token(ast::sibling_pos::prev, t);
            
            // we don't want nt instance to be release
            t = nt;
            // hup
            for(size_t i = 0; i < end_hup; ++i)
                nt = nt->next();
            
            t = nt->snip();
            if(t)
                n->add_token(t);
            
            n->parse(nt);
            return true;
        }
    }
    return false;
}

inline void close_scope(ast::token& t, ast::node_t* pn)
{
    if(!t)
        return;
    
    token nt = t->next();
    if(nt)
        t = nt->snip();
    else if(!t->is_root())
        t = t->root();
    
    token lt = t;
    while(lt && lt->next())
        lt = lt->next();
    if(lt)
        t = lt->snip();
    pn->add_token(t);
    if(lt)
        pn->add_token(lt);
    
    pn->parent()->parse(nt);
}

inline void literal_node_t::parse(ast::token t)
{
    ast::token tt = t;
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
                    "Missing closing brace for node: '{}', "
                    "line: '{}', col: '{}'",
                    to_string(this->node_type()), 
                    t->root()->line(), t->root()->col()
                );
        }
        
        if(t) t = t->next();
    }
    
    this->add_token(tt);
}

inline void expr_node_t::parse(ast::token t)
{
    ast::token st = t;
    while(t){
        if(open_scope(t, this))
            return;
        
        if(t->is_cpp_op() && !t->is_scope_resolution()){
            this->add_token(t->snip());
            st = t;
            t = t->next();
            this->add_token(t->snip());
            st = t;
        }
        
        if(this->_type == expr_type::parens && t->is_parenthesis_close()){
            if(this->parent()->sec_type() == ast::section_type::code_funa){
                auto an =
                    std::static_pointer_cast<code_async_node_t>(
                        this->parent()
                    );
                

                bool is_dec = false;
                bool is_def = false;
                ast::token it = t->next();
                while(it){
                    if(!it->trim_text().empty() && !it->is_eol()){
                        if(it->is_semicolon()){
                            is_dec = true;
                            break;
                        }else if(it->is_curly_brace_open()){
                            is_def = true;
                            break;
                        }else
                            break;
                    }
                    it = it->next();
                }
                
                if(is_dec){
                    an->_need_semicol = true;
                    an->_need_code = false;
                }else if(is_def){
                    an->_need_semicol = false;
                    an->_need_code = true;
                }else
                    throw MD_ERR(
                        "Open curly brace or semicolon "
                        "required, line: '{}', col '{}'",
                        t->line(), t->col()
                    );
            }
            
            close_scope(t, this);
            return;
        }
        else if(this->_type == expr_type::semicol && t->is_semicolon()){
            close_scope(t, this);
            return;
        }
        
        if(t) t = t->next();
    }
}

inline void string_node_t::parse(ast::token t)
{
    ast::token st = t;
    while(t){
        ast::token nt = t->next();
        
        if(this->_enclosing_char == "\"" && t->is_double_quote()){
            if(nt && nt->text() == "\""){
                t = nt->next();
                continue;
            }
            
            close_scope(t, this);
            return;
            
        } else if(this->_enclosing_char == "'" && t->is_single_quote()){
            if(nt && nt->text() == "'"){
                t = nt->next();
                continue;
            }
            
            close_scope(t, this);
            return;
        
        } else if(this->_enclosing_char == "`" && t->is_backtick()){
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
        
        if(t) t = t->next();
    }
}

inline void directive_node_t::parse(ast::token t)
{
    ast::token st = t;
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
    ast::token st = t;
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
    ast::token st = t;
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
    ast::token st = t;
    while(t){
        if(open_scope(t, this))
            return;
        
        if(t->is_curly_brace_close()){
            
            if(this->parent()->node_type() == ast::node_type::code_control){
                auto cn =
                    std::static_pointer_cast<code_control_node_t>(
                        this->parent()
                    );
                
                if(cn->sec_type() == ast::section_type::code_if){
                    bool is_else = false;
                    bool is_if = false;
                    ast::token it = t->next();
                    while(it){
                        if(!it->trim_text().empty() && !it->is_eol()){
                            if(it->is_cpp_else())
                                is_else = true;
                            else if(is_else && it->is_cpp_if()){
                                is_if = true;
                                break;
                            }else
                                break;
                        }
                        it = it->next();
                    }
                    
                    if(is_else && is_if){
                        cn->_need_expr = true;
                        cn->_need_code = true;
                    }else if(is_else){
                        cn->_need_code = true;
                    }else{
                        cn->_done = true;
                    }
                    
                }else if(cn->sec_type() != ast::section_type::code_do)
                    cn->_done = true;
            }

            if(this->parent()->node_type() == ast::node_type::code_err){
                auto en =
                    std::static_pointer_cast<code_err_node_t>(
                        this->parent()
                    );
                
                bool is_catch = false;
                ast::token it = t->next();
                while(it){
                    if(!it->trim_text().empty() && !it->is_eol()){
                        if(it->is_cpp_catch()){
                            is_catch = true;
                            break;
                        }else
                            break;
                    }
                    it = it->next();
                }
                
                if(is_catch){
                    en->_need_expr = true;
                    en->_need_code = true;
                }else{
                    en->_done = true;
                }
            }

            if(this->parent()->sec_type() == ast::section_type::code_funa){
                auto an =
                    std::static_pointer_cast<code_async_node_t>(
                        this->parent()
                    );
                an->_done = true;
            }
            
            close_scope(t, this);
            return;
        }
        
        if(t)
            t = t->next();
    }
}

inline void code_control_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this);
        return;
    }
    
    ast::token st = t;
    while(t){
        if(!t->trim_text().empty() && !t->is_eol()
            && !t->is_cpp_else() && !t->is_cpp_if() &&
            !t->is_cpp_while()
        ){
            if(_need_expr && 
                (
                    this->sec_type() != section_type::code_do ||
                    (this->sec_type() == section_type::code_do && !_need_code)
                )
            ){
                if(t->is_parenthesis_open()){
                    _need_expr = false;
                    open_scope(t, this);
                    return;
                }
                throw MD_ERR(
                    "Expression required, line: '{}', col: '{}'",
                    t->line(), t->col()
                );
            }
            
            if(_need_code &&
                (
                    this->sec_type() != section_type::code_do ||
                    (this->sec_type() == section_type::code_do && _need_expr)
                )
            ){
                if(t->is_curly_brace_open()){
                    _need_code = false;
                    open_scope(t, this);
                    return;
                }
                throw MD_ERR(
                    "Open curly brace is required, line: '{}', col: '{}'",
                    t->line(), t->col()
                );
            }
            
            if(_need_semicol){
                if(t->is_semicolon()){
                    _need_semicol = false;
                    
                    ast::token nt = t->next();
                    if(nt)
                        t = nt->snip();
                    else if(!t->is_root())
                        t = t->root();
                    
                    this->add_token(t);
                    this->parent()->parse(nt);
                    return;
                }
                throw MD_ERR(
                    "Semicolon is required, line: '{}', col: '{}'",
                    t->line(), t->col()
                );
            }
        }
        
        if(t) t = t->next();
    }
}

inline void code_err_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this);
        return;
    }
    
    ast::token st = t;
    while(t){
        if(!t->trim_text().empty() && !t->is_eol() && !t->is_cpp_catch()){

            if(_need_expr){
                if(t->is_parenthesis_open()){
                    _need_expr = false;
                    open_scope(t, this);
                    return;
                }
                throw MD_ERR(
                    "Expression required, line: '{}', col: '{}'",
                    t->line(), t->col()
                );
            }
            
            if(_need_code){
                if(t->is_curly_brace_open()){
                    _need_code = false;
                    open_scope(t, this);
                    return;
                }
                throw MD_ERR(
                    "Open curly brace is required, line: '{}', col: '{}'",
                    t->line(), t->col()
                );
            }
        }
        
        if(t) t = t->next();
    }
}

inline void code_fun_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this);
        return;
    }
    
    ast::token st = t;
    while(t){
        if(!t->trim_text().empty() && !t->is_eol()){
            if(t->is_curly_brace_open()){
                _done = true;
                open_scope(t, this);
                return;
            }
            throw MD_ERR(
                "Open curly brace is required, line: '{}', col: '{}'",
                t->line(), t->col()
            );
        }
        
        if(t) t = t->next();
    }
}

inline void code_async_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this);
        return;
    }
    
    //  @<
    //      (expr)
    //      ;|{
    
    //  @await
    //      (expr)
    //      ;
    ast::token st = t;
    int line = t->line();
    int opened_tags = 
        _need_expr &&
        this->sec_type() == ast::section_type::code_funa ?
            1 : 0;
    
    while(t){
        if(!t->trim_text().empty() && !t->is_eol()
        ){
            if(opened_tags > 0){
                if(t->is_gt())
                    --opened_tags;
                else if(t->is_lt())
                    ++opened_tags;
                
            }else if(_need_expr){
                if(t->is_parenthesis_open()){
                    _need_expr = false;
                    open_scope(t, this);
                    return;
                }
                throw MD_ERR(
                    "Expression required, line: '{}', col: '{}'",
                    t->line(), t->col()
                );
            }else if(_need_code){
                if(t->is_curly_brace_open()){
                    _need_code = false;
                    open_scope(t, this);
                    return;
                }
                throw MD_ERR(
                    "Open curly brace is required, line: '{}', col: '{}'",
                    t->line(), t->col()
                );
            }else if(_need_semicol){
                if(t->is_semicolon()){
                    _need_semicol = false;
                    _done = true;
                    ast::token nt = t->next();
                    if(nt)
                        t = nt->snip();
                    else if(!t->is_root())
                        t = t->root();
                    
                    this->add_token(t);
                    this->parent()->parse(nt);
                    return;
                }
                throw MD_ERR(
                    "Semicolon is required, line: '{}', col: '{}'",
                    t->line(), t->col()
                );
            }
        }
        
        if(opened_tags > 0)
            _async_type += t->text();
        
        if(t) t = t->next();
        
        if(opened_tags == 0 && t && _need_expr && !t->is_parenthesis_open()){
            auto pt = t->prev();
            if(pt->is_gt()){
                auto tt = pt->snip();
                this->add_token(tt);
                this->add_token(t->snip());
            }
            st = t;
            while(t && !t->is_parenthesis_open())
                t = t->next();
            if(t)
                this->add_token(t->snip());
        }
    }
    
    if(opened_tags != 0)
        throw MD_ERR(
            "Invalid async function definition, line: '{}'",
            line
        );
}



}}}//::evmvc::fanjet::ast