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
        "html",
        ast::section_type::literal
    ));
    this->add_child(l);
    l->parse(t);
}

inline bool open_scope(
    ast::token& t, ast::node_t* pn, ast::node n,
    size_t end_hup = 1, bool parse = true)
{
    if(!n)
        return false;
        
    token nt = t;
    t = nt->snip();
    pn->add_child(n);
    if(t)
        n->add_sibling_token(ast::sibling_pos::prev, t);
    
    // we don't want nt instance to be release
    t = nt;
    // hup
    for(size_t i = 0; i < end_hup; ++i){
        nt = nt->next();
        if(!nt){
            throw MD_ERR(
                "Are you missing an open scope? line: '{}', col: '{}'",
                t->line(), t->col()
            );
        }
    }
    
    t = nt->snip();
    if(t)
        n->add_token(t);
    
    t = nt;
    if(parse)
        n->parse(t);
    return true;
}

inline bool open_code_string(ast::token& t, ast::node_t* pn)
{
    if(!t->is_double_quote() && !t->is_single_quote())
        return false;
    ast::node n = string_node(new string_node_t(
        t->text(), t->is_double_quote() ? "\\\"" : "\\'"
    ));
    
    return open_scope(t, pn, n);
}

inline bool open_attr_string(ast::token& t, ast::node_t* pn)
{
    if(!t->is_double_quote() && !t->is_single_quote())
        return false;
    
    ast::node n = string_node(new string_node_t(
        t->text(), ""
    ));
    return open_scope(t, pn, n);
}

inline bool open_markdown_string(ast::token& t, ast::node_t* pn)
{
    static std::unordered_set<std::string> _s {
        "``````````", "`````````", "````````", "```````", "``````", 
        "`````", "````", "```", "``",
        "~~~~~~~~~~", "~~~~~~~~~", "~~~~~~~~", "~~~~~~~", "~~~~~~", 
        "~~~~~", "~~~~", "~~~", "~~"
    };
    
    ast::node n = nullptr;
    if(t->is_eol()){
        ast::token nt = t->next();
        if(!nt)
            return false;
        
        bool is_code = false;
        if(nt->is_space()){
            size_t c = 0;
            while(nt && nt->is_space_or_tab()){
                ++c;
                nt = nt->next();
            }
            
            is_code = c >= 4;
        }else if(nt->is_tab()){
            is_code = true;
        }
        
        if(is_code){
            n = string_node(new string_node_t(
                "\n", ""
            ));
            return open_scope(nt, pn, n);
        }
        
        nt = t->next();
        if(_s.find(nt->text()) == _s.end())
            return false;
        
        n = string_node(new string_node_t(
            "\n" + nt->text(), ""
        ));
        return open_scope(nt, pn, n);
    }
    
    if(_s.find(t->text()) == _s.end())
        return false;
    
    n = string_node(new string_node_t(
        t->text(), ""
    ));
    return open_scope(t, pn, n);
}


inline bool open_fan_comment(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_comment() && !t->is_fan_region())
        return false;
    
    ast::node n = comment_node(new comment_node_t(
        t->is_fan_line_comment() ?
            ast::section_type::comment_line :
        t->is_fan_blk_comment_open() ?
            ast::section_type::comment_block :
            ast::section_type::region_start
    ));
    return open_scope(t, pn, n);
}

inline bool open_fan_directive(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_directive())
        return false;
    
    if(pn->sec_type() != section_type::literal)
        throw MD_ERR(
            "Directive can only be defined at the first scope level."
            " line: '{}', col: '{}', pos: '{}'",
            t->line(), t->col(), t->pos()
        );
    
    ast::node n = directive_node(new directive_node_t(
        t->is_fan_namespace() ?
            ast::section_type::dir_ns :
        t->is_fan_path() ?
            ast::section_type::dir_path :
        t->is_fan_name() ?
            ast::section_type::dir_name :
        t->is_fan_layout() ?
            ast::section_type::dir_layout :
        t->is_fan_include() ?
            ast::section_type::dir_include :
        t->is_fan_header() ?
            ast::section_type::dir_header :
            ast::section_type::dir_inherits
    ));
    return open_scope(t, pn, n);
}

