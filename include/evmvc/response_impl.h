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
#include "file_reply.h"
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
        c->remote_address(),
        c->remote_address().find("unix:") == 0 ? 
            "" : ":" + std::to_string(c->remote_port()),

        to_string(_req->method()).data(),

        _req->uri().to_string(),
        
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

#define EVMVC_MAX_RES_STATUS_LINE_LEN 47
// max header field size is 8KiB
#define EVMVC_MAX_RES_HEADER_LINE_LEN 8192
static char* status_line_buf()
{
    static char sl[EVMVC_MAX_RES_STATUS_LINE_LEN]{
        "HTTP/v.v sss 0123456789012345678901234567890"
    };
    return sl;
}
static char* header_line_buf()
{
   static char hl[EVMVC_MAX_RES_HEADER_LINE_LEN]{0};
   return hl;
}

void response::_prepare_headers()
{
    if(_started)
        throw std::runtime_error(
            "unable to prepare headers after response is started!"
        );
        
    auto c = _conn.lock();
    if(!c){
        _log->warn("Unable to lock the connection");
        return;
    }
    
    char* sl = status_line_buf();
    switch(c->parser()->http_ver()){
        case http_version::http_10:
            sl[5] = '1';
            sl[7] = '0';
            break;
        case http_version::http_11:
        case http_version::http_2:
            sl[5] = '1';
            sl[7] = '1';
            break;
        default:
            break;
    }
    
    const char* stxt = to_status_text(this->_status);
    sl[9] = stxt[0];
    sl[10] = stxt[1];
    sl[11] = stxt[2];
    
    const char* smsg = to_status_string(this->_status);
    size_t smsgl = strlen(smsg);
    for(size_t i = 0; i < smsgl; ++i)
        sl[i + 13] = smsg[i];
    sl[smsgl + 13] = '\r';
    sl[smsgl + 14] = '\n';
    
    bufferevent_write(c->bev(), sl, smsgl+14);
    
    // lookfor keepalive header
    if(c->parser()->http_ver() == http_version::http_10){
        if(_req->headers().compare_value("connection", "keep-alive")){
            _headers->set(field::connection, "keep-alive");
            c->keep_alive(true);
        }else{
            c->keep_alive(false);
        }
        
    }else{
        if(_req->headers().compare_value("connection", "close")){
            _headers->set(field::connection, "close");
            c->keep_alive(false);
        }else{
            c->keep_alive(true);
        }
    }
    
    // write headers
    char* hl = header_line_buf();
    
    for(auto& it : *_headers->_hdrs.get()){
        for(auto& itv : it.second){
            memcpy(hl, it.first.c_str(), it.first.size());
            hl[it.first.size()] = ':';
            hl[it.first.size()+1] = ' ';
            memcpy(hl+ it.first.size()+2, itv.c_str(), itv.size());
            hl[it.first.size()+2+itv.size()+1] = '\r';
            hl[it.first.size()+2+itv.size()+2] = '\n';
            bufferevent_write(c->bev(), hl, it.first.size()+2+itv.size()+2);
        }
    }
    
    // write cookies headers
    for(auto& it : *_cookies->_out_hdrs.get()){
        for(auto& itv : it.second){
            memcpy(hl, it.first.c_str(), it.first.size());
            hl[it.first.size()] = ':';
            hl[it.first.size()+1] = ' ';
            memcpy(hl+ it.first.size()+2, itv.c_str(), itv.size());
            hl[it.first.size()+2+itv.size()+1] = '\r';
            hl[it.first.size()+2+itv.size()+2] = '\n';
            bufferevent_write(c->bev(), hl, it.first.size()+2+itv.size()+2);
        }
    }
    
    bufferevent_write(c->bev(), "\r\n", 2);
    bufferevent_flush(c->bev(), EV_WRITE, BEV_FLUSH);
}

void response::_reply_raw(const char* data, size_t len)
{
    if(auto c = this->_conn.lock())
        bufferevent_write(c->bev(), data, len);
}

void response::send_file(
    const bfs::path& filepath,
    const evmvc::string_view& enc, 
    async_cb cb)
{
    auto c = this->_conn.lock();
    if(!c){
        if(cb)
            cb(EVMVC_ERR("Connection must be closed!"));
        return;
    }
    
    FILE* file_desc = nullptr;
    //struct evmvc::_internal::file_reply* reply = nullptr;
    
    // open up the file
    file_desc = fopen(filepath.c_str(), "r");
    BOOST_ASSERT(file_desc != nullptr);
    
    // create internal file_reply struct
    auto reply = std::make_shared<file_reply>(
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
    c->send_file(reply);
}





}//::evmvc
