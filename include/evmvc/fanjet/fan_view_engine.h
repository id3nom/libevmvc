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
        const evmvc::response& res
    )> view_generator_fn;

class view_engine
    : public evmvc::view_engine
{
    friend class app;
public:

    view_engine(
        const std::string& ns)
        : evmvc::view_engine(ns)
    {
    }

    /**
     * View Engine name
     */
    md::string_view name() const { return EVMVC_FANJET_VIEW_ENGINE_NAME;};

    void render_view(
        const evmvc::response& res,
        const std::string& path,
        md::callback::value_cb<const std::string&> cb)
    {
        md::log::default_logger()->debug(MD_ERR(
            "request view at '{}'",
            path
        ));

        view_generator_fn vg = find_generator(path);
        if(vg == nullptr){
            cb(MD_ERR(
                "Engine '{}' is unable to locate view at: '{}::{}'",
                this->name(), this->ns(), path
            ), "");
            return;
        }
        std::shared_ptr<evmvc::view_base> v = vg(this->shared_from_this(), res);

        std::vector<std::shared_ptr<evmvc::view_base>> views;
        views.emplace_back(v);

        std::shared_ptr<evmvc::view_base> tv = v;

        while(tv && !tv->layout().empty()){
            std::string s = tv->layout().to_string();
            if(s.find("::") == std::string::npos){
                // first find with relative path
                std::string rel_path = path;
                size_t rplsep = rel_path.rfind("/");
                if(rplsep == std::string::npos)
                    rel_path.clear();
                else
                    rel_path = rel_path.substr(0, rplsep +1);
                tv = this->get_view(res, rel_path + s);
                // then find with path
                if(!tv)
                    tv = this->get_view(res, s);
            }else
                tv = evmvc::view_engine::get(res, s);

            if(tv){
                // assign current view body
                tv->set_body(*views.rbegin());
                views.emplace_back(tv);
            }
        }

        // fetch the upper layout
        v = *views.rbegin();

        // render all views starting with the fartest child
        md::async::each<std::shared_ptr<evmvc::view_base>>(views,
        [views, res](
            std::shared_ptr<evmvc::view_base> v,
            md::callback::async_cb ecb
        ){
            try{
                md::log::default_logger()->debug(MD_ERR(
                    "rendering view '{}'",
                    v->abs_path()
                ));
                v->render(v, [v, ecb](const md::callback::cb_error& err){
                    if(err){
                        md::log::default_logger()->error(MD_ERR(
                            "view '{}' rendering failed!{}",
                            v->abs_path(),
                            err
                        ));
                    }else{
                        md::log::default_logger()->debug(MD_ERR(
                            "view '{}' rendering succeeded",
                            v->abs_path()
                        ));
                    }
                    ecb(err);
                });
            }catch(const std::exception& err){
                ecb(err);
            }
        },
        // on completion return view html data
        [views, res, v, cb](const md::callback::cb_error& err){
            if(err)
                return cb(err, "");

            md::log::default_logger()->debug(MD_ERR(
                "view '{}' rendered",
                v->abs_path()
            ));
            cb(nullptr, v->buffer());
        });

    }

    std::shared_ptr<evmvc::view_base> get_view(
        const evmvc::response& res,
        const std::string& path)
    {
        view_generator_fn vg = find_generator(path);
        if(!vg)
            return nullptr;
        return vg(this->shared_from_this(), res);
    }

    bool view_exists(const std::string& view_path) const
    {
        return find_generator(view_path) != nullptr;
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
    view_generator_fn find_generator(
        const std::string& path) const
    {
        if(path.empty())
            throw MD_ERR(
                "path is empty!"
            );

        //TODO: isolate per namespace
        // std::string ns;
        // size_t nsp = path.rfind("::");
        // if(nsp == std::string::npos)
        //     ns = doc->ns;

        std::vector<std::string> parts;
        std::string p = path;
            //*path.rbegin() == '/' ? path.substr(1) : doc->path + path;
        boost::split(parts, p, boost::is_any_of("/"));

        std::string name = *parts.rbegin();

        // std::vector<view_generator_fn> ds;
        // for(auto d : docs)
        //     if(d->name == name)
        //         ds.emplace_back(d);

        // if(ds.empty())
        //     throw MD_ERR(
        //         "No view matching path: '{}'", path
        //     );

        for(ssize_t i = (ssize_t)parts.size() -1; i >= 0; --i){
            // look for the file in this order:
            //  path dir,
            //  partials dir,
            //  layouts dir,
            //  helpers dir,

            std::string rp;
            for(ssize_t j = 0; j < i; ++j)
                rp += parts[j] + "/";

            for(auto& v : _views){
                if(v.first == rp + name)
                    return v.second;

                if(v.first == rp + "partials/" + name)
                    return v.second;

                if(v.first == rp + "layouts/" + name)
                    return v.second;

                if(v.first == rp + "helpers/" + name)
                    return v.second;

                // if(d->path == rp)
                //     return d;

                // if(d->path == rp + "partials/")
                //     return d;

                // if(d->path == rp + "layouts/")
                //     return d;

                // if(d->path == rp + "helpers/")
                //     return d;
            }
        }

        // throw MD_ERR(
        //     "No view matching path: '{}'", path
        // );
        return nullptr;
    }

    std::unordered_map<std::string, view_generator_fn> _views;

};

}};//evmvc::fanjet
#endif //_libevmvc_fanjet_view_engine_h