inline bool open_fan_output(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_output())
        return false;
    ast::output_node n = output_node(new output_node_t(
        t->is_fan_output_raw() ?
            ast::section_type::output_raw :
            ast::section_type::output_enc
    ));
    
    if(!open_scope(t, pn, n, 1, false))
        throw MD_ERR("output_node instance is NULL!");
    n->_done = true;
    
    ast::node expr_n = expr_node(new expr_node_t(
        expr_type::semicol
    ));
    return open_scope(t, n.get(), expr_n);
}

inline bool open_fan_code_block(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_code_block())
        return false;
    
    ast::node n = code_block_node(new code_block_node_t());
    return open_scope(t, pn, n);
}

inline bool open_code_block(ast::token& t, ast::node_t* pn)
{
    if(!t->is_curly_brace_open())
        return false;
    
    ast::node n = code_block_node(new code_block_node_t());
    return open_scope(t, pn, n);
}

inline bool open_fan_control(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_control())
        return false;
    
    ast::node n = code_control_node(new code_control_node_t(
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
    return open_scope(t, pn, n);
}

inline bool open_control(ast::token& t, ast::node_t* pn)
{
    if(
        !t->is_cpp_if() &&
        !t->is_cpp_switch() &&
        !t->is_cpp_for() &&
        !t->is_cpp_do() &&
        !(t->is_cpp_while() && pn->node_type() != node_type::code_control)
    )
        return false;
    
    ast::node n = code_control_node(new code_control_node_t(
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
    return open_scope(t, pn, n);
}

inline bool open_fan_try(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_try())
        return false;
        
    ast::node n = code_err_node(new code_err_node_t(
        ast::section_type::code_try
    ));
    return open_scope(t, pn, n);
}

inline bool open_try(ast::token& t, ast::node_t* pn)
{
    if(!t->is_cpp_try())
        return false;
    
    ast::node n = code_err_node(new code_err_node_t(
        ast::section_type::code_try
    ));
    return open_scope(t, pn, n);
}

inline bool open_fan_funi_func(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_funi() && !t->is_fan_func())
        return false;
    
    ast::node n = code_fun_node(new code_fun_node_t(
        t->is_fan_funi() ?
            ast::section_type::code_funi :
            ast::section_type::code_func
    ));
    return open_scope(t, pn, n);
}

inline bool open_fan_funa_await(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_funa() && !t->is_fan_await())
        return false;
    
    ast::node n = code_async_node(new code_async_node_t(
        t->is_fan_funa() ?
            ast::section_type::code_funa :
            ast::section_type::code_await
    ));
    return open_scope(t, pn, n);
}

inline bool open_expr(ast::token& t, ast::node_t* pn)
{
    ast::node n = nullptr;
    if(t->is_parenthesis_open())
        n = expr_node(new expr_node_t(
            expr_type::parens
        ));
    
    else if(t->is_cpp_return() || t->is_cpp_throw())
        n = expr_node(new expr_node_t(
            expr_type::semicol
        ));
    
    return open_scope(t, pn, n);
}

