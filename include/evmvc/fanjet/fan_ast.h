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

#ifndef _libevmvc_fanjet_ast_h
#define _libevmvc_fanjet_ast_h

#include "../stable_headers.h"
#include "fan_common.h"
#include "fan_tokenizer.h"

#include <stack>

namespace evmvc { namespace fanjet { namespace ast {

class node_t;
typedef std::shared_ptr<node_t> node;
class root_node_t;
typedef std::shared_ptr<root_node_t> root_node;
class token_node_t;
typedef std::shared_ptr<token_node_t> token_node;
class expr_node_t;
typedef std::shared_ptr<expr_node_t> expr_node;
class string_node_t;
typedef std::shared_ptr<string_node_t> string_node;

class directive_node_t;
typedef std::shared_ptr<directive_node_t> directive_node;
class literal_node_t;
typedef std::shared_ptr<literal_node_t> literal_node;
class comment_node_t;
typedef std::shared_ptr<comment_node_t> comment_node;
class output_node_t;
typedef std::shared_ptr<output_node_t> output_node;
class code_block_node_t;
typedef std::shared_ptr<code_block_node_t> code_block_node;
class code_control_node_t;
typedef std::shared_ptr<code_control_node_t> code_control_node;
class code_err_node_t;
typedef std::shared_ptr<code_err_node_t> code_err_node;
class code_fun_node_t;
typedef std::shared_ptr<code_fun_node_t> code_fun_node;
class code_async_node_t;
typedef std::shared_ptr<code_async_node_t> code_async_node;

enum class node_type
{
    invalid         = INT_MIN,
    root            = INT_MIN +1,
    token           = INT_MIN +2,
    expr            = INT_MIN +3,
    string          = INT_MIN +4,
    any             = INT_MIN +5,
    
