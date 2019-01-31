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

#ifndef _libevmvc_router_h
#define _libevmvc_router_h

#include "stable_headers.h"

extern "C" {
#include <pcre.h>
}

#include "cb_def.h"
#include "methods.h"
#include "request.h"
#include "response.h"


namespace evmvc {

//    # simple route that will match url "/abc/123" and "/abc/123/"
//    /abc/123
//    
//    # simple route that will match url's "/abc/123", "/abc/123/def",
//    # "/abc/123/456/" but not "/abc/123/def/other_path"
//    /abc/123/*
//    
//    # simple route that will match url's "/abc/123", "/abc/123/def",
//    # "/abc/123/456/" and any sub path "/abc/123/def/sub/path/..."
//    /abc/123/**
//    
//    # optional parameter can be defined
//    # by enclosing the parameter in square brackets
//    /abc/123/:p1/:[p2]
//    
//    # PCRE regex can be use to validate parameter by enclosing
//    # the rules inside parentheses following the parameter name
//    /abc/123/:p1(\\d+)/:[p2]
//    
//    # regex parameter can be optional as well
//    /abc/123/:[p1(\\d+)]
//    
//    # all parameters following an optional parameter must be optional
//    /abc/123/:p1(\\d+)/:[p2]/:[p3]
//    

//typedef std::function<void(boost::system::error_code)> async_cb;

typedef 
    std::function<void(
        const evmvc::request&,
        evmvc::response&,
        async_cb
    )> route_handler_cb;
// template<class Send>
// using route_handler_cb = 
//     std::function<void(
//         const evmvc::request&,
//         evmvc::response<Send>&,
//         async_cb
//     )>;


class route_result;
typedef std::shared_ptr<route_result> sp_route_result;

class route;
typedef std::shared_ptr<route> sp_route;

class router;
typedef std::shared_ptr<router> sp_router;

class route_result
{
public:
    route_result(sp_route r)
        : route(r)
    {
    }
    
    void execute(
        evhtp_request_t* req,
        evmvc::response& res, async_cb cb
    );
    