inline bool open_fan_markup(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_markup_open())
        return false;
    
    size_t end_hup = 1;
    
    // find lang
    token tl = t->next();
    std::string l;
    while(tl && !tl->is_parenthesis_close()){
        ++end_hup;
        l += tl->text();
        tl = tl->next();
    }
    md::trim(l);
    ast::root_node rn = std::static_pointer_cast<ast::root_node_t>(pn->root());
    if(!rn->support_markup_lang(l)){
        std::unordered_set<std::string> allowed_langs = {
            "html", "htm",
            "markdown", "md",
            "tex",
            "latex",
            "wiki"
        };
        
        throw MD_ERR(
            "Invalid markup language '{}' at line: '{}', col: '{}'!\n\n"
            "Available language are: '{}'\n"
            "Known language are: '{}'\n"
            "Please note that known language must be enabled, "
            "except for 'html' and 'htm' languages.",
            l, t->line(), t->col(),
            md::join(rn->markup_langs(), ", "),
            md::join(allowed_langs, ", ")
        );

        // throw MD_ERR(
        //     "invalid literal language of '{}', line: '{}', col: '{}'\n"
        //     "available language are 'html, htm, markdown and md!'",
        //     md::replace_substring_copy(
        //         md::replace_substring_copy(
        //             l, "{", "{{"
        //         ), "}", "}}"
        //     ),
        //     t->line(), t->col()
        // );
    }
    
    ast::node n = literal_node(new literal_node_t(
        l,
        l == "html" || l == "htm" ?
            ast::section_type::markup_html :
            ast::section_type::markup_other
    ));
    while(tl && !tl->is_curly_brace_open()){
        ++end_hup;
        tl = tl->next();
    }
    ++end_hup;
    
    return open_scope(t, pn, n, end_hup);
}

inline bool open_fan_key(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_keyword())
        return false;
    
    ast::node n = fan_key_node(new fan_key_node_t(
        t->is_fan_body() ?
            ast::section_type::body :

        t->is_fan_scripts() ?
            ast::section_type::scripts :
        t->is_fan_styles() ?
            ast::section_type::styles :

        t->is_fan_filename() ?
            ast::section_type::filename :
        t->is_fan_dirname() ?
            ast::section_type::dirname :
            ast::section_type::invalid
    ));
    return open_scope(t, pn, n);
}

inline bool open_fan_fn(ast::token& t, ast::node_t* pn)
{
    if(!t->is_fan_fn())
        return false;
    
    ast::fan_fn_node n = fan_fn_node(new fan_fn_node_t(
        t->is_fan_render() ?
            ast::section_type::render :
        t->is_fan_set() ?
            ast::section_type::set :
        t->is_fan_get() ?
            ast::section_type::get :
        t->is_fan_fmt() ?
            ast::section_type::fmt :
        t->is_fan_get_raw() ?
            ast::section_type::get_raw :
        t->is_fan_fmt_raw() ?
            ast::section_type::fmt_raw :
        
        t->is_fan_script() ?
            ast::section_type::script :
        t->is_fan_style() ?
            ast::section_type::style :

        t->is_fan_import() ?
            ast::section_type::import :
        
            ast::section_type::invalid
    ));
    
    
    if(n->sec_type() == section_type::render){
        if(!open_scope(t, pn, n, 1, false))
            throw MD_ERR("fan_fn_node instance is NULL!");
        
        n->_done = true;
        ast::node expr_n = expr_node(new expr_node_t(
            expr_type::semicol
        ));
        return open_scope(t, n.get(), expr_n);
    }
    
    
    return open_scope(t, pn, n);
}


inline bool open_tag(ast::token& t, ast::node_t* pn)
{
    if(!t->is_tag_lt())
        return false;
    
    auto nt = t->next();
    if(!nt)
        return false;
    
    ast::tag_node n = tag_node(new tag_node_t());
    
    if(nt->is_html_void_tag())
        n->_tag_type = tag_type::html_void;
    else if(nt->is_html_tag())
        n->_tag_type = tag_type::html;
    else if(nt->is_svg_tag())
        n->_tag_type = tag_type::svg;
    else if(nt->is_mathml_tag())
        n->_tag_type = tag_type::mathml;
    else
        n->_tag_type = tag_type::unknown;
    
    n->_tag_name = nt->text();
    
    return open_scope(t, pn, n);
}

