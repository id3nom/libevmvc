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

namespace evmvc { namespace fanjet {

class ast_node_t;
typedef std::shared_ptr<ast_node_t> ast_node;

class ast_tree_t;
typedef std::shared_ptr<ast_tree_t> ast_tree;

enum class ast_node_type
{
    root,
    literal,
    code,
};
inline md::string_view to_string(ast_node_type t)
{
    switch(t){
        case ast_node_type::root:
            return "root";
        case ast_node_type::literal:
            return "literal";
        case ast_node_type::code:
            return "code";
    }
}

enum class sibling_pos
{
    prev,
    next
};

class ast_node_t
    : public std::enable_shared_from_this<ast_node_t>
{
public:
    ast_node_t(
        ast_node parent,
        ast_node prev,
        size_t line = 0,
        size_t pos = 0,
        const std::string& text = "",
        ast_node next = nullptr)
        : _root(parent->root()),
        _parent(parent),
        _prev(prev),
        _line(line),
        _pos(pos),
        _text(text),
        _next(next)
    {
    }
    
    virtual ast_node_type type() const { return ast_node_type::literal;}
    
    bool is_root() const { return type() == ast_node_type::root;}
    ast_node root() const { return _root.lock();}
    ast_node parent() const { return _parent.lock();}
    ast_node prev() const { return _prev.lock();}
    ast_node next() const { return _next.lock();}
    
    virtual bool prev_sibling_allowed() const { return true;}
    virtual bool next_sibling_allowed() const { return true;}
    
    size_t line() const { return _line;}
    size_t pos() const { return _pos;}
    std::string text() const { return _text;}
    
    const std::vector<ast_node>& childs() const { return _childs;}
    std::vector<ast_node>& childs() { return _childs;}
    
    void add_sibling(sibling_pos pos, ast_node node)
    {
        if(pos == sibling_pos::prev){
            if(!this->prev_sibling_allowed())
                throw MD_ERR(
                    "No 'prev' siblings allowed for '{}' node",
                    to_string(this->type())
                );
            
            ast_node sib = prev();
            node->_prev = sib;
            if(sib)
                sib->_next = node;
            node->_next = this->shared_from_this();
            this->_prev = node;
            
        }else if(pos == sibling_pos::next){
            if(!this->next_sibling_allowed())
                throw MD_ERR(
                    "No 'next' siblings allowed for '{}' node",
                    to_string(this->type())
                );
            
            ast_node sib = next();
            node->_next = sib;
            if(sib)
                sib->_prev = node;
            node->_prev = this->shared_from_this();
            this->_next = node;
        }
        
        node->_root = this->_root;
        node->_parent = this->_parent;
    }
    
private:
    std::weak_ptr<ast_node_t> _root;
    std::weak_ptr<ast_node_t> _parent;
    
    std::weak_ptr<ast_node_t> _prev;
    std::weak_ptr<ast_node_t> _next;
    
    size_t _line;
    size_t _pos;
    std::string _text;
    
    std::vector<ast_node> _childs;
};


class ast_tree_t
    : public ast_node_t
{
public:
    ast_tree_t()
        : ast_node_t(nullptr, nullptr)
    {
    }
    
    ast_node_type type() const { return ast_node_type::root;}
    
    bool prev_sibling_allowed() const { return false;}
    bool next_sibling_allowed() const { return false;}
    
private:
    
};




}}//::evmvc::fanjet
#endif //_libevmvc_fanjet_ast_h
