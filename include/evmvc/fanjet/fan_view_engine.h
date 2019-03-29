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

#ifndef _libevmvc_fanjet_view_engine_h
#define _libevmvc_fanjet_view_engine_h

#include "../stable_headers.h"
#include "../response.h"
#include "../view_engine.h"
#include "fan_view_base.h"

#define EVMVC_FANJET_VIEW_ENGINE_NAME "fanjet"

namespace evmvc { namespace fanjet {

typedef std::function<
    std::shared_ptr<fanjet::view_base>(
        sp_view_engine engine,
        const evmvc::sp_response& res
    )> view_generator_fn;

class view_engine
    : public evmvc::view_engine
{
    friend class app;
public:
    
    view_engine(
        const std::string& ns, const bfs::path& views_dir,
        const bfs::path& dest_dir)
        : evmvc::view_engine(ns, views_dir),
        _dest_dir(dest_dir)
    {
    }
    
    /**
     * View Engine name
     */
    md::string_view name() const { return EVMVC_FANJET_VIEW_ENGINE_NAME;};
    
    void render(
        const evmvc::sp_response& res,
        const bfs::path& view_path,
        md::callback::async_cb cb)
    {
        auto it = _views.find(view_path.string());
        if(it == _views.end())
            throw MD_ERR(
                "Engine '{}' is unable to locate view at: '{}::{}'",
                this->name(), this->ns(), view_path.string()
            );
        
        auto v = it->second(this->shared_from_this(), res);
        v->render(v, cb);
    }
    
    bool view_exists(bfs::path view_path) const
    {
        auto it = _views.find(view_path.string());
        return it != _views.end();
    }
    
    void register_view_generator(bfs::path view_path, view_generator_fn vg)
    {
        auto it = _views.find(view_path.string());
        if(it != _views.end())
            throw MD_ERR(
                "Engine '{}' view '{}::{}' is already registered!",
                this->name(), this->ns(), view_path.string()
            );
        
        _views.emplace(
            std::make_pair(view_path.string(), vg)
        );
    }
    
    
    
private:
    bfs::path _dest_dir;
    
    std::unordered_map<std::string, view_generator_fn> _views;
    
};

}};//evmvc::fanjet
#endif //_libevmvc_fanjet_view_engine_h
