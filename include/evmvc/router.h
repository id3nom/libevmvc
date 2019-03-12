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
#include "methods.h"
#include "filter_policies.h"
#include "request.h"
#include "response.h"
//#include "multipart_parser.h"

namespace evmvc {

typedef 
    std::function<void(
        const evmvc::sp_request,
        evmvc::sp_response,
        md::callback::async_cb
    )> route_handler_cb;


class route_result
{
    route_result()
        : _route()
    {
        EVMVC_DEF_TRACE("route_result {:p} created", (void*)this);
    }
    
public:
    route_result(sp_route r)
        : _route(r)
    {
        EVMVC_DEF_TRACE("route_result {:p} created", (void*)this);
    }
    
    ~route_result()
    {
        EVMVC_DEF_TRACE("file_route_result {:p} released", (void*)this);
    }
    
    md::log::sp_logger log();
    
    virtual void execute(evmvc::sp_response res, md::callback::async_cb cb);
    virtual void validate_access(
        evmvc::policies::filter_rule_ctx ctx,
        evmvc::policies::validation_cb cb);
    
    sp_route _route;
    std::vector<std::shared_ptr<evmvc::http_param>> params;
};

class route
    : public std::enable_shared_from_this<route>
{
    friend class route_result;
    friend class file_route_result;
    
    struct route_seg
    {
        std::string seg_path;
        std::string re_pattern;
        bool optional;
        bool is_param;
        std::string param_name;
    };

protected:
    route(std::weak_ptr<router> rtr)
        : _rtr(rtr), _log(), _rp(""), _re(nullptr)
    {
        EVMVC_DEF_TRACE("route {:p} created", (void*)this);
    }
    
public:
    route(std::weak_ptr<router> rtr, md::string_view route_path)
        : _rtr(rtr), _log(), _rp(route_path), _re(nullptr)
    {
        EVMVC_DEF_TRACE("route {:p} created", (void*)this);
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
        EVMVC_DEF_TRACE("route {:p} released", (void*)this);
    }
    
    static sp_route& null(wp_app a);
    
    evmvc::sp_router get_router() const { return _rtr.lock();}
    md::log::sp_logger log() const;
    
    bool has_callbacks() const { return !_handlers.empty();}
    bool has_policies() const { return !_policies.empty();}
    
    sp_route register_policy(policies::filter_policy pol)
    {
        _policies.emplace_back(pol);
        return this->shared_from_this();
    }
    
    sp_route register_handler(route_handler_cb cb)
    {
        _handlers.emplace_back(cb);
        return this->shared_from_this();
    }
    
    virtual sp_route_result match(const md::string_view& value)
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
            if(!pval.empty()){
                char* dp = evhttp_uridecode(pval.c_str(), false, nullptr);
                rr->params.emplace_back(
                    std::make_shared<evmvc::http_param>(pname, std::string(dp))
                );
                
                free(dp);
            }
            
            tabptr += name_entry_size;
        }
        
        return rr;
    }
    
    void validate_access(
        evmvc::policies::filter_rule_ctx ctx,
        evmvc::policies::validation_cb cb);
    
