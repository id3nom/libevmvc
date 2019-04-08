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

#ifndef _libevmvc_view_base_h
#define _libevmvc_view_base_h

#include "stable_headers.h"
#include "response.h"

#include <stack>

namespace evmvc {

typedef std::function<void(std::string& source)> view_lang_parser_fn;

class view_engine;
typedef std::shared_ptr<view_engine> sp_view_engine;

class view_base
    : public std::enable_shared_from_this<view_base>
{
    typedef std::unordered_map<std::string, view_lang_parser_fn>
        lang_parser_map;
public:
    view_base(
        sp_view_engine engine,
        const evmvc::sp_response& _res)
        : _engine(engine),
        res(_res),
        req(_res->req())
    {
    }
    
    sp_view_engine engine() const { return _engine;}
    
    virtual md::string_view ns() const = 0;
    virtual md::string_view path() const = 0;
    virtual md::string_view name() const = 0;
    virtual md::string_view abs_path() const = 0;
    virtual md::string_view layout() const = 0;
    
    virtual void render(
        std::shared_ptr<view_base> self,
        md::callback::async_cb cb
    ) = 0;
    
    void add_lang_parser(md::string_view lng, view_lang_parser_fn pfn)
    {
        std::string lng_str = lng.to_string();
        auto it = _lang_parsers.find(lng_str);
        if(it != _lang_parsers.end())
            throw MD_ERR(
                "Language parser '{}' is already registered!",
                lng_str
            );
        
        if(pfn == nullptr)
            throw MD_ERR("Language parser can't be NULL");
        
        _lang_parsers.emplace(
            std::make_pair(lng_str, pfn)
        );
    }
    
    void begin_write(md::string_view lng)
    {
        _push_buffer(lng);
    }
    void commit_write(md::string_view lng)
    {
        _pop_buffer(lng);
    }
    
    template<typename T>
    void write_enc(T data)
    {
        std::stringstream ss;
        ss << "Writing to " << typeid(T).name() << " is not supported";
        throw MD_ERR(ss.str());
    }
    template<typename T>
    void write_raw(T data)
    {
        std::stringstream ss;
        ss << "Writing to " << typeid(T).name() << " is not supported";
        throw MD_ERR(ss.str());
    }
    
    void write_body()
    {
        this->begin_write("html");
        this->write_raw(_body_buffer);
        this->commit_write("html");
    }
    void render_view(md::string_view path, md::callback::async_cb cb);
    
    template<typename T>
    void set(md::string_view name, T val)
    {
        std::stringstream ss;
        ss << "Writing to " << typeid(T).name() << " is not supported";
        throw MD_ERR(ss.str());
    }
    
    template<typename T>
    T get(md::string_view name, T def_val = T())
    {
        std::stringstream ss;
        ss << "Converting to " << typeid(T).name() << " is not supported";
        throw MD_ERR(ss.str());
    }
    
private:
    sp_view_engine _engine;
    std::string _body_buffer;
    std::string _out_buffer;
    
    std::stack<std::string> _buffers;
    std::stack<std::string> _buffer_lngs;
    
    void _append_buffer(md::string_view d)
    {
        _buffers.top() += d.to_string();
    }
    
    void _push_buffer(md::string_view lng)
    {
        _buffers.push("");
        _buffer_lngs.push(lng.to_string());
    }
    
    void _pop_buffer(md::string_view lng)
    {
        if(_buffer_lngs.top() != lng.to_string())
            throw MD_ERR(
                "Invalid language\n"
                "Current language is: '{}', "
                "and the request language to pop is '{}'",
                _buffer_lngs.top(),
                lng
            );
        
        // look for a parser
        auto it = _lang_parsers.find(lng.to_string());
        if(it != _lang_parsers.end())
            it->second(_buffers.top());
        _out_buffer += _buffers.top();
        _buffers.pop();
        _buffer_lngs.pop();
    }
    
protected:
    
    static std::string uri_encode(md::string_view s)
    {
        char* r = evhttp_encode_uri(s.data());
        std::string tmp(r);
        free(r);
        return tmp;
    }
    static std::string uri_decode(md::string_view s)
    {
        char* r = evhttp_decode_uri(s.data());
        std::string tmp(r);
        free(r);
        return tmp;
    }
    static std::string html_escape(md::string_view s)
    {
        char* r = evhttp_htmlescape(s.data());
        std::string tmp(r);
        free(r);
        return tmp;
    }
    
    evmvc::sp_response res;
    evmvc::sp_request req;
    
private:
    lang_parser_map _lang_parsers;
};

};
#endif //_libevmvc_view_base_h