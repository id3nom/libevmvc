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

typedef std::function<void(std::string& source)> view_lang_parser_fn;

class view_engine
    : public std::enable_shared_from_this<view_engine>
{
    typedef std::unordered_map<std::string, view_lang_parser_fn>
        lang_parser_map;

public:
    
    view_engine(const std::string& ns)
        : _ns(ns)
    {
        
    }
    
    /**
     * Engine instance namespace
     */
    std::string ns() const { return _ns;}
    
    void register_language_parser(md::string_view lng, view_lang_parser_fn pfn)
    {
        std::string lng_str = lng.to_string();
        auto it = lang_parsers().find(lng_str);
        if(it != lang_parsers().end())
            throw MD_ERR(
                "Language parser '{}' is already registered!",
                lng_str
            );
        
        if(pfn == nullptr)
            throw MD_ERR("Language parser can't be NULL");
        
        lang_parsers().emplace(
            std::make_pair(lng_str, pfn)
        );
    }
    
    static void parse_language(const std::string& lang, std::string& s)
    {
        auto it = lang_parsers().find(lang);
        if(it != lang_parsers().end())
            it->second(s);
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
    
    static void render(
        const evmvc::sp_response& res,
        md::string_view path,
        md::callback::value_cb<const std::string&> cb)
    {
        size_t p = path.rfind("::");
        std::string ns = 
            p == std::string::npos ? 
                "" :
                path.substr(0, p).to_string();
        std::string vpath = path.substr(ns.size() + 2).to_string();
        
        if(!ns.empty()){
            auto it = _engines().find(ns);
            if(it == _engines().end()){
                cb(MD_ERR(
                    "Unable to find engine with namespace: '{}', path: '{}'",
                    ns, path
                ), "");
                return;
            }
            it->second->render_view(res, vpath, cb);
            return;
        }
        
        // search the view in all namespace
        for(auto it = _engines().begin(); it != _engines().end(); ++it){
            if(it->second->view_exists(vpath)){
                it->second->render_view(res, vpath, cb);
                return;
            }
        }
        
        cb(MD_ERR(
            "No view engine contains view at path: '{}'",
            path
        ), "");
    }
    
    static std::shared_ptr<evmvc::view_base> get(
        const evmvc::sp_response& res, md::string_view path)
    {
        size_t p = path.rfind("::");
        std::string ns = 
            p == std::string::npos ? 
                "" :
                path.substr(0, p).to_string();
        std::string vpath = path.substr(ns.size() + 2).to_string();
        
        if(!ns.empty()){
            auto it = _engines().find(ns);
            if(it == _engines().end())
                return nullptr;
            return it->second->get_view(res, vpath);
        }
        
        // search the view in all namespace
        for(auto it = _engines().begin(); it != _engines().end(); ++it){
            if(it->second->view_exists(vpath)){
                return it->second->get_view(res, vpath);
            }
        }
        return nullptr;
    }
    
    /**
     * View Engine name
     */
    virtual md::string_view name() const = 0;
    
    virtual std::shared_ptr<evmvc::view_base> get_view(
        const evmvc::sp_response& res,
        const std::string& path
    ) = 0;
    virtual bool view_exists(const std::string& view_path) const = 0;
    virtual void render_view(
        const evmvc::sp_response& res,
        const std::string& path,
        md::callback::value_cb<const std::string&> cb
    ) = 0;
    
private:
    static std::unordered_map<std::string, sp_view_engine>& _engines()
    {
        static std::unordered_map<std::string, sp_view_engine> _c;
        return _c;
    }
    
    static lang_parser_map& lang_parsers()
    {
        static lang_parser_map _lang_parsers;
        return _lang_parsers;
    };
    
    std::string _ns;
    
};

};//::evmvc

#include "view_base_impl.h"

#endif //_libevmvc_view_engine_h