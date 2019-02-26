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
#include "logging.h"
#include "utils.h"
#include "url.h"
#include "statuses.h"
#include "headers.h"
#include "fields.h"
#include "mime.h"
#include "cookies.h"
#include "request.h"

#include <boost/filesystem.hpp>


namespace evmvc {


class response
    : public std::enable_shared_from_this<response>
{
    // friend evmvc::sp_response _internal::create_http_response(
    //     sp_logger log,
    //     evhtp_request_t* ev_req,
    //     sp_route rt,
    //     const std::vector<std::shared_ptr<evmvc::http_param>>& params
    //     );
    
public:
    
    response(
        uint64_t id,
        sp_request req,
        wp_connection conn,
        evmvc::sp_logger log,
        const sp_route& rt,
        url uri,
        const sp_http_cookies& http_cookies)
        : _id(id),
        _req(req),
        _conn(conn),
        _log(log->add_child(
            "res-" + evmvc::num_to_str(id, false) + uri.path()
        )),
        _rt(rt),
        _headers(std::make_shared<response_headers>()),
        _cookies(http_cookies),
        _started(false), _ended(false),
        _status(-1), _type(""), _enc(""), _paused(false)
    {
        EVMVC_DEF_TRACE(
            "response '{}' created", this->id()
        );
    }
    
    ~response()
    {
        EVMVC_DEF_TRACE(
            "response '{}' released", this->id()
        );
    }
    
    //static sp_response null(wp_app a, evhtp_request_t* ev_req);
    
    uint64_t id() const { return _id;}
    
    
    sp_connection connection() const;
    bool secure() const;
    
    evmvc::sp_app get_app() const;
    evmvc::sp_router get_router()const;
    evmvc::sp_route get_route()const { return _rt;}
    evmvc::sp_logger log() const { return _log;}
    
    //evhtp_request_t* evhtp_request(){ return _ev_req;}
    evmvc::response_headers& headers() const { return *(_headers.get());}
    http_cookies& cookies() const { return *(_cookies.get());}
    evmvc::sp_request req() const { return _req;}
    
    bool paused() const { return _paused;}
    void pause()
    {
        if(_paused)
            return;
        _paused = true;
        this->log()->debug("Connection paused");
        //evhtp_request_pause(_ev_req);
    }
    
    void resume(async_cb cb)
    {
        if(!_paused || _resuming){
            _log->warn(EVMVC_ERR(
                "SHOULD NOT RESUME, is paused: {}, is resuming: {}",
                _paused ? "true" : "false",
                _resuming ? "true" : "false"
            ));
            return;
        }
        _resume_cb = cb;
        resume();
    }
    void resume()
    {
        if(!_paused || _resuming){
            _log->warn(EVMVC_ERR(
                "SHOULD NOT RESUME, is paused: {}, is resuming: {}",
                _paused ? "true" : "false",
                _resuming ? "true" : "false"
            ));
            return;
        }
        _resuming = true;
        this->log()->debug("Resuming connection");
        //evhtp_request_resume(_ev_req);
    }
    
    bool started(){ return _started;};
    bool ended(){ return _ended;}
    void end()
    {
        if(!_started)
            this->encoding("utf-8").type("txt")._reply_start();
        
        //evhtp_send_reply_end(_ev_req);
        this->_reply_end();
        _ended = true;
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
    evmvc::string_view encoding(){ return _enc;}
    response& encoding(evmvc::string_view enc)
    {
        _enc = enc.to_string();
        return *this;
    }
    
    bool has_type(){ return !_type.empty();}
    evmvc::string_view get_type()
    {
        return _type.empty() ? "" : _type;
    }
    
    response& type(evmvc::string_view type, evmvc::string_view enc = "")
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
    
    void send(evmvc::string_view body)
    {
        this->resume();
        if(_type.empty())
            this->type("txt", "utf-8");
        
        this->headers().set(
            evmvc::field::content_length,
            evmvc::num_to_str(body.size())
        );
        
        _reply_start();
        this->_reply_raw(body.data(), body.size());
        this->end();
    }
    
    void html(evmvc::string_view html_val)
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
        evmvc::string_view cb_name = "callback"
        )
    {
        if(!this->has_encoding())
            this->encoding("utf-8");
        if(!this->has_type()){
            this->headers().set("X-Content-Type-Options", "nosniff");
            this->type("text/javascript");
        }
        
        std::string sv = json_val.dump();
        evmvc::replace_substring(sv, "\u2028", "\\u2028");
        evmvc::replace_substring(sv, "\u2029", "\\u2029");
        
        sv = "/**/ typeof " + cb_name.to_string() + " === 'function' && " +
            cb_name.to_string() + "(" + sv + ");";
        this->send(sv);
    }
    
    void download(const bfs::path& filepath,
        evmvc::string_view filename = "",
        const evmvc::string_view& enc = "utf-8", 
        async_cb cb = evmvc::noop_cb)
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
        const evmvc::string_view& enc = "utf-8", 
        async_cb cb = evmvc::noop_cb
    );
    
    void error(const cb_error& err)
    {
        this->error(evmvc::status::internal_server_error, err);
    }
    void error(evmvc::status err_status, const cb_error& err);
    
    void redirect(evmvc::string_view new_location,
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
                throw EVMVC_ERR(
                    "Invalid redirection status!"
                );
        }
    }
    
private:

    void _prepare_headers();
    
    void _reply_start()
    {
        if(_started){
            _log->error(EVMVC_ERR(
                "MUST NOT _reply_start, started: true"
            ));
            return;
        }
        _prepare_headers();
        _started = true;
        //evhtp_send_reply_start(_ev_req, _status);
    }
    
    void _reply_end()
    {
        
    }
    
    void _reply_raw(const char* data, size_t len);
    
    uint64_t _id;
    wp_connection _conn;
    evmvc::sp_logger _log;
    sp_route _rt;
    evmvc::sp_response_headers _headers;
    evmvc::sp_request _req;
    sp_http_cookies _cookies;
    bool _started;
    bool _ended;
    int16_t _status;
    std::string _type;
    std::string _enc;
    bool _paused;
    bool _resuming;
    async_cb _resume_cb;
};


} //ns evmvc
#endif //_libevmvc_response_h