inline void close_scope(ast::token& t, ast::node_t* pn, bool ff = true)
{
    if(!ff){
        pn->parent()->parse(t);
        return;
    }
    
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
        // if(
        //     open_fan_comment(t, this) ||
        //     open_fan_directive(t, this) ||
        //     open_fan_output(t, this) ||
            
        //     open_fan_code_block(t, this) ||
        //     open_fan_control(t, this) ||
        //     open_fan_try(t, this) ||
            
        //     open_fan_funi_func(t, this) ||
            
        //     open_fan_markup(t, this) ||
            
        //     open_fan_key(t, this) ||
        //     open_fan_fn(t, this) ||
            
        //     open_tag(t, this)
        // )
        //     return;
        
        if(
            t->is_double_quote() ||
            t->is_single_quote()
        ){
            t = t->next();
            continue;
        }
        
        switch(this->sec_type()){
            case section_type::literal:{
                if(open_tag(t, this))
                    return;
                break;
            }
            case section_type::markup_html:{
                if(open_tag(t, this))
                    return;
                
                if(t->is_curly_brace_close()){
                    close_scope(t, this);
                    return;
                }
                break;
            }
            case section_type::markup_other:{
                if(this->is_markdown()){
                    if(open_tag(t, this))
                        return;
                    
                    if(open_markdown_string(t, this))
                        return;
                }
                
                if(this->is_tex() || this->is_latex()){
                    // escaped braces and backslash
                    if(t->is_backslash()){
                        if(auto nt = t->next()){
                            if(
                                nt->is_curly_brace_close() ||
                                nt->is_curly_brace_open() ||
                                nt->is_backslash()
                            ){
                                t = nt->next();
                                continue;
                            }
                        }
                    }else if(t->is_curly_brace_open()){
                        ++this->braces;
                        // md::log::info(
                        //     "++braces: {}, line: {}, col: {}",
                        //     this->braces, t->line(), t->col()
                        // );
                        t = t->next();
                        continue;
                    }else if(t->is_curly_brace_close()){
                        if(this->braces == 0){
                            close_scope(t, this);
                            return;
                        }
                        --this->braces;
                        // md::log::info(
                        //     "--braces: {}, line: {}, col: {}",
                        //     this->braces, t->line(), t->col()
                        // );
                        t = t->next();
                        continue;
                    }
                }
                
                if(t->is_curly_brace_close()){
                    close_scope(t, this);
                    return;
                }
                
                break;
            }
            
            default:
                break;
        }
        
        
        if(
            open_fan_comment(t, this) ||
            open_fan_directive(t, this) ||
            open_fan_output(t, this) ||
            
            open_fan_code_block(t, this) ||
            open_fan_control(t, this) ||
            open_fan_try(t, this) ||
            
            open_fan_funi_func(t, this) ||
            
            open_fan_markup(t, this) ||
            
            open_fan_key(t, this) ||
            open_fan_fn(t, this)
        )
            return;
        
        
        if(t) t = t->next();
    }
    
    if(this->sec_type() != section_type::literal)
        throw MD_ERR(
            "Scope is not closed, are you missing {}? "
            "line: '{}', col: '{}'",
            "a closing parenthesis, a closing brace or semicolon",
            tt->line(), tt->col()
        );
    
    this->add_token(tt);
}

