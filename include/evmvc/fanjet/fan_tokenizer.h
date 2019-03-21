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

namespace evmvc { namespace fanjet {
    
class token_t;
typedef std::shared_ptr<token_t> token;

class token_t
    : public std::enable_shared_from_this<token_t>
{
public:
    token_t(
        token pre,
        const std::string& _text, size_t _line, size_t _pos
        sp_token nxt)
        : prev(pre), text(_text), line(_line), pos(_pos), next(nxt)
    {
    }
    
    ~token_t()
    {
    }
    
    std::string norm_text();
    
    bool norm_is_empty()
    {
        return this->text.size() > 0 && this->norm_text().size() > 0;
    }
    
    sp_token get_norm_non_empty_prev()
    {
        if(!this->prev)
            return nullptr;
        if(prev->norm_is_empty())
            return prev->get_norm_non_empty_prev();
        return prev;
    }
    
    std::string debug_string()
    {
        return fmt::format(
            "TOKEN: cur '{0}', prev '{1}', next '{2}'",
            this->text,
            this->prev ? this->prev->text : "NULL",
            this->next ? this->next->text : "NULL"
        );
    }
    
    token add_next(const std::string& _text, size_t _line, size_t _pos)
    {
        this->next = std::make_shared(
            this->shared_from_this(),
            _text,
            _line,
            _pos
        );
    }
    
    std::weak_ptr<token_t> prev;
    
    std::string text;
    size_t line;
    size_t pos;
    
    token next;
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
    
    static sp_token tokenize(const std::string& text)
    {
        //std::vector<std::string> tokens;
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
                        //tokens.emplace_back(tmp_text);
                    tmp_text = "";
                    ti = i
                    tl = l;
                    t = t->add_next(tok, lb, ib);
                    //tokens.emplace_back(tok);
                    break;
                }
                ++tp;
            }
            
            if(!is_token)
                tmp_text += cur_chr;
        }
        
        if(tmp_text.size() > 0)
            t = t->add_next(tmp_text, tl, ti);
            //tokens.emplace_back(tmp_text);
        
        // token root = std::make_shared<token>(nullptr, "", nullptr);
        // token t = root;
        // for(auto s : tokens){
        //     token new_tok = std::make_shared<token>(t, s, nullptr);
        //     t->next = new_tok;
        //     t = new_tok;
        // }
        
        return root;
    }
    
    static bool is_token(
        const std::string& std::string token,
        const std::string& text,
        size_t& i,
        size_t& lnsize_t& l
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

constexpr char* tokenizer::s_tokens[] = {
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
    
    "\"",
    "'",
    "\n",
    ";",
    ":",
    nullptr
};

inline std::string token::norm_text()
{
    if(tokenizer::find_token(this->text))
        return this->text;
    return trim_copy(this->text);
}


}}//::evmvc::fanjet
#endif //_libevmvc_fanjet_tokenizer_h
