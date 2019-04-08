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

#ifndef _libevmvc_view_engine_h
#define _libevmvc_view_engine_h

#include "stable_headers.h"
#include "view_base.h"

namespace evmvc {
    
class view_engine
    : public std::enable_shared_from_this<view_engine>
{
public:
    
    view_engine(const std::string& ns, const bfs::path& views_dir)
        : _ns(ns),
        _views_dir(views_dir)
    {
        
    }
    
    /**
     * Engine instance namespace
     */
    std::string ns() const { return _ns;}
    
    bfs::path views_dir() const { return _views_dir;}
    
    /**
     * View Engine name
     */
    virtual md::string_view name() const = 0;
    static void render(
        const evmvc::sp_response& res,
        md::string_view path,
        md::callback::async_cb cb)
    {
        size_t p = path.find("::");
        std::string ns = p == std::string::npos ? "" : path.substr(0, p);
        std::string vpath = path.substr(ns.size() + 2);
        
        if(!ns.empty()){
            auto it = _engines.find(ns);
            if(it == _engines.end())
                throw MD_ERR(
                    "Unable to find engine with namespace: '{}', path: '{}'",
                    ns, path
                );
            it->second->render_view(res, vpath, cb);
            return;
        }
        
        // search the view in all namespace
        for(auto it = _engines.begin(); it != _engines.end(); ++it){
            if(it->second->view_exists(vpath)){
                it->second->render_view(res, vpath, cb);
                return;
            }
        }
        throw MD_ERR(
            "No view engine contains view at path: '{}'",
            path
        );
    }
    
    static void register_engine(const std::string& ns, sp_view_engine engine)
    {
        auto it = _engines().find(ns);
        if(it != _engines().end())
            throw MD_ERR(
                "Engine '{}' is already registered with namespace '{}'",
                engine->name(), ns
            );
        _engines().emplace(
            std::make_pair(ns, engine)
        );
    }
    
    
    virtual bool view_exists(bfs::path view_path) const = 0;
    virtual void render_view(
        const evmvc::sp_response& res,
        const std::string& path,
        md::callback::async_cb cb
    ) = 0;
    
private:
    static std::unordered_map<std::string, sp_view_engine>& _engines()
    {
        static std::unordered_map<std::string, sp_view_engine> _c;
        return _c;
    }

    std::string _ns;
    bfs::path _views_dir;
    
};

};//::evmvc

#include "view_base_impl.h"

#endif //_libevmvc_view_engine_h