inline void expr_node_t::parse(ast::token t)
{
    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    
    ast::token st = t;
    while(t){
        if(
            open_fan_comment(t, this)
            || open_fan_directive(t, this)
            || open_fan_output(t, this)
            
            //|| open_fan_code_block(t, this)
            //|| open_fan_control(t, this)
            //|| open_fan_try(t, this)
            
            //|| open_fan_funi_func(t, this)
            
            //|| open_fan_markup(t, this)
            
            || open_fan_key(t, this)
            || open_fan_fn(t, this)
            
            //|| open_tag(t, this)
            
            || open_expr(t, this)
            
            || open_code_string(t, this)
            || open_code_block(t, this)
        )
            return;
        
        if((t->is_cpp_op() && !t->is_scope_resolution()) ||
            (t->is_semicolon() && this->_type != expr_type::semicol)
        ){
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
                    if(!it->text(false, true).empty() && !it->is_eol()){
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
    
    throw MD_ERR(
        "Scope is not closed, are you missing {}? "
        "line: '{}', col: '{}'",
        "a closing parenthesis, a closing brace or semicolon",
        _dbg_line, _dbg_col
    );
}

inline void tag_node_t::parse(ast::token t)
{
    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    
    ast::token st = t;
    while(t){
        // for opening tag: <t...>, <t.../>
        if(!this->_start_completed){
            if(open_attr_string(t, this))
                return;
            
            if(this->_tag_type == tag_type::html_void){
                if(t->is_tag_gt() || t->is_tag_slash_gt()){
                    this->_start_completed = true;
                    this->_end_started = true;
                    this->_end_completed = true;
                    this->_done = true;
                    close_scope(t, this);
                    return;
                }
            
            }else if(t->is_tag_slash_gt()){
                    this->_start_completed = true;
                    this->_end_started = true;
                    this->_end_completed = true;
                    this->_done = true;
                    close_scope(t, this);
                    return;

            }else if(t->is_tag_gt()){
                this->_start_completed = true;
            }else if(
                open_fan_comment(t, this)
                //|| open_fan_directive(t, this)
                || open_fan_output(t, this)
                
                || open_fan_code_block(t, this)
                || open_fan_control(t, this)
                || open_fan_try(t, this)
                
                || open_fan_funi_func(t, this)
                || open_fan_funa_await(t, this)
                
                || open_fan_markup(t, this)
                
                || open_fan_key(t, this)
                || open_fan_fn(t, this)
                
                //|| open_tag(t, this)
                
                //|| open_expr(t, this)
                
                //|| open_code_string(t, this)
                //|| open_code_block(t, this)
                
                //|| open_control(t, this)
                //|| open_try(t, this)
            ){
                    return;
            }
            
            if(t) t = t->next();
            continue;
        }
        
        // for <t>...</t>
        if(this->_start_completed &&
            !this->_end_started &&
            !this->_end_completed
        ){
            if(
                open_fan_comment(t, this) ||
                open_fan_directive(t, this) ||
                
                open_fan_code_block(t, this) ||
                open_fan_control(t, this) ||
                open_fan_try(t, this) ||
                
                open_fan_funi_func(t, this) ||
                
                open_fan_markup(t, this) ||
                
                open_fan_output(t, this) ||
                open_fan_key(t, this) ||
                open_fan_fn(t, this)
            )
                return;
            
            if(open_tag(t, this))
                return;
            
            if(t->is_tag_lt_slash()){
                this->_end_started = true;
            }
            
            if(t) t = t->next();
            continue;
        }
        
        // for </t>
        if(this->_start_completed &&
            this->_end_started &&
            !this->_end_completed
        ){
            if(t->is_tag_gt()){
                this->_end_completed = true;
                this->_done = true;
                close_scope(t, this);
                return;
            }
            
            if(t) t = t->next();
            continue;
        }
        
        if(t) t = t->next();
    }
    
    throw MD_ERR(
        "Tag is not closed, are you missing {}? "
        "line: '{}', col: '{}'",
        "a closing '" + _tag_name + "' tag",
        _dbg_line, _dbg_col
    );
}

inline void string_node_t::parse(ast::token t)
{
    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    
    ast::token st = t;
    ast::node p = this->parent();
    while(t){
        if(this->_escape_char.empty()){
            // if(this->_enclosing_char == t->text()){
            //     close_scope(t, this);
            //     return;
            // }
            bool is_valid = false;
            
            if(boost::starts_with(this->_enclosing_char, t->text())){
                std::string tesc = t->text();
                auto nt = t;
                while(nt &&
                    boost::starts_with(this->_enclosing_char, tesc)
                ){
                    if(this->_enclosing_char == tesc){
                        is_valid = true;
                        t = nt;
                        break;
                    }
                    nt = nt->next();
                    if(nt)
                        tesc += nt->text();
                }
            }
            
            if(is_valid){
                close_scope(t, this);
                return;
            }
            
            
        }else{
            bool is_escaped = false;
            if(boost::starts_with(this->_escape_char, t->text())){
                std::string tesc = t->text();
                auto nt = t;
                while(nt && boost::starts_with(this->_escape_char, tesc)){
                    if(this->_escape_char == tesc){
                        is_escaped = true;
                        t = nt;
                        break;
                    }
                    nt = nt->next();
                    if(nt)
                        tesc += nt->text();
                }
                
            }
            
            if(!is_escaped){
                bool is_valid = false;
                
                if(boost::starts_with(this->_enclosing_char, t->text())){
                    std::string tesc = t->text();
                    auto nt = t;
                    while(nt &&
                        boost::starts_with(this->_enclosing_char, tesc)
                    ){
                        if(this->_enclosing_char == tesc){
                            is_valid = true;
                            t = nt;
                            break;
                        }
                        nt = nt->next();
                        if(nt)
                            tesc += nt->text();
                    }
                }
                
                if(is_valid){
                    close_scope(t, this);
                    return;
                }
            }
            
        }
        
        // only verify open_scope if w'ere in a literal section
        if(p && 
            (p->node_type() == node_type::literal ||
            p->node_type() == node_type::tag)
        ){
            if(
                open_fan_output(t, this) ||
                open_fan_key(t, this) ||
                open_fan_fn(t, this)
            )
                return;
        }
        
        if(t) t = t->next();
    }

    throw MD_ERR(
        "String is not closed, are you missing a '{}'? "
        "line: '{}', col: '{}'",
        this->_enclosing_char, _dbg_line, _dbg_col
    );
    
}

inline void directive_node_t::parse(ast::token t)
{
    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    
    if(!this->parent() || this->parent()->sec_type() != section_type::literal)
        throw MD_ERR(
            "Directive can only be defined at top level scope!\n"
            "line: {}, col: {}",
            _dbg_line, _dbg_col
        );
    
    if(_done){
        close_scope(t, this, false);
        return;
    }
    
    ast::token st = t;
    while(t){
        switch(this->sec_type()){
            case ast::section_type::dir_ns:
            case ast::section_type::dir_path:
            case ast::section_type::dir_name:
            case ast::section_type::dir_layout:
            case ast::section_type::dir_include:
                if(t->is_eol()){
                    _done = true;
                    close_scope(t, this);
                    return;
                }
                if(t->is_fan_comment()){
                    _done = t->is_fan_line_comment();
                    open_fan_comment(t, this);
                    return;
                }
                break;
            
            case ast::section_type::dir_header:
            case ast::section_type::dir_inherits:
                if(!t->is_whitespace()){
                    if(t->is_curly_brace_open()){
                        _done = true;
                        open_code_block(t, this);
                        return;
                    }
                    throw MD_ERR(
                        "Open curly brace is required, line: '{}', col: '{}'",
                        t->line(), t->col()
                    );
                }
                break;
                
            default:
                break;
        }
        
        if(t)
            t = t->next();
    }

    throw MD_ERR(
        "Directive is not closed, are you missing {}? "
        "line: '{}', col: '{}'",
        "a closing brace",
        _dbg_line, _dbg_col
    );
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
    close_scope(t, this);
}

inline void output_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this, false);
        return;
    }
    
    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    
    ast::token st = t;
    while(t){
        if(t->is_semicolon()){
            close_scope(t, this);
            return;
        }
        
        if(t)
            t = t->next();
    }
    throw MD_ERR(
        "Output is not closed, are you missing a ';'? "
        "line: '{}', col: '{}'",
        _dbg_line, _dbg_col
    );
}

