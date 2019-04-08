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

template<>
inline void view_base::write_enc<md::string_view>(md::string_view data)
{
    _append_buffer(html_escape(data));
}

template<>
inline void view_base::write_raw<md::string_view>(md::string_view data)
{
    _append_buffer(data);
}

template<>
inline void view_base::set<md::string_view>(
    md::string_view name, md::string_view data)
{
    auto it = _data.find(name.to_string());
    if(it == _data.end())
        _data.emplace(std::make_pair(name.to_string(), data.to_string()));
    else
        it->second = data.to_string();
}

template<>
inline std::string view_base::get<std::string>(
    md::string_view name, const std::string& def_data
    ) const
{
    auto it = _data.find(name.to_string());
    if(it == _data.end())
        return def_data;
    return it->second;
}

template<>
inline void view_base::set<bool>(
    md::string_view name, bool data)
{
    auto it = _data.find(name.to_string());
    if(it == _data.end())
        _data.emplace(std::make_pair(name.to_string(), data ? "true" : ""));
    else
        it->second = data ? "true" : "";
}

template<>
inline bool view_base::get<bool>(
    md::string_view name, const bool& def_data
    ) const
{
    auto it = _data.find(name.to_string());
    if(it == _data.end())
        return def_data;
    return !it->second.empty();
}


inline void view_base::render_view(
    md::string_view path, md::callback::async_cb cb)
{
    std::string ps = path.to_string();
    if(ps.find("::") == std::string::npos)
        engine()->render_view(this->res, ps, cb);
    else{
        std::string p = 
            (*path.rbegin() == '/') ?
                path.substr(1).to_string() :
                this->path().to_string() + ps;
        
        view_engine::render(this->res, ps, cb);
    }
}



};
