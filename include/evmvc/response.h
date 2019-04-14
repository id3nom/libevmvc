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

#ifndef _libevmvc_response_h
#define _libevmvc_response_h

#include "stable_headers.h"
#include "utils.h"
#include "url.h"
#include "statuses.h"
#include "headers.h"
#include "fields.h"
#include "mime.h"
#include "cookies.h"
#include "request.h"
#include "response_data.h"

#include <boost/filesystem.hpp>

#define EVMVC_RES_VIEW_DATA_SET_TYPE(T, tos_data) \
    void set_data(md::string_view name, T data) \
    { \
        auto it = _view_data->find(name.to_string()); \
        if(it == _view_data->end()) \
            _view_data->emplace( \
                std::make_pair( \
                    name.to_string(), \
                    view_data( \
                        new view_data_t(tos_data) \
                    ) \
                ) \
            ); \
        else \
            it->second = view_data( \
                new view_data_t(tos_data) \
            ); \
    }

namespace evmvc {


class response
    : public std::enable_shared_from_this<response>
{
    friend class connection;
    friend class http_parser;
    
public:
    
    response(
        uint64_t id,
        sp_request req,
        wp_connection conn,
        md::log::sp_logger log,
        const sp_route& rt,
        url uri,
        const sp_http_cookies& http_cookies)
        : _id(id),
        _req(req),
        _conn(conn),
        _log(log->add_child(
            "res-" + md::num_to_str(id, false) + uri.path()
        )),
        _rt(rt),
        _headers(std::make_shared<response_headers>()),
        _cookies(http_cookies),
        _started(false), _ended(false),
        _status(-1), _type(""), _enc(""),
        _paused(false),
        _resuming(false),
        //_view_data(std::make_shared<evmvc::view_data_map_t>()),
        _res_data(std::make_shared<evmvc::response_data_map_t>())
    {
        EVMVC_DEF_TRACE("response {} {:p} created", _id, (void*)this);
    }
    
    ~response()
    {
        EVMVC_DEF_TRACE("response {} {:p} released", _id, (void*)this);
    }
    
    uint64_t id() const { return _id;}
    
    sp_connection connection() const;
    bool secure() const;
    
    evmvc::sp_app get_app() const;
    evmvc::sp_router get_router()const;
    evmvc::sp_route get_route()const { return _rt;}
    md::log::sp_logger log() const { return _log;}
    
    evmvc::response_headers& headers() const { return *(_headers.get());}
    http_cookies& cookies() const { return *(_cookies.get());}
    evmvc::sp_request req() const { return _req;}
    
    bool paused() const { return _paused;}
    void pause();
    void resume(md::callback::async_cb cb)
    {
        if(!_paused || _resuming){
            _log->warn(MD_ERR(
                "SHOULD NOT RESUME, is paused: {}, is resuming: {}",
                _paused ? "true" : "false",
                _resuming ? "true" : "false"
            ));
            return;
        }
        _resume_cb = cb;
        resume();
    }
    void resume();
    
    bool started(){ return _started;};
    bool ended(){ return _ended;}
    void end()
    {
        if(!_started)
            this->encoding("utf-8").type("txt")._reply_start();
        if(_ended){
            _log->error(MD_ERR(
                "MUST NOT END, ended: true"
            ));
            return;
        }
        this->_reply_end();
    }
    

    
    int16_t get_status()
    {
        return _status == -1 ? (int16_t)evmvc::status::ok : _status;
    }
    
    response& status(evmvc::status code)
    {
        _status = (uint16_t)code;
        return *this;
    }
    
    response& status(uint16_t code)
    {
        _status = code;
        return *this;
    }
    
    bool has_encoding(){ return !_enc.empty();}
    md::string_view encoding(){ return _enc;}
    response& encoding(md::string_view enc)
    {
        _enc = enc.to_string();
        return *this;
    }
    
    bool has_type(){ return !_type.empty();}
    md::string_view get_type()
    {
        return _type.empty() ? "" : _type;
    }
    
    response& type(md::string_view type, md::string_view enc = "")
    {
        _type = evmvc::mime::get_type(type);
        if(_type.empty())
            _type = type.data();
        if(enc.size() > 0)
            _enc = enc.to_string();
        
        std::string ct = _type;
        if(!_enc.empty())
            ct += "; charset=" + _enc;
        this->headers().set(evmvc::field::content_type, ct);
        
        return *this;
    }
    
    void send_status(evmvc::status code)
    {
        this->status((uint16_t)code)
            .send(evmvc::statuses::status((uint16_t)code));
    }
    
    void send_status(uint16_t code)
    {
        this->status(code);
        this->send(evmvc::statuses::status(code));
    }
    
    void send(md::string_view body)
    {
        if(this->_paused)
            this->resume();
        
        if(_type.empty())
            this->type("txt", "utf-8");
        
        this->headers().set(
            evmvc::field::content_length,
            md::num_to_str(body.size())
        );
        
        _reply_start();
        this->_reply_raw(body.data(), body.size());
        this->end();
    }
    
    void html(md::string_view html_val)
    {
        this->encoding("utf-8").type("html").send(html_val);
    }
    
    template<
        typename T, 
        typename std::enable_if<
            std::is_same<T, evmvc::json>::value,
            int32_t
        >::type = -1
    >
    void send(const T& json_val)
    {
        this->encoding("utf-8").type("json").send(json_val.dump());
    }
    
    void json(const evmvc::json& json_val)
    {
        this->encoding("utf-8").type("json").send(json_val.dump());
    }
    
    void jsonp(
        const evmvc::json& json_val,
        bool escape = false,
        md::string_view cb_name = "callback"
        )
    {
        if(!this->has_encoding())
            this->encoding("utf-8");
        if(!this->has_type()){
            this->headers().set("X-Content-Type-Options", "nosniff");
            this->type("text/javascript");
        }
        
        std::string sv = json_val.dump();
        md::replace_substring(sv, "\u2028", "\\u2028");
        md::replace_substring(sv, "\u2029", "\\u2029");
        
        sv = "/**/ typeof " + cb_name.to_string() + " === 'function' && " +
            cb_name.to_string() + "(" + sv + ");";
        this->send(sv);
    }
    
    void download(const bfs::path& filepath,
        md::string_view filename = "",
        const md::string_view& enc = "utf-8", 
        md::callback::async_cb cb = md::callback::noop_cb)
    {
        this->headers().set(
            evmvc::field::content_disposition,
            fmt::format(
                "attachment; filename={}",
                filename.size() == 0 ?
                    filepath.filename().c_str() :
                    filename.data()
            )
        );
        this->send_file(filepath, enc, cb);
    }
    
    void send_file(
        const bfs::path& filepath,
        const md::string_view& enc = "utf-8", 
        md::callback::async_cb cb = md::callback::noop_cb
    );
    
    void error(const md::callback::cb_error& err)
    {
        this->error(evmvc::status::internal_server_error, err);
    }
    void error(evmvc::status err_status, const md::callback::cb_error& err);
    
    void redirect(md::string_view new_location,
        evmvc::status redir_status = evmvc::status::found)
    {
        //"See 'https://developer.mozilla.org/en-US/docs/Web/HTTP/
        switch (redir_status){
            // Permanent redirections
            case evmvc::status::moved_permanently:
            case evmvc::status::permanent_redirect:
            
            // Temporary redirections
            case evmvc::status::found:
            case evmvc::status::see_other:
            case evmvc::status::temporary_redirect:
            
            // Special redirections
            case evmvc::status::multiple_choices:
            case evmvc::status::not_modified:
                
                this->headers().set(evmvc::field::location, new_location);
                this->send_status(redir_status);
                break;
            
            default:
                throw MD_ERR(
                    "Invalid redirection status!"
                );
        }
    }


    // ===========
    // == views ==
    // ===========
    template<typename T>
    void set_data(md::string_view name, T data)
    {
        auto it = _res_data->find(name.to_string());
        if(it == _res_data->end()){
            _res_data->emplace(
                std::make_pair(
                    name.to_string(),
                    response_data<T>(
                        new response_data_t<T>(data)
                    )
                )
            );
        }else{
            it->second = response_data<T>(
                new response_data_t<T>(data)
            );
        }
    }
    
    template<typename T>
    T get_data(md::string_view name, const T& def_val)
    {
        auto it = _res_data->find(name.to_string());
        if(it == _res_data->end())
            return def_val;
        return std::static_pointer_cast<response_data_t<T>>(
            it->second
        )->value();
    }
    
    template<typename T>
    T get_data(md::string_view name)
    {
        auto it = _res_data->find(name.to_string());
        if(it == _res_data->end())
            throw MD_ERR("Data '{}' not found!", name);
        return std::static_pointer_cast<response_data_t<T>>(
            it->second
        )->value();
    }
    
    std::string get_data(md::string_view name, const std::string& def_val)
    {
        auto it = _res_data->find(name.to_string());
        if(it == _res_data->end())
            return def_val;
        return it->second->to_string();
    }
    
    std::string get_data(md::string_view name)
    {
        auto it = _res_data->find(name.to_string());
        if(it == _res_data->end())
            throw MD_ERR("Data '{}' not found!", name);
        return it->second->to_string();
    }
    
    
    void render(const std::string& view_path, md::callback::async_cb cb);
    
    
    
private:
    
    void _resume(md::callback::cb_error err)
    {
        if(!_paused || !_resuming){
            _log->warn(MD_ERR(
                "SHOULD NOT RESUME, is paused: {}, is resuming: {}",
                _paused ? "true" : "false",
                _resuming ? "true" : "false"
            ));
            return;
        }
        
        _paused = _resuming = false;
        if(_resume_cb)
            _resume_cb(err);
        _resume_cb = nullptr;
    }
    
    void _prepare_headers();
    
    void _reply_start()
    {
        if(_started){
            _log->error(MD_ERR(
                "MUST NOT _reply_start, started: true"
            ));
            return;
        }
        EVMVC_TRACE(_log, "_reply_start");
        
        _prepare_headers();
        _started = true;
    }
    
    void _reply_end();
    
    void _reply_raw(const char* data, size_t len);
    
    uint64_t _id;
    evmvc::sp_request _req;
    wp_connection _conn;
    md::log::sp_logger _log;
    sp_route _rt;
    evmvc::sp_response_headers _headers;
    sp_http_cookies _cookies;
    bool _started;
    bool _ended;
    int16_t _status;
    std::string _type;
    std::string _enc;
    bool _paused;
    bool _resuming;
    md::callback::async_cb _resume_cb;
    
    evmvc::response_data_map _res_data;
};


} //ns evmvc
#endif //_libevmvc_response_h