inline void code_block_node_t::parse(ast::token t)
{
    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    ast::token st = t;
    while(t){
        if(
            open_fan_comment(t, this)
            || open_fan_directive(t, this)
            //|| open_fan_output(t, this)
            
            //|| open_fan_code_block(t, this)
            //|| open_fan_control(t, this)
            //|| open_fan_try(t, this)
            
            //|| open_fan_funi_func(t, this)
            || open_fan_funa_await(t, this)
            
            || open_fan_markup(t, this)
            
            || open_fan_key(t, this)
            || open_fan_fn(t, this)
            
            //|| open_tag(t, this)
            
            || open_expr(t, this)
            
            || open_code_string(t, this)
            || open_code_block(t, this)
            
            || open_control(t, this)
            || open_try(t, this)
        )
            return;
        //if(open_scope(t, this))
        //    return;
        
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
                        if(!it->text(false, true).empty() && !it->is_eol()){
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
                    if(!it->text(false, true).empty() && !it->is_eol()){
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
    
    throw MD_ERR(
        "Code block is not closed, are you missing {}? "
        "line: '{}', col: '{}'",
        "a closing brace", _dbg_line, _dbg_col
    );
}

inline void code_control_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this);
        return;
    }

    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    
    ast::token st = t;
    while(t){
        if(!t->text(false, true).empty() && !t->is_eol()
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
                    open_expr(t, this);
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
                    open_code_block(t, this);
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
    
    throw MD_ERR(
        "Incomplete control statement, "
        "line: '{}', col: '{}'",
        _dbg_line, _dbg_col
    );
}

inline void code_err_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this);
        return;
    }
    
    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    
    ast::token st = t;
    while(t){
        if(!t->text(false, true).empty() && !t->is_eol() && !t->is_cpp_catch()){

            if(_need_expr){
                if(t->is_parenthesis_open()){
                    _need_expr = false;
                    open_expr(t, this);
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
                    open_code_block(t, this);
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
    
    throw MD_ERR(
        "Incomplete try catch statement, are you missing the catch statement? "
        "line: '{}', col: '{}'",
        _dbg_line, _dbg_col
    );
}

inline void code_fun_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this);
        return;
    }

    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    
    ast::token st = t;
    while(t){
        if(!t->text(false, true).empty() && !t->is_eol()){
            if(t->is_curly_brace_open()){
                _done = true;
                open_code_block(t, this);
                return;
            }
            throw MD_ERR(
                "Open curly brace is required, line: '{}', col: '{}'",
                t->line(), t->col()
            );
        }
        
        if(t) t = t->next();
    }
    
    throw MD_ERR(
        "Open curly brace is required, line: '{}', col: '{}'",
        _dbg_line, _dbg_col
    );
}