protected:
    
    virtual void _exec(
        evmvc::sp_request req, evmvc::sp_response res,
        size_t hidx, md::callback::async_cb cb)
    {
        _handlers[hidx](
            req, res,
        [self = this->shared_from_this(), &req, &res, hidx, cb]
        (md::callback::cb_error err) mutable {
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
    
    void _validate_route_policies(
        policies::filter_type type,
        size_t idx,
        evmvc::policies::filter_rule_ctx ctx,
        evmvc::policies::validation_cb cb)
    {
        if(idx >= _policies.size())
            return cb(nullptr);
        
        _policies[idx]->validate(
            type,
            ctx,
        [self = this->shared_from_this(), type, ctx, idx, cb]
        (const md::callback::cb_error& err) mutable {
            if(err)
                return cb(err);
            if(++idx >= self->_policies.size())
                return cb(nullptr);
            
            self->_validate_route_policies(type, idx, ctx, cb);
        });
    }
    
    
    void _build_route_re(md::string_view route_path)
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
        
        _re_pattern = "^" + _build_route_re(segs, 0) + "($|\\/$)";
        
        const char* error = nullptr;
        int erroffset;
        int re_opts = PCRE_CASELESS | PCRE_UTF8;
        _re = pcre_compile(
            _re_pattern.c_str(),
            re_opts,
            &error, &erroffset, nullptr
        );
        if(!_re)
            throw MD_ERR(
                "PCRE compilation failed for route '{0}', pattern '{1}' "
                "at offset {2}: {3}",
                route_path.data(), _re_pattern, erroffset, error
            );
        
        _re_study = pcre_study(_re, 0, &error);
        if(!_re_study){
            std::string error_msg;
            if(error)
                error_msg += error;
            
            pcre_free(_re);
            _re = nullptr;
            
            throw MD_ERR(
                "PCRE study failed for route '{0}' : {1}",
                route_path.data(), error_msg
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
    
    std::weak_ptr<router> _rtr;
    mutable md::log::sp_logger _log;
    std::string _rp;
    
    std::vector<std::string> _param_names;
    std::vector<route_handler_cb> _handlers;
    std::vector<policies::filter_policy> _policies;
    
    std::string _re_pattern;
    pcre* _re;
    pcre_extra* _re_study;
};



class router
    : public std::enable_shared_from_this<router>
{
    friend class evmvc::app;
    
    typedef std::unordered_map<std::string, sp_router> router_map;
    typedef std::unordered_map<std::string, sp_route> route_map;
    typedef std::unordered_map<std::string, route_map> verb_map;
    
public:
    router(evmvc::wp_app app);
    router(evmvc::wp_app app, const md::string_view& path);
    
    virtual ~router()
    {
        EVMVC_DEF_TRACE("router {:p} released", (void*)this);
    }
    
    static sp_router& null(wp_app a);
    
    evmvc::sp_app get_app() const { return _app.lock();}
    md::log::sp_logger log() const { return _log;}
    
    md::string_view path() const { return _path;}
    
    std::string full_path() const
    {
        if(this->_parent)
            return this->_parent->full_path() + this->_path;
        else
            return this->_path;
    }
    
    sp_router find_router(md::string_view path)
    {
        if(!strcasecmp(this->full_path().c_str(), path.data()))
            return this->shared_from_this();
        
        for(auto it = _routers.begin(); it != _routers.end(); ++it){
            sp_router rtr = it->second->find_router(path);
            if(rtr)
                return rtr;
        }
        
        return nullptr;
    }
    
    sp_route resolve_route(
        evmvc::method method,
        const md::string_view& url)
    {
        md::string_view vs = evmvc::to_string(method);
        return resolve_route(vs, url);
    }
    
    sp_route resolve_route(
        const md::string_view& method,
        const md::string_view& url)
    {
        auto rm = _verbs.find(std::string(method));
        if(rm == _verbs.end())
            return nullptr;
        
        auto it = rm->second.find(std::string(url));
        if(it == rm->second.end())
            return nullptr;
        
        return it->second;
    }
    
    virtual sp_route_result resolve_url(
        evmvc::method method,
        const md::string_view& url)
    {
        md::string_view vs = evmvc::to_string(method);
        return resolve_url(vs, url);
    }
    
    virtual sp_route_result resolve_url(
        const md::string_view& method,
        const md::string_view& url)
    {
        // std::string local_url = 
        //     std::string(url).substr(_path.size());
        
        // // verify if child router match path
        // for(auto it = _routers.begin(); it != _routers.end(); ++it)
        //     if(local_url.size() > it->first.size() &&
        //         !std::strncmp(local_url.data(),
        //             it->first.c_str(), it->first.size()
        //         )
        //     )
        //         return it->second->resolve_url(
        //             method,
        //             local_url
        //         );
        
        // auto rm = _verbs.find(std::string(method));
        // if(rm != _verbs.end())
        //     for(auto it = rm->second.begin(); it != rm->second.end(); ++it){
        //         if(it->second->has_callbacks()){
        //             sp_route_result rr = it->second->match(local_url);
        //             if(rr)
        //                 return rr;
        //         }
        //     }
        
        // if(std::string(method) != "ALL")
        //     return resolve_url("ALL", url);
        
        // return nullptr;
        md::string_view local_url = url.substr(_path.size());
        
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
        
        local_url = url.substr(_path.size() -1);
        auto rm = _verbs.find(std::string(method));
        if(rm != _verbs.end())
            for(auto it = rm->second.begin(); it != rm->second.end(); ++it){
                if(it->second->has_callbacks()){
                    sp_route_result rr = it->second->match(local_url);
                    if(rr)
                        return rr;
                }
            }
        
        if(std::string(method) != "ALL")
            return resolve_url("ALL", url);
        
        return nullptr;
    }
    
    
    
    virtual sp_router all(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_route_handler("ALL", route_path, cb, pol);
    }
    virtual sp_router options(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(evmvc::method::options, route_path, cb, pol);
    }
    virtual sp_router del(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(evmvc::method::del, route_path, cb, pol);
    }
    virtual sp_router head(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(evmvc::method::head, route_path, cb, pol);
    }
    virtual sp_router get(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(evmvc::method::get, route_path, cb, pol);
    }
    virtual sp_router post(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(evmvc::method::post, route_path, cb, pol);
    }
    virtual sp_router put(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(evmvc::method::put, route_path, cb, pol);
    }
    virtual sp_router connect(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(evmvc::method::connect, route_path, cb, pol);
    }
    virtual sp_router trace(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(evmvc::method::trace, route_path, cb, pol);
    }
    virtual sp_router patch(
        const md::string_view& route_path, route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(evmvc::method::patch, route_path, cb, pol);
    }
    
    virtual sp_router register_route_handler(
        const md::string_view& method,
        const md::string_view& route_path,
        route_handler_cb cb,
        policies::filter_policy pol = policies::filter_policy())
    {
        return register_handler(method, route_path, cb, pol);
    }
    
    virtual sp_router register_router(sp_router router)
    {
        if(router->path() == "/")
            throw MD_ERR(
                "invalid path '/', can't add router with root path!"
            );
        
        this->_log->info(
            "Registering router '{}'",
            router->_path
        );
        
        router->_app = this->_app;
        router->_parent = this->shared_from_this();
        router->_log = this->_log->add_child(router->_path);
        
        _routers.emplace(std::make_pair(router->_path, router));
        return this->shared_from_this();
    }
    

    virtual sp_router register_policy(policies::filter_policy pol)
    {
        this->_log->info(
            "Registering policy for router '{}'",
            this->_path
        );
        _policies.emplace_back(pol);
        return this->shared_from_this();
    }
    
    virtual sp_router register_policy(
        const md::string_view& route_path, policies::filter_policy pol)
    {
        return register_policy("ALL", route_path, pol);
    }
    
    virtual sp_router register_policy(
        evmvc::method method,
        const md::string_view& route_path,
        policies::filter_policy pol)
    {
        md::string_view vs = evmvc::to_string(method);
        return register_policy(vs, route_path, pol);
    }
    
    virtual sp_router register_policy(
        const md::string_view& method,
        const md::string_view& route_path,
        policies::filter_policy pol)
    {
        this->_log->info(
            "Registering policy for route [{}] '{}'",
            method.data(), route_path.data()
        );
        auto rp = resolve_route(method, route_path);
        if(!rp)
            rp = register_route(method, route_path);
        
        if(pol)
            rp->register_policy(pol);
        return this->shared_from_this();
    }
    
    void validate_access(
        evmvc::policies::filter_rule_ctx ctx,
        evmvc::policies::validation_cb cb)
    {
        if(_parent)
            return _parent->validate_access(ctx,
            [self = this->shared_from_this(), ctx, cb](const md::callback::cb_error& err){
                if(err)
                    return cb(err);
                
                if(self->_policies.empty())
                    return cb(nullptr);
                self->_validate_router_policies(
                    policies::filter_type::access, 0, ctx, cb
                );
            });
        
        if(_policies.empty())
            return cb(nullptr);
        _validate_router_policies(policies::filter_type::access, 0, ctx, cb);
    }
    
protected:
    
    static std::string _norm_path(const md::string_view& path)
    {
        if(path.size() == 0 || path == "/")
            return "/";
        
        std::string p(path);
        md::replace_substring(p, "//", "");
        if(p[0] == '/')
            p = p.substr(1);
        if(p[p.size() -1] != '/')
            p += "/";
        
        return p;
    }
    
    virtual sp_router register_handler(
        evmvc::method method,
        const md::string_view& route_path,
        route_handler_cb cb,
        policies::filter_policy pol)
    {
        md::string_view vs = evmvc::to_string(method);
        return register_handler(vs, route_path, cb, pol);
    }
    
    virtual sp_router register_handler(
        const md::string_view& method,
        const md::string_view& route_path,
        route_handler_cb cb,
        policies::filter_policy pol)
    {
        this->_log->info(
            "Registering handler for route [{}] '{}'",
            method.data(), route_path.data()
        );
        auto rp = resolve_route(method, route_path);
        if(!rp)
            rp = register_route(method, route_path);
        
        if(pol)
            rp->register_policy(pol);
        rp->register_handler(cb);
        return this->shared_from_this();
    }

    virtual sp_route register_route(
        evmvc::method method,
        const md::string_view& route_path)
    {
        md::string_view vs = evmvc::to_string(method);
        return register_route(vs, route_path);
    }
    
    virtual sp_route register_route(
        const md::string_view& method,
        const md::string_view& route_path)
    {
        this->_log->info(
            "Registering route [{}] '{}'",
            method.data(), route_path.data()
        );
        
        auto rm = _verbs.find(std::string(method));
        if(rm == _verbs.end()){
            _verbs.emplace(std::make_pair(method, route_map()));
            rm = _verbs.find(std::string(method));
        }
        
        sp_route r = std::make_shared<evmvc::route>(
            this->shared_from_this(), route_path
        );
        rm->second.emplace(std::make_pair(route_path, r));
        return r;
    }
    
    void _validate_router_policies(
        policies::filter_type type,
        size_t idx,
        evmvc::policies::filter_rule_ctx ctx,
        evmvc::policies::validation_cb cb)
    {
        if(idx >= _policies.size())
            return cb(nullptr);
        
        _policies[idx]->validate(
            type,
            ctx,
        [self = this->shared_from_this(), type, ctx, idx, cb]
        (const md::callback::cb_error& err) mutable {
            if(err)
                return cb(err);
            if(++idx >= self->_policies.size())
                return cb(nullptr);
            
            self->_validate_router_policies(type, idx, ctx, cb);
        });
    }
    
    evmvc::wp_app _app;
    std::string _path;
    mutable md::log::sp_logger _log;
    sp_router _parent;
    
    std::vector<policies::filter_policy> _policies;
    
    // // if the first matching router_path is return
    // // if is false and more than one router_path is found,
    // // it will throw an error.
    // boost::tribool _match_first;
    // // if routes "/abc/123" and "/abc/123/" are different.
    // boost::tribool _match_strict;
    // // if route case is sensitive.
    // boost::tribool _match_case;
    
    verb_map _verbs;
    router_map _routers;
    
};// class router

inline void route::validate_access(
    evmvc::policies::filter_rule_ctx ctx,
    evmvc::policies::validation_cb cb)
{
    get_router()->validate_access(ctx,
    [self = this->shared_from_this(), ctx, cb](const md::callback::cb_error& err){
        if(err)
            return cb(err);
        
        if(self->_policies.empty())
            return cb(nullptr);
        self->_validate_route_policies(
            policies::filter_type::access, 0, ctx, cb
        );
    });
}

inline void route_result::validate_access(
        evmvc::policies::filter_rule_ctx ctx,
        evmvc::policies::validation_cb cb)
{
    // first validate router access
    auto rt = this->_route;
    auto rtr = rt->get_router();
    
    rt->validate_access(ctx, cb);
    
    // rt->validate_access(ctx, 
    // [rt, rtr](const md::callback::cb_error& err){
    //     if(err)
    //         return cb(err);
    //         
    // });
}


class file_route_result
    : public route_result
{
public:
    file_route_result(evmvc::sp_route rt, evmvc::sp_router rtr)
        : route_result(rt), _rtr(rtr),
        _filepath(), _local_url(""),
        _not_found(true)
    {
        EVMVC_DEF_TRACE("file_route_result {:p} created", (void*)this);
    }
    
    file_route_result(evmvc::sp_route rt, evmvc::sp_router rtr,
        bfs::path filepath, const std::string& local_url)
        : route_result(rt), _rtr(rtr), 
        _filepath(filepath), _local_url(local_url), _not_found(false)
    {
        EVMVC_DEF_TRACE("file_route_result {:p} created", (void*)this);
    }
    
    ~file_route_result()
    {
        EVMVC_DEF_TRACE("file_route_result {:p} released", (void*)this);
    }
    
protected:
    void execute(
        evmvc::sp_response res, md::callback::async_cb cb)
    {
        auto rt = sp_route( new route(_rtr));
        rt->_rp = _rtr->path().to_string() + "/" + _local_url;
        
        if(_not_found){
            res->error(evmvc::status::not_found, MD_ERR(""));
            if(cb)
                cb(nullptr);
            return;
        }
        
        res->send_file(_filepath, "utf-8", cb);
    }
    
    evmvc::sp_router _rtr;
    bfs::path _filepath;
    std::string _local_url;
    bool _not_found;
};

class file_route
    : public route
{
public:
    file_route(std::weak_ptr<router> rtr)
        : route(rtr)
    {
        EVMVC_DEF_TRACE("file_route {:p} created", (void*)this);
    }
    
    ~file_route()
    {
        EVMVC_DEF_TRACE("file_route {:p} released", (void*)this);
    }
};

class file_router
    : public router
{
public:
    file_router(
        evmvc::wp_app app,
        const bfs::path& base_path,
        const md::string_view& virt_path)
        : router(app, virt_path),
        _base_path(bfs::absolute(base_path)), _rt()
    {
        EVMVC_DEF_TRACE("file_router {:p} created", (void*)this);
    }
    
    ~file_router()
    {
        EVMVC_DEF_TRACE("file_router {:p} released", (void*)this);
    }
    
    sp_route_result resolve_url(
        const md::string_view& method,
        const md::string_view& url)
    {
        std::string local_url = 
            std::string(url).substr(_path.size());
        
        boost::system::error_code ec;
        bfs::path file_path = 
            bfs::canonical(
                _base_path / bfs::path(local_url.data()),
                ec
            );
        
        if(!_rt)
            _rt = std::make_shared<file_route>(
                this->shared_from_this()
            );
        
        if(ec)
            return nullptr;
        
        return std::static_pointer_cast<route_result>(
            std::make_shared<file_route_result>(
                _rt, this->shared_from_this(),
                file_path, local_url
            )
        );
    }
    
protected:
    
    sp_router register_handler(
        const md::string_view& method,
        const md::string_view& route_path,
        route_handler_cb cb)
    {
        throw MD_ERR("Can't add new handler on file_router!");
    }
    
    bfs::path _base_path;
    std::shared_ptr<file_route> _rt;
};



} //ns evmvc
#endif //_libevmvc_router_h
