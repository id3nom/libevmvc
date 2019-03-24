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

#ifndef _libevmvc_fanjet_h
#define _libevmvc_fanjet_h

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
    root            = INT_MIN -1,
    token           = INT_MIN -2,
    expr            = INT_MIN -3,
    string          = INT_MIN -4,
    
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

        case node_type::directive:
            return "directive";
        case node_type::literal:
            return "literal";
        case node_type::comment:
            return "comment";
        case node_type::output:
            return "output";
        case node_type::code_block:
            return "code_block";
        case node_type::code_control:
            return "code_control";
        case node_type::code_err:
            return "code_err";
        case node_type::code_fun:
            return "code_fun";
        case node_type::code_async:
            return "code_async";
    }
}

enum class sibling_pos
{
    prev,
    next
};


class node_t
    : public std::enable_shared_from_this<node_t>
{
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
        _root(parent->root()),
        _parent(parent),
        _prev(prev),
        _token(nullptr),
        _next(next)
    {
        if((int)node_type::invalid & (int)sec_type)
            _node_type = node_type::invalid;
        else if((int)node_type::root & (int)sec_type)
            _node_type = node_type::root;
        else if((int)node_type::expr & (int)sec_type)
            _node_type = node_type::expr;

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
        _root(parent->root()),
        _parent(parent),
        _prev(prev),
        _token(nullptr),
        _next(next)
    {
        if(!((int)n_type && (int)sec_type))
            throw MD_ERR("section_type and node_type mismatch!");
    }


public:
    ast::node_type node_type() const { return _node_type;}
    ast::section_type sec_type() const { return _sec_type;}
    bool is_root() const { return node_type() == node_type::root;}
    node root() const { return _root.lock();}
    node parent() const { return _parent.lock();}
    node prev() const { return _prev.lock();}
    node next() const { return _next.lock();}
    
    virtual bool prev_sibling_allowed() const { return true;}
    virtual bool next_sibling_allowed() const { return true;}
    
    ast::token token() const { return _token;}
    
    const std::vector<node>& childs() const { return _childs;}
    std::vector<node>& childs() { return _childs;}
    size_t child_count() const { return _childs.size();}
    node child(size_t idx) const { return _childs[idx];}

    size_t line() const { return _token ? _token->line() : 0;}
    size_t pos() const { return  _token ? _token->pos() : 0;}
    std::string token_text() const
    {
        if(!_token)
            return "";
        
        std::string text;
        ast::token t = _token;
        while(t){
            text += t->text();
            t = _token->next();
        }
        
        return text;
    }
    
    std::string token_section_text() const
    {
        std::string text = token_text();
        for(auto n : _childs)
            text += n->token_section_text();
        return text;
    }
    
    void add_sibling(sibling_pos pos, node n)
    {
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
        }
        
        n->_root = this->_root;
        n->_parent = this->_parent;
    }
    
    void add_child(node n)
    {
        n->remove();
        if(this->_childs.empty()){
            n->_root = this->_root;
            n->_parent = this->shared_from_this();
            this->_childs.emplace_back(n);
            return;
        }
        
        this->_childs[this->_childs.size()]->add_sibling(
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
    
protected:
    
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
    std::weak_ptr<node_t> _next;
    
    ast::token _token;
    
    std::vector<node> _childs;
};

root_node parse(ast::token t);
class root_node_t
    : public node_t
{
private:
    friend root_node parse(ast::token t);
    
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
    root_node r = std::make_shared<root_node_t>();
    r->parse(t);
    return r;
}

#define EVMVC_FANJET_NODE_FRIENDS friend class node_t; \
    friend class root_node_t; \
    friend class directive_node_t; \
    friend class literal_node_t; \
    friend class comment_node_t; \
    friend class output_node_t; \
    friend class clode_block_node_t; \
    friend class code_control_node_t; \
    friend class code_err_node_t; \
    friend class code_fun_node_t; \
    friend class code_async_node_t; \
    friend bool open_scope(ast::token& t, ast::node_t* n); \
    friend void close_scope(ast::token& t, ast::node_t* pn); \
    

class token_node_t
    : public node_t
{
protected:
    token_node_t(
        ast::token t
        )
        : node_t(ast::section_type::token , nullptr, nullptr, nullptr)
    {
    }
protected:
    ast::token parse(ast::token t)
    {
        throw MD_ERR("Can't parse a token node!");
        //this->_token = t;
    }
};


class expr_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    expr_node_t(
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::section_type::expr , parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
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
        : node_t(node_type::expr, dir_type, parent, prev, next)
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
    EVMVC_FANJET_NODE_FRIENDS
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
        : node_t(ast::node_type::code_control, t, parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
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
        : node_t(ast::node_type::code_err, t, parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
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
        : node_t(ast::node_type::code_fun, t, parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
};

class code_async_node_t
    : public node_t
{
    EVMVC_FANJET_NODE_FRIENDS
protected:
    code_async_node_t(
        ast::section_type t,
        node parent = nullptr,
        node prev = nullptr,
        node next = nullptr)
        : node_t(ast::node_type::code_async, t, parent, prev, next)
    {
    }

public:
    
    
protected:
    void parse(ast::token t);

private:
    
};

}}}//::evmvc::fanjet::ast
#include "fan_ast_impl.h"

#endif //_libevmvc_fanjet_h
