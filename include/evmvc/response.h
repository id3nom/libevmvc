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
#include "statuses.h"
#include "fields.h"
#include "mime.h"
#include "cookies.h"

#include <boost/filesystem.hpp>

namespace evmvc {
namespace _internal {
    struct file_reply {
        evhtp_request_t* request;
        FILE* file_desc;
        struct evbuffer* buffer;
        evmvc::async_cb cb;
    };
    
    static evhtp_res send_file_chunk(evhtp_connection_t* conn, void* arg)
    {
        struct file_reply* reply = (struct file_reply*)arg;
        char buf[4000];
        size_t bytes_read;
        
        /* try to read 4000 bytes from the file pointer */
        bytes_read = fread(buf, 1, sizeof(buf), reply->file_desc);
        
        std::clog << fmt::format("Sending {0} bytes\n", bytes_read);
        
        if (bytes_read > 0) {
            /* add our data we read from the file into our reply buffer */
            evbuffer_add(reply->buffer, buf, bytes_read);

            /* send the reply buffer as a http chunked message */
            evhtp_send_reply_chunk(reply->request, reply->buffer);

            /* we can now drain our reply buffer as to not be a resource
            * hog.
            */
            evbuffer_drain(reply->buffer, bytes_read);
        }

        /* check if we have read everything from the file */
        if(feof(reply->file_desc)){
            std::clog << fmt::format("Sending last chunk\n");
            
            /* now that we have read everything from the file, we must
            * first unset our on_write hook, then inform evhtp to send
            * this message as the final chunk.
            */
            evhtp_connection_unset_hook(conn, evhtp_hook_on_write);
            evhtp_send_reply_chunk_end(reply->request);

            /* we can now free up our little reply_ structure */
            {
                fclose(reply->file_desc);
                
                evhtp_safe_free(reply->buffer, evbuffer_free);
                evhtp_safe_free(reply, free);
                
                // no need for the connection fini hook anymore
                evhtp_connection_unset_hook(
                    conn, evhtp_hook_on_connection_fini
                );
            }
        }
        
        return EVHTP_RES_OK;
    }
    
    static evhtp_res send_file_fini(
        struct evhtp_connection* c, void* arg)
    {
        struct file_reply* reply = (struct file_reply*)arg;
        fclose(reply->file_desc);
        evhtp_safe_free(reply->buffer, evbuffer_free);
        evhtp_safe_free(reply, free);
        
        return EVHTP_RES_OK;
    }
    
}

class response
    : public std::enable_shared_from_this<response>
{
public:
    
    response(const sp_app& app, evhtp_request_t* ev_req,
        const sp_http_cookies& http_cookies)
        : _app(app), _ev_req(ev_req), _cookies(http_cookies),
        _started(false), _ended(false),
        _status(-1), _type(""), _enc("")
    {
    }
    
    evmvc::sp_app app()const { return _app;}
    evhtp_request_t* evhtp_request(){ return _ev_req;}
    http_cookies& cookies() const { return *(_cookies.get());}
    sp_http_cookies shared_cookies() const { return _cookies;}
    
    bool started(){ return _started;};
    bool ended(){ return _ended;}
    void end()
    {
        if(!_started)
            this->encoding("utf-8").type("txt")._start_reply();
        
        evhtp_send_reply_end(_ev_req);
        _ended = true;
    }
    
    template<class K, class V,
        typename std::enable_if<
            (std::is_same<K, std::string>::value ||
                std::is_same<K, evmvc::field>::value) &&
            std::is_same<V, std::string>::value
        , int32_t>::type = -1
    >
    response& set(
        const std::map<K, V>& m,
        bool clear_existing = true)
    {
        for(const auto& it = m.begin(); it < m.end(); ++it)
            this->set(it->first, it->second, clear_existing);
        return *this;
    }
    
    
    template<
        typename T, 
        typename std::enable_if<
            std::is_same<T, evmvc::json>::value,
            int32_t
        >::type = -1
    >
    response& set(
        const T& key_val,
        bool clear_existing = true)
    {
        for(const auto& el : key_val.items())
            this->set(el.key(), el.value(), clear_existing);
        return *this;
    }
    
    response& set(
        evmvc::field header_name, evmvc::string_view header_val,
        bool clear_existing = true)
    {
        return set(to_string(header_name), header_val, clear_existing);
    }
    
    response& set(
        evmvc::string_view header_name, evmvc::string_view header_val,
        bool clear_existing = true)
    {
        evhtp_kv_t* header = nullptr;
        if(clear_existing)
            while((header = evhtp_headers_find_header(
                _ev_req->headers_out, header_name.data()
            )) != nullptr)
                evhtp_header_rm_and_free(_ev_req->headers_out, header);
        
        header = evhtp_header_new(header_name.data(), header_val.data(), 1, 1);
        evhtp_headers_add_header(_ev_req->headers_out, header);
        
        return *this;
    }
    
    evmvc::string_view get(evmvc::field header_name) const
    {
        return get(to_string(header_name));
    }
    
    evmvc::string_view get(evmvc::string_view header_name) const
    {
        evhtp_kv_t* header = nullptr;
        if((header = evhtp_headers_find_header(
            _ev_req->headers_out, header_name.data()
        )) != nullptr)
            return header->val;
        return nullptr;
    }
    
    std::vector<evmvc::string_view> list(evmvc::field header_name) const
    {
        return list(to_string(header_name));
    }
    
    std::vector<evmvc::string_view> list(evmvc::string_view header_name) const
    {
        std::vector<evmvc::string_view> vals;
        
        evhtp_kv_t* kv;
        TAILQ_FOREACH(kv, _ev_req->headers_out, next){
            if(strcmp(kv->key, header_name.data()) == 0)
                vals.emplace_back(kv->val);
        }
        
        return vals;
    }
    
    response& remove_header(evmvc::field header_name)
    {
        return remove_header(to_string(header_name));
    }
    
    response& remove_header(evmvc::string_view header_name)
    {
        evhtp_kv_t* header = nullptr;
        while((header = evhtp_headers_find_header(
            _ev_req->headers_out, header_name.data()
        )) != nullptr)
            evhtp_header_rm_and_free(_ev_req->headers_out, header);
        
        return *this;
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
        _type = evmvc::mime::get_type(type).to_string();
        if(_type.empty())
            _type = type.data();
        if(enc.size() > 0)
            _enc = enc.to_string();
        
        std::string ct = _type;
        if(!_enc.empty())
            ct += "; charset=" + _enc;
        this->set(evmvc::field::content_type, ct);
        
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
        if(_type.empty())
            this->type("txt", "utf-8");
        
        struct evbuffer* b = evbuffer_new();
        
        int len = evbuffer_add_printf(b, "%s", body.data());
        this->set(evmvc::field::content_length, evmvc::num_to_str(len));
        
        _start_reply();
        evhtp_send_reply_body(_ev_req, b);
        
        evbuffer_free(b);
        
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
            this->set("X-Content-Type-Options", "nosniff");
            this->type("text/javascript");
        }
        
        std::string sv = json_val.dump();
        evmvc::replace_substring(sv, "\u2028", "\\u2028");
        evmvc::replace_substring(sv, "\u2029", "\\u2029");
        
        sv = "/**/ typeof " + cb_name.to_string() + " === 'function' && " +
            cb_name.to_string() + "(" + sv + ");";
        this->send(sv);
    }
    
