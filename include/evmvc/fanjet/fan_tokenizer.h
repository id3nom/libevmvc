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

#ifndef _libevmvc_fanjet_tokenizer_h
#define _libevmvc_fanjet_tokenizer_h

#include "../stable_headers.h"

namespace evmvc { namespace fanjet { namespace ast {
    
class token_t;
typedef std::shared_ptr<token_t> token;

class token_t
    : public std::enable_shared_from_this<token_t>
{
public:
    token_t(
        token pre,
        const std::string& text, size_t line, size_t pos,
        token next)
        : _prev(pre), _text(text), _line(line), _pos(pos), _next(next)
    {
    }
    
    ~token_t()
    {
    }
    
    bool is_root() const { return _prev.expired();}
    token root() const
    {
        token r = this->_prev.lock();
        if(!r)
            return nullptr;
        while(!r->is_root())
            r = r->prev();
        return r;
    }
    
    token prev() const { return _prev.lock();}
    
    const std::string& text() const { return _text;}
    const std::string& trim_text() const { return md::trim_copy(_text);}
    size_t line() const { return _line;}
    size_t pos() const { return _pos;}
    token next() const { return _next;}
    
    std::string norm_text();
    
    bool norm_is_empty()
    {
        return this->_text.size() > 0 && this->norm_text().size() > 0;
    }
    
    token get_norm_non_empty_prev()
    {
        auto p = this->_prev.lock();
        if(!p)
            return p;
        if(p->norm_is_empty())
            return p->get_norm_non_empty_prev();
        return p;
    }
    
    std::string debug_string()
    {
        auto p = this->_prev.lock();
        return fmt::format(
            "TOKEN: cur '{0}', prev '{1}', next '{2}'",
            this->_text,
            p ? p->_text : "NULL",
            this->_next ? this->_next->_text : "NULL"
        );
    }
    
    token add_next(const std::string& text, size_t line, size_t pos)
    {
        this->_next = std::make_shared<token_t>(
            this->shared_from_this(),
            text,
            line,
            pos
        );
        
        return this->_next;
    }
    
    /**
     * detach the current token from the token list.
     * after that call, _prev node is nullptr.
     */
    token snip()
    {
        token r = root();
        _prev.reset();
        return r;
    }
    
    
    
    
    bool is_space() const { return _text == " ";}

    bool is_cpp_line_comment() const { return _text == "//";}
    bool is_cpp_blk_comment_open() const { return _text == "/*";}
    bool is_cpp_blk_comment_close() const { return _text == "*/";}
    
    bool is_cpp_pointer() const { return _text == "->";}
    
    bool is_parenthesis_open() const { return _text == "(";}
    bool is_parenthesis_close() const { return _text == ")";}
    bool is_curly_brace_open() const { return _text == "{";}
    bool is_curly_brace_close() const { return _text == "}";}
    
    bool is_htm_ltc() const { return _text == "</";}
    bool is_htm_gtc() const { return _text == "/>";}
    bool is_lt() const { return _text == "<";}
    bool is_gt() const { return _text == ">";}
    
    bool is_fan_key() const
    {
        return
            is_fan_escape() ||
            is_fan_comment() ||
            is_fan_region() ||
            is_fan_directive() ||
            is_fan_output() ||
            is_fan_code_block() ||
            is_fan_code() ||
            
            
            is_fan_markup_open();
    }
    
    bool is_fan_start_key() const
    {
        return
            is_fan_line_comment() ||
            is_fan_blk_comment_open() ||
            is_fan_region_open() ||
            is_fan_directive() ||
            is_fan_output() ||
            
            is_fan_code_block() ||

            is_fan_if() ||
            is_fan_switch() ||
            is_fan_while() ||
            is_fan_for() ||
            is_fan_do() ||

            is_fan_try() ||

            is_fan_funi() ||
            is_fan_func() ||

            is_fan_funa() ||
            is_fan_await() ||
            
            is_fan_markup_open();
    }
    
    bool is_fan_escape() const { return _text == "@@";}
    
    bool is_fan_code_block() const { return _text == "@{";}

    bool is_fan_comment() const
    {
        return
            is_fan_line_comment() ||
            is_fan_blk_comment_open() ||
            is_fan_blk_comment_close();
    }
    bool is_fan_line_comment() const { return _text == "@**";}
    bool is_fan_blk_comment_open() const { return _text == "@*";}
    bool is_fan_blk_comment_close() const { return _text == "*@";}
    
    bool is_fan_directive() const
    {
        return
            is_fan_name() ||
            is_fan_namespace() ||
            is_fan_header() ||
            is_fan_inherits();
    }
    bool is_fan_namespace() const
    {
        return _text == "@namespace" || _text == "ns";
    }
    bool is_fan_name() const { return _text == "@name";}
    bool is_fan_layout() const { return _text == "@layout";}
    bool is_fan_header() const { return _text == "@header";}
    bool is_fan_inherits() const { return _text == "@inherits";}
    
    bool is_fan_region() const
    {
        return
            is_fan_region_open() || is_fan_region_close();
    }
    bool is_fan_region_open() const { return _text == "@region";}
    bool is_fan_region_close() const { return _text == "@endregion";}
    
    bool is_fan_output() const 
    {
        return is_fan_output_raw() || is_fan_output_enc();
    }
    bool is_fan_output_raw() const { return _text == "@::";}
    bool is_fan_output_enc() const { return _text == "@:";}
    
    bool is_fan_code() const
    {
        return
            is_fan_if() ||
            is_fan_switch() ||
            is_fan_while() ||
            is_fan_for() ||
            is_fan_do() ||
            is_fan_try() ||
            is_fan_funi() ||
            is_fan_func() ||
            is_fan_funa() ||
            
            is_fan_await();
    }
    
    bool is_fan_control() const
    {
        return
            is_fan_if() ||
            is_fan_switch() ||
            is_fan_while() ||
            is_fan_for() ||
            is_fan_do();
    }
    
    
    bool is_fan_if() const { return _text == "@if";}
    bool is_cpp_else() const { return _text == "else";}
    bool is_cpp_if() const { return _text == "if";}
    
    bool is_fan_switch() const { return _text == "@switch";}
    
    bool is_fan_while() const { return _text == "@while";}
    bool is_fan_for() const { return _text == "@for";}
    bool is_fan_do() const { return _text == "@do";}
    bool is_cpp_while() const { return _text == "while";}
    
    bool is_fan_try() const { return _text == "@try";}
    bool is_cpp_catch() const { return _text == "catch";}
    bool is_cpp_try() const { return _text == "try";}
    
    bool is_fan_funi() const { return _text == "@funi";}
    bool is_fan_func() const { return _text == "@func";}
    
    bool is_fan_funa() const { return _text == "@<";}
    bool is_fan_await() const { return _text == "@await";}
    
    bool is_fan_markup_open() const { return _text == "@(";}
    
    bool is_double_quote() const { return _text == "\"";}
    bool is_single_quote() const { return _text == "'";}
    bool is_backtick() const { return _text == "`";}
    
    bool is_eol() const { return _text == "\n";}
    
    bool is_semicolon() const { return _text == ";";}
    bool is_colon() const { return _text == ":";}
    
private:
    std::weak_ptr<token_t> _prev;
    
    std::string _text;
    size_t _line;
    size_t _pos;
    
    token _next;
};

class tokenizer
{
    tokenizer()
    {
    }
    
public:
    