    sp_route route;
    //std::unordered_map<std::string, std::string> params;
    evmvc::request::param_map params;
};

class route
    : public std::enable_shared_from_this<route>
{
    friend class route_result;
    
    struct route_seg
    {
        std::string seg_path;
        std::string re_pattern;
        bool optional;
        bool is_param;
        std::string param_name;
    };
    
public:
    route(evmvc::string_view route_path)
        : _re(nullptr)
    {
        this->_build_route_re(route_path);
    }
    
    ~route()
    {
        if(_re){
            pcre_free_study(_re_study);
            pcre_free(_re);
        }
        _re = nullptr;
        _re_study = nullptr;
    }
    
    sp_route add_handler(route_handler_cb cb)
    {
        _handlers.emplace_back(cb);
        return this->shared_from_this();
    }
    
    virtual sp_route_result match(const evmvc::string_view& value)
    {
        if(_re_pattern.size() == 0)
            return std::make_shared<route_result>(
                this->shared_from_this()
            );
        
        int OVECCOUNT = 300; /* should be a multiple of 3 */
        int ovector[OVECCOUNT];
        
        int rc = pcre_exec(
            _re, _re_study,
            value.data(), value.size(),
            0, // start at offset 0 in the subject
            0, // default options
            ovector, // output vector for substring information
            OVECCOUNT // number of elements in the output vector
        );
        
        if(rc < 0)
            return nullptr;
        if(rc == 0)
            throw std::runtime_error(
                fmt::format(
                    "ovector only has room for {0} captured substrings",
                    (OVECCOUNT/3-1)
                )
            );

        sp_route_result rr = std::make_shared<route_result>(
            this->shared_from_this()
        );
        
        int namecnt;
        pcre_fullinfo(_re, _re_study, PCRE_INFO_NAMECOUNT, &namecnt);
        if(namecnt <= 0)
            return rr;
        
        unsigned char* name_table;
        int name_entry_size;
        
        pcre_fullinfo(
            _re, _re_study, PCRE_INFO_NAMETABLE, &name_table
        );
        pcre_fullinfo(
            _re, _re_study, PCRE_INFO_NAMEENTRYSIZE, &name_entry_size
        );
        
        unsigned char* tabptr = name_table;
        for(int i = 0; i < namecnt; ++i){
            int n = (tabptr[0] << 8) | tabptr[1];
            std::string pname((char*)(tabptr + 2), name_entry_size -3);
            std::string pval(
                (char*)(value.data() + ovector[2*n]),
                ovector[2*n+1] - ovector[2*n]
            );
            
            rr->params.emplace(
                std::make_pair(pname, std::make_shared<http_param>(pval))
            );
            
            tabptr += name_entry_size;
        }
        return rr;
    }
    
private:
    
    void execute(
        const evmvc::request::param_map params,
        evhtp_request_t* ev_req,
        evmvc::response& res,
        async_cb cb)
    {
        evmvc::request req(ev_req, res.shared_cookies(), params);
        _exec(req, res, 0, cb);
    }
    
    void _exec(
        evmvc::request& req, evmvc::response& res,
        size_t hidx, async_cb cb)
    {
        _handlers[hidx](
            req, res,
        [self = this->shared_from_this(), &req, &res, &hidx, &cb]
        (cb_error err){
            if(err){
                cb(err);
                return;
            }
            if(++hidx == self->_handlers.size()){
                cb(nullptr);
                return;
            }
            
            self->_exec(req, res, hidx, cb);
        });
    }

    void _build_route_re(evmvc::string_view route_path)
    {
        if(route_path.size() == 0)
            return;
        
        std::vector<route_seg> segs;
        
        std::string cur_path;
        for(
            size_t i = route_path[0] == '/' ? 1 : 0;
            i < route_path.size();
            ++i
        ){
            if(route_path[i] == '/' || i == route_path.size() -1){
                if(route_path[i] != '/' && i == route_path.size() -1)
                    cur_path += route_path[i];
                
                if(cur_path.empty())
                    continue;
                
                // is a parameter
                if(cur_path[0] == ':'){
                    segs.emplace_back(route_seg{
                        cur_path,
                        "", 
                        cur_path[1] == '[' &&
                            cur_path[cur_path.size() -1] == ']',
                        true, ""
                    });
                    
                    cur_path.clear();
                    continue;
                }
                
                segs.emplace_back(route_seg{
                    cur_path,
                    "", 
                    cur_path[0] == '[' &&
                        cur_path[cur_path.size() -1] == ']',
                    false, ""
                });
                
                cur_path.clear();
                continue;
            }
            
            cur_path += route_path[i];
        }
        
        for(size_t i = 0; i < segs.size(); ++i){
            auto& rs = segs[i];
            if(rs.is_param)
                rs.seg_path = rs.seg_path.substr(1);
            if(rs.optional)
                rs.seg_path = rs.seg_path.substr(1, rs.seg_path.size() -2);
            if(rs.is_param){
                std::string re;
                auto poidx = rs.seg_path.find("(", 0);
                if(poidx != std::string::npos){
                    auto pcidx = rs.seg_path.rfind(")");
                    if(pcidx != std::string::npos)
                        rs.re_pattern =
                            rs.seg_path.substr(poidx +1, pcidx-poidx);
                    else
                        rs.re_pattern = "[^\\/\\n]+)";
                    rs.param_name = rs.seg_path.substr(0, poidx);
                    rs.re_pattern = "(?<" + rs.param_name + ">" + rs.re_pattern;
                }else{
                    rs.param_name = rs.seg_path;
                    rs.re_pattern = "(?<" + rs.param_name + ">[^\\/\\n]+)";
                }
            }else{
                if(rs.seg_path == "*")
                    rs.re_pattern = "[^\\/\\n]+";
                else if(rs.seg_path == "**")
                    rs.re_pattern = ".+";
                else
                    rs.re_pattern = rs.seg_path;
            }
        }
        
        _re_pattern = _build_route_re(segs, 0) + "($|\\/$)";
        
        const char* error = nullptr;
        int erroffset;
        int re_opts = PCRE_CASELESS | PCRE_UTF8;
        _re = pcre_compile(
            _re_pattern.c_str(),
            re_opts,
            &error, &erroffset, nullptr
        );
        if(!_re)
            throw std::runtime_error(
                fmt::format(
                    "PCRE compilation failed for route '{0}', pattern '{1}' "
                    "at offset {2}: {3}",
                    route_path.data(), _re_pattern, erroffset, error
                )
            );
        
        _re_study = pcre_study(_re, 0, &error);
        if(!_re_study){
            pcre_free(_re);
            _re = nullptr;
            
            throw std::runtime_error(
                fmt::format(
                    "PCRE study failed for route '{0}' : {1}",
                    route_path.data(), error
                )
            );
        }
    }
    
    std::string _build_route_re(
        std::vector<route_seg>& segs,
        size_t seg_idx)
    {
        if(seg_idx == segs.size())
            return "";

        route_seg& rs = segs[seg_idx];
        
        if(rs.is_param)
            _param_names.emplace_back(rs.param_name);
        
        if(rs.optional){
            return
                "($|\\/$|\\/" + rs.re_pattern + 
                _build_route_re(segs, ++seg_idx) +
                ")";
        }
        
        return "\\/" + rs.re_pattern + _build_route_re(segs, ++seg_idx);
    }
    
    std::vector<std::string> _param_names;
    std::vector<route_handler_cb> _handlers;
    
    std::string _re_pattern;
    pcre* _re;
    pcre_extra* _re_study;
};

void route_result::execute(
    evhtp_request_t* req,
    evmvc::response& res, async_cb cb)
{
    route->execute(params, req, res, cb);
}



class router
    : public std::enable_shared_from_this<router>
{
    typedef std::unordered_map<std::string, sp_router> router_map;
    typedef std::unordered_map<std::string, sp_route> route_map;
    typedef std::unordered_map<std::string, route_map> verb_map;
    
public:
    router()
        : _parent(nullptr), _path(_norm_path("")),
        _match_first(boost::indeterminate),
         _match_strict(boost::indeterminate),
          _match_case(boost::indeterminate)
    {
    }

    router(const evmvc::string_view& path)
        : _parent(nullptr), _path(_norm_path(path)),
        _match_first(boost::indeterminate),
         _match_strict(boost::indeterminate),
          _match_case(boost::indeterminate)
    {
    }
    
    ~router()
    {
    }
    
    sp_route resolve_route(
        evmvc::method method,
        const evmvc::string_view& url)
    {
        evmvc::string_view vs = evmvc::method_to_string(method);
        return resolve_route(vs, url);
    }
    
    sp_route resolve_route(
        const evmvc::string_view& method,
        const evmvc::string_view& url)
    {
        auto rm = _verbs.find(std::string(method));
        if(rm == _verbs.end())
            return nullptr;
        
        auto it = rm->second.find(std::string(url));
        if(it == rm->second.end())
            return nullptr;
        
        return it->second;
    }
    
    sp_route_result resolve_url(
        evmvc::method method,
        const evmvc::string_view& url)
    {
        evmvc::string_view vs = evmvc::method_to_string(method);
        return resolve_url(vs, url);
    }
    
    sp_route_result resolve_url(
        const evmvc::string_view& method,
        const evmvc::string_view& url)
    {
        std::string local_url = std::string(url).substr(_path.size());
        
        // verify if child router match path
        for(auto it = _routers.begin(); it != _routers.end(); ++it)
            if(local_url.size() > it->first.size() &&
                !std::strncmp(local_url.data(),
                    it->first.c_str(), it->first.size()
                )
            )
                return it->second->resolve_url(
                    method,
                    local_url
                );
        
        auto rm = _verbs.find(std::string(method));
        if(rm != _verbs.end())
            for(auto it = rm->second.begin(); it != rm->second.end(); ++it){
                sp_route_result rr = it->second->match(local_url);
                if(rr)
                    return rr;
            }
        
        if(std::string(method) != "ALL")
            return resolve_url("ALL", url);
        
        return nullptr;
    }
    
    sp_router all(const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_route_handler("ALL", route_path, cb);
    }
    sp_router options(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_handler(evmvc::method::options, route_path, cb);
    }
    sp_router del(const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_handler(evmvc::method::del, route_path, cb);
    }
    sp_router head(const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_handler(evmvc::method::head, route_path, cb);
    }
    sp_router get(const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_handler(evmvc::method::get, route_path, cb);
    }
    sp_router post(const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_handler(evmvc::method::post, route_path, cb);
    }
    sp_router put(const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_handler(evmvc::method::put, route_path, cb);
    }
    sp_router connect(
        const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_handler(evmvc::method::connect, route_path, cb);
    }
    sp_router trace(const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_handler(evmvc::method::trace, route_path, cb);
    }
    sp_router patch(const evmvc::string_view& route_path, route_handler_cb cb)
    {
        return add_handler(evmvc::method::patch, route_path, cb);
    }
    // sp_router purge(const evmvc::string_view& route_path, route_handler_cb cb)
    // {
    //     return add_handler(evmvc::method::purge, route_path, cb);
    // }
    // sp_router link(const evmvc::string_view& route_path, route_handler_cb cb)
    // {
    //     return add_handler(evmvc::method::link, route_path, cb);
    // }
    // sp_router unlink(
    //     const evmvc::string_view& route_path, route_handler_cb cb)
    // {
    //     return add_handler(evmvc::method::unlink, route_path, cb);
    // }
    
    sp_router add_route_handler(
        const evmvc::string_view& method,
        const evmvc::string_view& route_path,
        route_handler_cb cb)
    {
        return add_handler(method, route_path, cb);
    }
    
    sp_router add_router(sp_router router)
    {
        router->_parent = this->shared_from_this();
        _routers.emplace(std::make_pair(router->_path, router));
        return this->shared_from_this();
    }
    
protected:
    
    static std::string _norm_path(const evmvc::string_view& path)
    {
        if(path.size() == 0)
            return "";
        
        if(path.starts_with("/"))
            return std::string(path.substr(1));
        
        return std::string(path);
    }
    
    sp_router add_handler(
        evmvc::method method,
        const evmvc::string_view& route_path,
        route_handler_cb cb)
    {
        evmvc::string_view vs = evmvc::method_to_string(method);
        return add_handler(vs, route_path, cb);
    }
    
    sp_router add_handler(
        const evmvc::string_view& method,
        const evmvc::string_view& route_path,
        route_handler_cb cb)
    {
        auto rp = resolve_route(method, route_path);
        if(!rp)
            rp = add_route(method, route_path);
        
        rp->add_handler(cb);
        return this->shared_from_this();
    }

    sp_route add_route(
        evmvc::method method,
        const evmvc::string_view& route_path)
    {
        evmvc::string_view vs = evmvc::method_to_string(method);
        return add_route(vs, route_path);
    }
    
    sp_route add_route(
        const evmvc::string_view& method,
        const evmvc::string_view& route_path)
    {
        auto rm = _verbs.find(std::string(method));
        if(rm == _verbs.end()){
            _verbs.emplace(std::make_pair(method, route_map()));
            rm = _verbs.find(std::string(method));
        }
        
        sp_route r = std::make_shared<evmvc::route>(route_path);
        rm->second.emplace(std::make_pair(route_path, r));
        return r;
    }
    
    sp_router _parent;
    std::string _path;
    // if the first matching router_path is return
    // if is false and more than one router_path is found,
    // it will throw an error.
    boost::tribool _match_first;
    // if routes "/abc/123" and "/abc/123/" are different.
    boost::tribool _match_strict;
    // if route case is sensitive.
    boost::tribool _match_case;
    
    verb_map _verbs;
    router_map _routers;
};



} //ns evmvc
#endif //_libevmvc_router_h
