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

#include "app.h"
#include "connection.h"
#include "response.h"

namespace evmvc {

sp_connection response::connection() const { return _conn.lock();}
bool response::secure() const { return _conn.lock()->secure();}

evmvc::sp_app response::get_app() const
{
    return this->get_router()->get_app();
}
evmvc::sp_router response::get_router() const
{
    return this->get_route()->get_router();
}

void response::error(evmvc::status err_status, const cb_error& err)
{
    auto c = this->_conn.lock();
    
    //auto con_addr_in = (sockaddr_in*)this->_ev_req->conn->saddr;
    std::string log_val = fmt::format(
        "[{}{}] [{}] [{}] [Status {} {}, {}]\n{}",
        c->address(),
        c->address().find("unix:") == 0 ? "" : ":" + std::to_string(c->port()),

        to_string(_req->method()).data(),

        _req->uri()->to_string(),
        
        (int16_t)err_status,
        evmvc::statuses::category(err_status).data(),
        evmvc::statuses::status(err_status).data(),
        
        err.c_str()
    );
    
    char* what = evhttp_htmlescape(err.c_str());
    std::string err_msg = fmt::format(
        "<!DOCTYPE html><html><head><title>"
        "LIBEVMVC Error</title></head><body>"
        "<table>\n<tr><td style='background-color:{0};font-size:1.1em;'>"
            "<b>Error summary</b>"
        "</td></tr>\n"
        "<tr><td style='color:{0};'><b>Error {1}, {2}</b></td></tr>\n"
        "<tr><td style='color:{0};'>{3}</td></tr>\n",
        evmvc::statuses::color(err_status).data(),
        (int16_t)err_status,
        evmvc::statuses::status(err_status).data(),
        what
    );
    free(what);
    
    evmvc::sp_app a = this->get_route()->get_router()->get_app();
    if(a->options()
        .stack_trace_enabled && err.has_stack()
    ){
        log_val += fmt::format(
            "\nAdditional info\n\n{}:{}\n{}\n\n{}\n",
            err.file(), err.line(), err.func(), err.stack()
        );
        
        char* file = evhttp_htmlescape(err.file().c_str());
        char* func = evhttp_htmlescape(err.func().c_str());
        char* stack = evhttp_htmlescape(err.stack().c_str());
        
        err_msg += fmt::format(
            "<tr><td>&nbsp;</td></tr>\n"
            "<tr><td style='"
                "border-bottom: 1px solid black;"
                "font-size:1.1em;"
                "'>"
                "<b>Additional info</b>"
            "</td></tr>\n"
            "<tr><td><pre>"
            "\n\n{}:{}\n{}\n\n{}"
            "</pre></td></tr>\n",
            file, err.line(), func, stack
        );
        
        free(file);
        free(func);
        free(stack);
    }
    
    err_msg += fmt::format(
        "<tr><td style='"
        "border-top: 1px solid black; font-size:0.9em'>{}"
        "</td></tr>\n",
        evmvc::html_version()
    );
    
    err_msg += "</table></body></html>";
    
    if((int16_t)err_status < 400)
        this->log()->info(log_val);
    else if((int16_t)err_status < 500)
        this->log()->warn(log_val);
    else
        this->log()->error(log_val);
    
    this->status(err_status).html(err_msg);
}


void response::_prepare_headers()
{
    if(_started)
        throw std::runtime_error(
            "unable to prepare headers after response is started!"
        );
    // prepare response line
    throw EVMVC_ERR("TODO: _prepare_headers ...");
    
    // lookfor keepalive header
    auto conh = _req->headers().get("Connection");
    if(conh && !strcasecmp(conh->value(), ""))
        _conn->lock()->keep_alive(true);
    
    
    
    // // look for keepalive
    // evhtp_kv_t* header = nullptr;
    // if((header = evhtp_headers_find_header(
    //     _ev_req->headers_in, "Connection"
    // )) != nullptr){
    //     std::string con_val(header->val);
    //     evmvc::trim(con_val);
    //     evmvc::lower_case(con_val);
    //     if(con_val == "keep-alive"){
    //         this->headers().set(evmvc::field::connection, "keep-alive");
    //         evhtp_request_set_keepalive(_ev_req, 1);
    //     } else
    //         evhtp_request_set_keepalive(_ev_req, 0);
    // } else if(evhtp_request_get_proto(_ev_req) == EVHTP_PROTO_10)
    //     evhtp_request_set_keepalive(_ev_req, 0);
}

void response::_reply_raw(const char* data, size_t len)
{
    if(auto c = this->_conn->lock())
        bufferevent_write(c->bev(), data, len);
}

void response::send_file(
    const bfs::path& filepath,
    const evmvc::string_view& enc, 
    async_cb cb)
{
    FILE* file_desc = nullptr;
    //struct evmvc::_internal::file_reply* reply = nullptr;
    
    // open up the file
    file_desc = fopen(filepath.c_str(), "r");
    BOOST_ASSERT(file_desc != nullptr);
    
    // create internal file_reply struct
    auto reply = std::make_shared<_internal::file_reply>(
        this->shared_from_this(),
        this->_conn,
        file_desc,
        cb,
        this->_log
    );
    
    // set file content-type
    auto mime_type = evmvc::mime::get_type(filepath.extension().c_str());
    if(evmvc::mime::compressible(mime_type)){
        EVMVC_DBG(this->_log, "file is compressible");
        
        sp_header hdr = _req->headers().get(
            evmvc::field::accept_encoding
        );
        
        if(hdr){
            auto encs = hdr->accept_encodings();
            int wsize = EVMVC_COMPRESSION_NOT_SUPPORTED;
            if(encs.size() == 0)
                wsize = EVMVC_ZLIB_GZIP_WSIZE;
            for(const auto& cenc : encs){
                if(cenc.type == encoding_type::gzip){
                    wsize = EVMVC_ZLIB_GZIP_WSIZE;
                    break;
                }
                if(cenc.type == encoding_type::deflate){
                    wsize = EVMVC_ZLIB_DEFLATE_WSIZE;
                    break;
                }
                if(cenc.type == encoding_type::star){
                    wsize = EVMVC_ZLIB_GZIP_WSIZE;
                    break;
                }
            }
            
            if(wsize != EVMVC_COMPRESSION_NOT_SUPPORTED){
                reply->zs = (z_stream*)malloc(sizeof(*reply->zs));
                memset(reply->zs, 0, sizeof(*reply->zs));
                
                int compression_level = Z_DEFAULT_COMPRESSION;
                if(deflateInit2(
                    reply->zs,
                    compression_level,
                    Z_DEFLATED,
                    wsize, //MOD_GZIP_ZLIB_WINDOWSIZE + 16,
                    EVMVC_ZLIB_MEM_LEVEL,// MOD_GZIP_ZLIB_CFACTOR,
                    EVMVC_ZLIB_STRATEGY// Z_DEFAULT_STRATEGY
                    ) != Z_OK
                ){
                    throw EVMVC_ERR("deflateInit2 failed!");
                }
                
                this->headers().set(
                    evmvc::field::content_encoding, 
                    wsize == EVMVC_ZLIB_GZIP_WSIZE ? "gzip" : "deflate"
                );
            }
        }
    }
    
    //TODO: get file encoding
    this->encoding(enc == "" ? "utf-8" : enc).type(
        filepath.extension().c_str()
    );
    this->_prepare_headers();
    _started = true;
    
    /* we do not have to start sending data from the file from here -
    * this function will write data to the client, thus when finished,
    * will call our `http__send_chunk_` callback.
    */
    evhtp_send_reply_chunk_start(_ev_req, EVHTP_RES_OK);
}





}//::evmvc