    ~tokenizer()
    {
    }
    
    static bool find_token(const std::string& text)
    {
        const char* ptr = s_tokens[0];
        while(ptr){
            if(text.compare(ptr) == 0)
                return true;
            ++ptr;
        }
        return false;
    }
    
    static token tokenize(const std::string& text)
    {
        token root = std::make_shared<token_t>(nullptr, "", 0, 0, nullptr);
        token t = root;
        
        std::string tmp_text;
        
        size_t l = 0;
        
        size_t tl = 0;
        size_t ti = 0;
        
        for(size_t i = 0; i < text.size(); ++i){
            auto cur_chr = text[i];
            bool is_token = false;
            
            size_t ib = i;
            size_t lb = l;
            const char* tp = s_tokens[0];
            while(tp){
                std::string tok(tp);
                if(tokenizer::is_token(tok, text, i, l)){
                    is_token = true;
                    if(tmp_text.size() > 0)
                        t = t->add_next(tmp_text, tl, ti);
                    tmp_text = "";
                    ti = i;
                    tl = l;
                    t = t->add_next(tok, lb, ib);
                    break;
                }
                ++tp;
            }
            
            if(!is_token)
                tmp_text += cur_chr;
        }
        
        if(tmp_text.size() > 0)
            t = t->add_next(tmp_text, tl, ti);
        
        return root;
    }
    
    static bool is_token(
        const std::string& token,
        const std::string& text,
        size_t& i,
        size_t& l
        )
    {
        size_t start_i = i;
        
        if(start_i + token.size() > text.size() -1)
            return false;
        
        std::string val = text.substr(start_i, token.size());
        if(val == token){
            if(token == "*@"){
                if(start_i + token.size() + 1 <= text.size() -1 &&
                    text.substr(start_i, token.size() + 1) == "*@@"
                )
                    return false;
            }
            if(token == "\n")
                l += 1;
            i += token.size() -1;
            return true;
        }
        
        return false;
    }
    
    static const char* s_tokens[];
};

const char* tokenizer::s_tokens[] = {
    " ",
    "//",
    "/*",
    "*/",
    "->",
    "(",
    ")",
    "{",
    "}",
    "</", "/>",
    "<", ">",
    
    "@@",
    "@{",
    
    "@**",
    "@*",
    "*@", // this one is special, must be verified for not to be *@@ literral
    
    "@namespace", "@ns",
    "@name",
    "@layout",
    "@header",
    "@inherits",
    
    "@region",
    "@endregion",
    
    "@::", "@:",
    
    "@if", "else", "if",
    "@switch",
    
    "@while",
    "@for",
    "@do",
    "while",
    
    "@try", "catch", "try",
    
    "@funi", "@func",
    "@<", "@await",
    
    "@(",
    
    "\"", "'", "`",
    "\n",
    ";",
    ":",
    nullptr
};

inline std::string token_t::norm_text()
{
    if(tokenizer::find_token(this->_text))
        return this->_text;
    return md::trim_copy(this->_text);
}


}}}//::evmvc::fanjet::ast
#endif //_libevmvc_fanjet_tokenizer_h
