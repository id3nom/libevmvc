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

#include "stable_headers.h"
#include "view_base.h"
#include "view_engine.h"


namespace evmvc {

inline void view_base::render_view(
    md::string_view path, md::callback::async_cb cb)
{
    auto self = this->shared_from_this();
    auto scb = [self, cb](md::callback::cb_error err, const std::string& data){
        if(err)
            cb(err);
        try{
            self->write_raw(data);
        }catch(const std::exception& err){
            cb(err);
            return;
        }
        cb(nullptr);
    };
    
    std::string ps = path.to_string();
    size_t nsp = ps.rfind("::");
    if(nsp == std::string::npos){
        if(*ps.begin() != '/')
            ps = this->path().to_string() + ps;
            
        engine()->render_view(this->res, ps, scb);
        
    }else{
        std::string ns = ps.substr(0, nsp);
        std::string p = ps.substr(ns.size() +2);
        if(*p.begin() != '/')
            ps = ns + "::" + this->path().to_string() + p;
        
        view_engine::render(this->res, ps, scb);
    }
}

inline void view_base::_pop_buffer(md::string_view lng)
{
    std::string lngc = lng.to_string();
    if(_buffer_lngs.top() != lngc)
        throw MD_ERR(
            "Invalid language\n"
            "Current language is: '{}', "
            "and the request_t language to pop is '{}'",
            _buffer_lngs.top(),
            lng
        );
    
    std::string sec_name;
    bool in_section = *lngc.cbegin() == '$';
    if(in_section){
        sec_name = lngc;//lngc.substr(1);
        lngc = "html";
    }
    
    // look for a parser
    evmvc::view_engine::parse_language(lngc, _buffers.top());
    
    std::string tbuf = _buffers.top();
    _buffers.pop();
    _buffer_lngs.pop();
    
    if(in_section){
        this->add_section(sec_name, tbuf);
    }else if(_buffers.size() == 0)
        _out_buffer += tbuf;
    else
        _buffers.top() += tbuf;
}


};