    directive       =
        (int)(
            section_type::dir_ns |
            section_type::dir_name |
            section_type::dir_layout |
            section_type::dir_header |
            section_type::dir_inherits
        ),
    literal         = 
        (int)(
            section_type::literal |
            section_type::markup_html |
            section_type::markup_markdown
        ),
    comment         =
        (int)(
            section_type::comment_line |
            section_type::comment_block |
            section_type::region_start |
            section_type::region_end
        ),
    output          =
        (int)(
            section_type::output_raw |
            section_type::output_enc
        ),
    code_block      = (int)section_type::code_block,
    code_control    =
        (int)(
            section_type::code_if |
            section_type::code_elif |
            section_type::code_else |
            section_type::code_switch |
            section_type::code_while |
            section_type::code_for |
            section_type::code_do |
            section_type::code_dowhile
        ),
    code_err        =
        (int)(
            section_type::code_try |
            section_type::code_trycatch
        ),
    code_fun        =
        (int)(
            section_type::code_funi |
            section_type::code_func
        ),
    code_async      =
        (int)(
            section_type::code_funa |
            section_type::code_await
        )
};
inline md::string_view to_string(node_type t)
{
    switch(t){
        case node_type::invalid:
            return "invalid";
        case node_type::root:
            return "root";
        case node_type::token:
            return "token";
        case node_type::expr:
            return "expr";
        case node_type::string:
            return "string";
        case node_type::any:
            return "any";

        case node_type::directive:
            return "directive";
        case node_type::literal:
            return "literal";
        case node_type::comment:
            return "comment";
        case node_type::output:
            return "output";
        case node_type::code_block:
            return "code block";
        case node_type::code_control:
            return "control";
        case node_type::code_err:
            return "error";
        case node_type::code_fun:
            return "function";
        case node_type::code_async:
            return "async";
            
        default:
            return "UNKNOWN";
    }
}

enum class sibling_pos
{
    prev,
    next
};
inline md::string_view to_string(sibling_pos p)
{
    switch(p){
        case sibling_pos::prev:
            return "prev";
        case sibling_pos::next:
            return "next";
        default:
            return "UNKNOWN";
    }
}


class node_t
    : public std::enable_shared_from_this<node_t>
{
    friend class code_control_node_t;
    friend class code_async_node_t;
    
    friend bool open_scope(ast::token& t, ast::node_t* n);
    friend void close_scope(ast::token& t, ast::node_t* pn);
    
protected:
    node_t(
        ast::section_type sec_type,
        node parent,
        node prev,
        node next = nullptr)
        : 
        _sec_type(sec_type),
        _node_type(ast::node_type::invalid),
        _root(parent ? parent->root() : nullptr),
        _parent(parent),
        _prev(prev),
        _token(nullptr),
        _next(next)
    {
        if((int)node_type::invalid == (int)sec_type)
            _node_type = node_type::invalid;
        else if((int)node_type::root == (int)sec_type)
            _node_type = node_type::root;
        else if((int)node_type::token == (int)sec_type)
            _node_type = node_type::token;
        else if((int)node_type::expr == (int)sec_type)
            _node_type = node_type::expr;
        else if((int)node_type::string == (int)sec_type)
            _node_type = node_type::string;

        else if((int)node_type::directive & (int)sec_type)
            _node_type = node_type::directive;
        else if((int)node_type::literal & (int)sec_type)
            _node_type = node_type::literal;
        else if((int)node_type::comment & (int)sec_type)
            _node_type = node_type::comment;
        else if((int)node_type::output & (int)sec_type)
            _node_type = node_type::output;

        else if((int)node_type::code_block & (int)sec_type)
            _node_type = node_type::code_block;
        else if((int)node_type::code_control & (int)sec_type)
            _node_type = node_type::code_control;
        else if((int)node_type::code_fun & (int)sec_type)
            _node_type = node_type::code_fun;
        else if((int)node_type::code_async & (int)sec_type)
            _node_type = node_type::code_async;
        else if((int)node_type::code_err & (int)sec_type)
            _node_type = node_type::code_err;
        else
            _node_type = node_type::invalid;
            
        EVMVC_DEF_TRACE(
            "creating node: '{}', sec: '{}'",
            to_string(this->node_type()), to_string(this->sec_type())
        );
    }

    node_t(
        ast::node_type n_type,
        ast::section_type sec_type,
        node parent,
        node prev,
        node next = nullptr)
        : 
        _sec_type(sec_type),
        _node_type(n_type),
        _root(parent ? parent->root() : nullptr),
        _parent(parent),
        _prev(prev),
        _token(nullptr),
        _next(next)
    {
        EVMVC_DEF_TRACE(
            "creating node: '{}', sec: '{}'",
            to_string(this->node_type()), to_string(this->sec_type())
        );
        
        if(!((int)n_type && (int)sec_type))
            throw MD_ERR("section_type and node_type mismatch!");
    }


public:
    size_t id() const
    {
        if(is_root())
            return 1;
        auto p = this->parent();
        for(size_t i = 0; i < p->_childs.size(); ++i)
            if(p->_childs[i].get() == this)
                return i +1;
        return 0;
    }
    std::string path() const
    {
        std::string ps = md::num_to_str(this->id(), false);
        node p = this->parent();
        while(p){
            ps = md::num_to_str(p->id(), false) + "." + ps;
            p = p->parent();
        }
        return ps;
    }
    
    ast::node_type node_type() const { return _node_type;}
    ast::section_type sec_type() const { return _sec_type;}
    bool is_root() const { return node_type() == node_type::root;}
    node root() const { return _root.lock();}
    
    virtual bool prev_sibling_allowed() const { return true;}
    virtual bool next_sibling_allowed() const { return true;}
    
    ast::token token() const { return _token;}
    
    const std::vector<node>& childs() const { return _childs;}
    std::vector<node>& childs() { return _childs;}
    size_t child_count() const { return _childs.size();}
    node child(size_t idx) const { return _childs[idx];}
    
    node parent(ast::node_type t = ast::node_type::any) const
    {
        if(t == ast::node_type::any)
            return _parent.lock();
        
        node n = _parent.lock();
        while(n && n->_node_type != t)
            n = n->_parent.lock();
        return n;
    }
    node prev(ast::node_type t = ast::node_type::any) const
    {
        if(t == ast::node_type::any)
            return _prev.lock();
        
        auto n = _prev.lock();
        while(n && n->_node_type != t)
            n = n->prev();
        return n;
    }
    node next(ast::node_type t = ast::node_type::any) const
    {
        if(t == ast::node_type::any)
            return _next.lock();

        auto n = _next.lock();
        while(n && n->_node_type != t)
            n = n->next();
        return n;
    }
    
    bool is_first_sibling(ast::node_type t = ast::node_type::any) const
    {
        if(t == ast::node_type::any)
            return _prev.expired();
        
        if(this->_node_type != t)
            return false;
        if(_prev.expired())
            return true;
        node n = this->_prev.lock();
        while(n){
            if(n->_node_type == t)
                return false;
            n = n->prev();
        }
        return true;
    }
    node first_sibling(ast::node_type t = ast::node_type::any) const
    {
        if(t == ast::node_type::any){
            if(_prev.expired())
                return this->_shared_from_this();
        
            node n = this->prev();
            while(!n->_prev.expired())
                n = n->prev();
            return n;
        }
        
        node tn = _node_type == t ? this->_shared_from_this() : nullptr;
        node n = this->prev();
        while(n && !n->_prev.expired()){
            if(n->_node_type == t)
                tn = n;
            n = n->prev();
        }
        return tn;
    }
    bool is_last_sibling(ast::node_type t = ast::node_type::any) const
    {
        if(t == ast::node_type::any)
            return _next.expired();
        
        if(this->_node_type != t)
            return false;
        if(_next.expired())
            return true;
        node n = this->_next.lock();
        while(n){
            if(n->_node_type == t)
                return false;
            n = n->next();
        }
        return true;
    }
    node last_sibling(ast::node_type t = ast::node_type::any) const
    {
        if(t == ast::node_type::any){
            if(_next.expired())
                return this->_shared_from_this();
            
            node n = this->next();
            while(!n->_next.expired())
                n = n->next();
            return n;
        }
        
        node tn = _node_type == t ? this->_shared_from_this() : nullptr;
        node n = this->next();
        while(n && !n->_next.expired()){
            if(n->_node_type == t)
                tn = n;
            n = n->next();
        }
        return tn;
    }
    node first_child(ast::node_type t = ast::node_type::any) const
    {
        if(t == ast::node_type::any)
            return _childs.size() > 0 ? _childs[0] : nullptr;
        
        node n = _childs.size() > 0 ? _childs[0] : nullptr;
        while(n && n->_node_type != t)
            n = n->next();
        return n;
    }
    node last_child(ast::node_type t = ast::node_type::any) const
    {
        if(t == ast::node_type::any)
            return _childs.size() > 0 ? _childs[_childs.size() -1] : nullptr;
        
        node n = _childs.size() > 0 ? _childs[_childs.size() -1] : nullptr;
        while(n && n->_node_type != t)
            n = n->next();
        return n;
    }
    
    
    node parent(section_type t) const
    {
        if(t == section_type::any)
            return _parent.lock();
        
        node n = _parent.lock();
        while(n && n->_sec_type != t)
            n = n->_parent.lock();
        return n;
    }
    node prev(section_type t) const
    {
        if(t == section_type::any)
            return _prev.lock();
        
        auto n = _prev.lock();
        while(n && n->_sec_type != t)
            n = n->prev();
        return n;
    }
    node next(section_type t) const
    {
        if(t == section_type::any)
            return _next.lock();

        auto n = _next.lock();
        while(n && n->_sec_type != t)
            n = n->next();
        return n;
    }
    
    bool is_first_sibling(section_type t) const
    {
        if(t == section_type::any)
            return _prev.expired();
        
        if(this->_sec_type != t)
            return false;
        if(_prev.expired())
            return true;
        node n = this->_prev.lock();
        while(n){
            if(n->_sec_type == t)
                return false;
            n = n->prev();
        }
        return true;
    }
    node first_sibling(section_type t) const
    {
        if(t == section_type::any){
            if(_prev.expired())
                return this->_shared_from_this();
        
            node n = this->prev();
            while(!n->_prev.expired())
                n = n->prev();
            return n;
        }
        
        node tn = _sec_type == t ? this->_shared_from_this() : nullptr;
        node n = this->prev();
        while(n && !n->_prev.expired()){
            if(n->_sec_type == t)
                tn = n;
            n = n->prev();
        }
        return tn;
    }
    bool is_last_sibling(section_type t) const
    {
        if(t == section_type::any)
            return _next.expired();
        
        if(this->_sec_type != t)
            return false;
        if(_next.expired())
            return true;
        node n = this->_next.lock();
        while(n){
            if(n->_sec_type == t)
                return false;
            n = n->next();
        }
        return true;
    }
    node last_sibling(section_type t) const
    {
        if(t == section_type::any){
            if(_next.expired())
                return this->_shared_from_this();
            
            node n = this->next();
            while(!n->_next.expired())
                n = n->next();
            return n;
        }
        
        node tn = _sec_type == t ? this->_shared_from_this() : nullptr;
        node n = this->next();
        while(n && !n->_next.expired()){
            if(n->_sec_type == t)
                tn = n;
            n = n->next();
        }
        return tn;
    }
    node first_child(section_type t) const
    {
        if(t == section_type::any)
            return _childs.size() > 0 ? _childs[0] : nullptr;
        
        node n = _childs.size() > 0 ? _childs[0] : nullptr;
        while(n && n->_sec_type != t)
            n = n->next();
        return n;
    }
    node last_child(section_type t) const
    {
        if(t == section_type::any)
            return _childs.size() > 0 ? _childs[_childs.size() -1] : nullptr;
        
        node n = _childs.size() > 0 ? _childs[_childs.size() -1] : nullptr;
        while(n && n->_sec_type != t)
            n = n->next();
        return n;
    }

    
    size_t line() const { return _token ? _token->line() : 0;}
    size_t col() const { return _token ? _token->col() : 0;}
    size_t pos() const { return  _token ? _token->pos() : 0;}
    std::string token_text() const
    {
        if(!_token)
            return "";
        
        return _token->text(true);
    }
    
    std::string token_section_text(
        bool line_number = false) const
    {
        std::string text = token_text();
        for(auto n : _childs)
            text += n->token_section_text();
        
        if(line_number){
            std::vector<std::string> lines;
            boost::algorithm::split(
                lines, text, boost::is_any_of("\n")
            );
            std::string fmt_str = 
                "{:>" + 
                md::num_to_str(
                    md::num_to_str(lines.size(), false).size(),
                    false
                ) + "} {}{}";
            text = "";
            for(size_t i = 0; i < lines.size(); ++i)
                text += fmt::format(
                    fmt_str.c_str(),
                    i+1, lines[i], i < lines.size()-1 ? "\n" : ""
                );
        }
        return text;
    }
    
    void add_sibling(sibling_pos pos, node n)
    {
        EVMVC_DEF_TRACE(
            "node: '{}', sec: '{}', adding node sibling\n"
            "  node: '{}', sec: '{}', pos: '{}'",
            to_string(this->node_type()), to_string(this->sec_type()),
            to_string(n->node_type()), to_string(n->sec_type()), to_string(pos)
        );

        // find cur pos
        auto pos_it = this->parent()->_childs.end();
        for(auto it = this->parent()->_childs.begin();
            it != this->parent()->_childs.end(); ++it
        )
            if((*it).get() == this){
                pos_it = it;
                break;
            }
        if(pos_it == this->parent()->_childs.end())
            throw MD_ERR("Invalid position");
        
        if(pos == sibling_pos::prev){
            if(!this->prev_sibling_allowed())
                throw MD_ERR(
                    "No 'prev' siblings allowed for '{}' node",
                    to_string(this->node_type())
                );
            
            node sib = prev();
            n->_prev = sib;
            if(sib)
                sib->_next = n;
            n->_next = this->shared_from_this();
            this->_prev = n;
            
        }else if(pos == sibling_pos::next){
            if(!this->next_sibling_allowed())
                throw MD_ERR(
                    "No 'next' siblings allowed for '{}' node",
                    to_string(this->node_type())
                );
            
            node sib = next();
            n->_next = sib;
            if(sib)
                sib->_prev = n;
            n->_prev = this->shared_from_this();
            this->_next = n;
            
            ++pos_it;
        }
        
        n->_root = this->_root;
        n->_parent = this->_parent;
        
        this->parent()->_childs.emplace(pos_it, n);
    }
    
    void add_child(node n)
    {
        EVMVC_DEF_TRACE(
            "node: '{}', sec: '{}', adding node child\n"
            "  node: '{}', sec: '{}'",
            to_string(this->node_type()), to_string(this->sec_type()),
            to_string(n->node_type()), to_string(n->sec_type())
        );
        
        n->remove();
        if(this->_childs.empty()){
            n->_root = this->_root.expired() ?
                this->shared_from_this() : this->_root;
            n->_parent = this->shared_from_this();
            this->_childs.emplace_back(n);
            return;
        }
        
        this->_childs[this->_childs.size() -1]->add_sibling(
            sibling_pos::next, n
        );
    }
    
    void remove()
    {
        auto p = _prev.lock();
        auto n = _next.lock();
        
        if(p)
            p->_next = n;
        if(n)
            n->_prev = p;
        
        // remove self from parent
        if(auto o = _parent.lock())
            for(auto it = o->_childs.begin(); it != o->_childs.end(); ++it)
                if((*it).get() == this){
                    o->_childs.erase(it);
                    break;
                }
        
        this->_root.reset();
        this->_parent.reset();
        this->_prev.reset();
        this->_next.reset();
    }
    
    std::string dump() const
    {
        std::string text;
        text += fmt::format(
            "#start {} '{}' - type: '{}'{}\n",
            path(),
            to_string(this->node_type()),
            to_string(this->sec_type()),
            _token ?
                fmt::format(
                    ", line: '{}', col: '{}', pos: '{}'",
                    this->line(), this->col(), this->pos()
                ):
                ""
        );
        
        std::string tt = md::replace_substring_copy(
            token_text(),
            "\n", "\n  "
        );
        if(!tt.empty()){
            tt = "  " + tt;
            if(tt[tt.size()-1] != '\n')
                tt += "\n";
        }
        text += tt;
        
        for(auto n : _childs){
            text += n->dump();
        }
        
        text += fmt::format(
            "#end {} '{}' - type: '{}'",
            path(),
            to_string(this->node_type()),
            to_string(this->sec_type())
        );
        
        if(this->is_root())
            return text + "\n";
        
        return "|-" + md::replace_substring_copy(text, "\n", "\n| ") + "\n";
    }
    
    
protected:
    node _shared_from_this() const
    {
        if(is_root())
            return nullptr;
        
        node p = this->parent();
        for(size_t i = 0; i < p->_childs.size(); ++i)
            if(p->_childs[i].get() == this)
                return p->_childs[i];
        return nullptr;
    }
    
    void add_sibling_token(sibling_pos pos, ast::token t);
    void add_token(ast::token t);
    
    /**
     * parse the token list and should return unprocessed (snipped) token list
     */
    virtual void parse(ast::token t) = 0;
    
private:
    ast::section_type _sec_type;
    ast::node_type _node_type;
    
    std::weak_ptr<node_t> _root;
    std::weak_ptr<node_t> _parent;
    
    std::weak_ptr<node_t> _prev;
    ast::token _token;
    std::weak_ptr<node_t> _next;
    
    std::vector<node> _childs;
};

root_node parse(ast::token t);
class root_node_t
    : public node_t
{
    friend root_node ast::parse(ast::token t);

    root_node_t()
        : node_t(ast::section_type::root, nullptr, nullptr)
    {
    }
    
public:
    
    bool prev_sibling_allowed() const { return false;}
    bool next_sibling_allowed() const { return false;}
    
protected:
    void parse(ast::token t);

private:
    
};
inline root_node parse(ast::token t)
{
    root_node r = root_node(new root_node_t());
    r->parse(t);
    return r;
}

#define EVMVC_FANJET_NODE_FRIENDS \
    friend class node_t; \
    friend class root_node_t; \
    friend class code_block_node_t; \
    friend bool open_scope(ast::token& t, ast::node_t* n); \
    friend void close_scope(ast::token& t, ast::node_t* pn); \
    
class token_node_t
    : public node_t
{
    friend class node_t;
    
protected:
    token_node_t()
        : node_t(ast::section_type::token , nullptr, nullptr, nullptr)
    {
    }
protected:
    void parse(ast::token t)
    {
        throw MD_ERR("Can't parse a token node!");
        //this->_token = t;
    }
};

enum class expr_type
{
    parens      = 1,
    semicol     = 2
};

class expr_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
    
protected:
    expr_node_t(
        expr_type type,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::section_type::expr, parent, prev, next),
        _type(type)
    {
    }
    