inline void code_async_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this);
        return;
    }
    
    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
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
        if(!t->text(false, true).empty() && !t->is_eol()
        ){
            if(opened_tags > 0){
                if(t->is_gt())
                    --opened_tags;
                else if(t->is_lt())
                    ++opened_tags;
                
            }else if(_need_expr){
                if(t->is_parenthesis_open()){
                    _need_expr = false;
                    open_expr(t, this);
                    return;
                }
                throw MD_ERR(
                    "Expression required, line: '{}', col: '{}'",
                    t->line(), t->col()
                );
            }else if(_need_code){
                if(t->is_curly_brace_open()){
                    _need_code = false;
                    open_code_block(t, this);
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


inline void fan_key_node_t::parse(ast::token t)
{
    close_scope(t, this, false);
}

inline void fan_fn_node_t::parse(ast::token t)
{
    if(_done){
        close_scope(t, this, false);
        return;
    }
    
    if(_dbg_line == 0){
        _dbg_line = t->line();
        _dbg_col = t->col();
    }
    
    ast::token st = t;
    while(t){
        switch(this->sec_type()){
            case ast::section_type::render:
                if(t->is_semicolon()){
                    _done = true;
                    close_scope(t, this);
                    return;
                }
                break;
            
            case ast::section_type::set:
            case ast::section_type::get:
            case ast::section_type::fmt:
            case ast::section_type::get_raw:
            case ast::section_type::fmt_raw:
            
            case ast::section_type::script:
            case ast::section_type::style:
            
            case ast::section_type::import:
            
                if(!t->is_whitespace()){
                    if(t->is_parenthesis_open()){
                        _done = true;
                        open_expr(t, this);
                        return;
                    }
                    throw MD_ERR(
                        "Open parenthesis is required, line: '{}', col: '{}'",
                        t->line(), t->col()
                    );
                }
                break;
                
            default:
                break;
        }
        
        if(t)
            t = t->next();
    }
    
    throw MD_ERR(
        "Incomplete function, are you missing a '('? "
        "line: '{}', col: '{}'",
        _dbg_line, _dbg_col
    );
    
}

inline void token_node_t::parse(ast::token t)
{
    throw MD_ERR("Can't parse a token node!");
}


}}}//::evmvc::fanjet::ast