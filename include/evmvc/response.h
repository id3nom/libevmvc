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

extern "C" {
#define EVHTP_DISABLE_REGEX
#include <event2/http.h>
#include <evhtp/evhtp.h>
}

namespace evmvc {

class response
    : public std::enable_shared_from_this<response>
{
public:
    
    response(evhtp_request_t* ev_req)
        : _ev_req(ev_req),
        _started(false), _ended(false),
        _status(-1), _type(""), _enc("")
    {
    }

    bool started(){ return _started;};
    bool ended(){ return _ended;}
    void end()
    {
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
    response& set(const std::map<K, V>& m)
    {
        for(const auto& it = m.begin(); it < m.end(); ++it)
            this->set(it->first, it->second);
        return *this;
    }
    
    response& set(const evmvc::json& key_val)
    {
        for(const auto& el : key_val.items())
            this->set(el.key(), el.value());
        return *this;
    }
    
    response& set(evmvc::field header_name, evmvc::string_view header_val)
    {
        return set(to_string(header_name), header_val);
    }
    
    response& set(evmvc::string_view header_name, evmvc::string_view header_val)
    {
        evhtp_kv_t* header = nullptr;
        if((header = evhtp_headers_find_header(
            _ev_req->headers_out, header_name.data()
        )) != nullptr)
            evhtp_header_rm_and_free(_ev_req->headers_out, header);
        
        header = evhtp_header_new(header_name.data(), header_val.data(), 1, 1);
        evhtp_headers_add_header(_ev_req->headers_out, header);
        
        return *this;
    }
    
    evmvc::string_view get(evmvc::field header_name)
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
    
    response& remove_header(evmvc::field header_name)
    {
        return remove_header(to_string(header_name));
    }
    
    response& remove_header(evmvc::string_view header_name)
    {
        evhtp_kv_t* header = nullptr;
        if((header = evhtp_headers_find_header(
            _ev_req->headers_out, header_name.data()
        )) != nullptr)
            evhtp_header_rm_and_free(_ev_req->headers_out, header);
        
        return *this;
    }
    
    int16_t get_status()
    {
        return _status == -1 ? 200 : _status;
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
    
    evmvc::string_view encoding(){ return _enc;}
    response& encoding(evmvc::string_view enc)
    {
        _enc = enc.to_string();
        return *this;
    }
    
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
        this->status((uint16_t)code);
        this->send(evmvc::statuses::status((uint16_t)code));
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
        
        if(evhtp_request_get_proto(_ev_req) == EVHTP_PROTO_10)
            evhtp_request_set_keepalive(_ev_req, 0);
        
        //evbuffer_add_printf(_ev_req->buffer_out, "%s", body.data());
        
        struct evbuffer* b = evbuffer_new();
        
        int len = evbuffer_add_printf(b, "%s", body.data());
        this->set(evmvc::field::content_length, evmvc::num_to_str(len));
        //this->set(evmvc::field::encoding, "utf-8");
        
        _start_reply();
        evhtp_send_reply_body(_ev_req, b);
        
        evbuffer_free(b);
        
        this->end();
    }
    
    void download(evmvc::string_view path, evmvc::string_view filename)
    {
        this->set(evmvc::field::content_disposition, filename);
        this->send(path);
    }
    
    
    void send_bad_request(evmvc::string_view body)
    {
        this->status(evmvc::status::bad_request).send(body);
    }
    
    void send_not_found(evmvc::string_view body)
    {
        this->status(evmvc::status::not_found).send(body);
    }
    
    
private:
    void _start_reply()
    {
        _started = true;
        // look for keepalive
        evhtp_kv_t* header = nullptr;
        if((header = evhtp_headers_find_header(
            _ev_req->headers_in, "Connection"
        )) != nullptr){
            std::string con_val(header->val);
            evmvc::trim(con_val);
            evmvc::lower_case(con_val);
            if(con_val == "keep-alive")
                this->set(evmvc::field::connection, "keep-alive");
        }
        evhtp_send_reply_start(_ev_req, _status);
    }
    
    evhtp_request_t* _ev_req;
    bool _started;
    bool _ended;
    int16_t _status;
    std::string _type;
    std::string _enc;
    
};


} //ns evmvc
#endif //_libevmvc_response_h