    void download(evmvc::string_view path, evmvc::string_view filename)
    {
        this->set(evmvc::field::content_disposition, filename);
        this->send(path);
    }
    
    
    void send_file(
        const boost::filesystem::path filepath,
        const evmvc::string_view& enc = "utf-8", 
        async_cb cb = evmvc::noop_cb)
    {
        FILE* file_desc = nullptr;
        struct evmvc::_internal::file_reply* reply = nullptr;
        
        // open up the file
        file_desc = fopen(filepath.c_str(), "r");
        BOOST_ASSERT(file_desc != nullptr);
        
        // create internal file_reply struct
        mm__alloc_(reply, struct evmvc::_internal::file_reply, {
            _ev_req, file_desc, evbuffer_new(), cb
        });
        
        /* here we set a connection hook of the type `evhtp_hook_on_write`
        *
        * this will execute the function `http__send_chunk_` each time
        * all data has been written to the client.
        */
        evhtp_connection_set_hook(
            _ev_req->conn, evhtp_hook_on_write,
            (evhtp_hook)evmvc::_internal::send_file_chunk,
            reply
        );
        
        /* set a hook to be called when the client disconnects */
        evhtp_connection_set_hook(
            _ev_req->conn, evhtp_hook_on_connection_fini,
            (evhtp_hook)evmvc::_internal::send_file_fini,
            reply
        );
        
        // set file content-type
        //TODO: get file encoding
        this->encoding(enc).type(filepath.extension().c_str());
        this->_prepare_headers();
        _started = true;
        
        /* we do not have to start sending data from the file from here -
        * this function will write data to the client, thus when finished,
        * will call our `http__send_chunk_` callback.
        */
        evhtp_send_reply_chunk_start(_ev_req, EVHTP_RES_OK);
    }
    
    void error(const cb_error& err)
    {
        this->error(evmvc::status::internal_server_error, err);
    }
    void error(evmvc::status err_status, const cb_error& err);
    
    void redirect(evmvc::string_view new_location,
        evmvc::status redir_status = evmvc::status::found)
    {
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
                
                this->set(evmvc::field::location, new_location);
                this->send_status(redir_status);
                break;
            
            default:
                throw EVMVC_ERR(
                    "Invalid redirection status!\nSee 'https://developer.mozilla.org/en-US/docs/Web/HTTP/Redirections' for valid statuses!"
                );
        }
    }
    
private:
    void _prepare_headers()
    {
        if(_started)
            throw std::runtime_error(
                "unable to prepare headers after response is started!"
            );
        
        // look for keepalive
        evhtp_kv_t* header = nullptr;
        if((header = evhtp_headers_find_header(
            _ev_req->headers_in, "Connection"
        )) != nullptr){
            std::string con_val(header->val);
            evmvc::trim(con_val);
            evmvc::lower_case(con_val);
            if(con_val == "keep-alive"){
                this->set(evmvc::field::connection, "keep-alive");
                evhtp_request_set_keepalive(_ev_req, 1);
            } else
                evhtp_request_set_keepalive(_ev_req, 0);
        } else if(evhtp_request_get_proto(_ev_req) == EVHTP_PROTO_10)
            evhtp_request_set_keepalive(_ev_req, 0);
    }
    
    void _start_reply()
    {
        _prepare_headers();
        _started = true;
        evhtp_send_reply_start(_ev_req, _status);
    }
    
    sp_app _app;
    evhtp_request_t* _ev_req;
    sp_http_cookies _cookies;
    bool _started;
    bool _ended;
    int16_t _status;
    std::string _type;
    std::string _enc;
    
};


} //ns evmvc
#endif //_libevmvc_response_h