    expr_type type() const { return _type;}

public:
    
    
protected:
    void parse(ast::token t);

private:
    expr_type _type;
};

class string_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    string_node_t(
        std::string enclosing_char,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::section_type::string , parent, prev, next),
        _enclosing_char(enclosing_char)
    {
    }

public:
    std::string enclosing_char() const
    {
        return _enclosing_char;
    }
    
protected:
    void parse(ast::token t);

private:
    std::string _enclosing_char;
};


class directive_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    directive_node_t(
        ast::section_type dir_type,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(node_type::directive, dir_type, parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
};

class literal_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    literal_node_t(
        ast::section_type t,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(node_type::literal, t, parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
};

class comment_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    comment_node_t(
        ast::section_type t,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::node_type::comment, t, parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
};

class output_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    output_node_t(
        ast::section_type t,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::node_type::output, t, parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
};

class code_block_node_t
    : public node_t
{
    friend class node_t;
    friend class root_node_t;
    friend bool open_scope(ast::token& t, ast::node_t* n);
    friend void close_scope(ast::token& t, ast::node_t* pn);
    
protected:
    code_block_node_t(
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::section_type::code_block, parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
};

class code_control_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    code_control_node_t(
        ast::section_type t,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::node_type::code_control, t, parent, prev, next),
        _need_expr(true),
        _need_code(true),
        _need_semicol(t == ast::section_type::code_do),
        _done(false)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    bool _need_expr;
    bool _need_code;
    bool _need_semicol;
    bool _done;
};

class code_err_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    code_err_node_t(
        ast::section_type t,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::node_type::code_err, t, parent, prev, next),
        _need_expr(false),
        _need_code(true),
        _done(false)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    bool _need_expr;
    bool _need_code;
    bool _done;
};

class code_fun_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    code_fun_node_t(
        ast::section_type t,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::node_type::code_fun, t, parent, prev, next),
        _done(false)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    bool _done;
};

class code_async_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
    friend class expr_node_t;
    
protected:
    code_async_node_t(
        ast::section_type t,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::node_type::code_async, t, parent, prev, next),
        _need_expr(true),
        _need_code(false),
        _need_semicol(t == ast::section_type::code_await),
        _done(false),
        _async_type("")
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    bool _need_expr;
    bool _need_code;
    bool _need_semicol;
    bool _done;
    
    std::string _async_type;
};

}}}//::evmvc::fanjet::ast
#include "fan_ast_impl.h"

#endif //_libevmvc_fanjet_ast_h
