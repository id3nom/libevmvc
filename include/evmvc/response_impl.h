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

#define EVMVC_MAX_RES_STATUS_LINE_LEN 47
// max header field size is 8KiB
#define EVMVC_MAX_RES_HEADER_LINE_LEN 8192

namespace evmvc {

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


inline sp_connection response::connection() const { return _conn.lock();}
inline bool response::secure() const { return _conn.lock()->secure();}

inline evmvc::sp_app response::get_app() const
{
    return this->get_router()->get_app();
}
inline evmvc::sp_router response::get_router() const
{
    return this->get_route()->get_router();
}


inline void response::pause()
{
    if(_paused)
        return;
    _paused = true;
    this->log()->debug("Connection paused");
    if(auto c = _conn.lock())
        c->set_conn_flag(conn_flags::paused);
}

inline void response::resume()
{
    if(!_paused || _resuming){
        _log->warn(MD_ERR(
            "SHOULD NOT RESUME, is paused: {}, is resuming: {}",
            _paused ? "true" : "false",
            _resuming ? "true" : "false"
        ));
        return;
    }
    _resuming = true;
    this->log()->debug("Resuming connection");
    if(auto c = _conn.lock())
        c->resume();
    else
        _resume_cb = nullptr;
}




inline void response::error(
    evmvc::status err_status, const md::callback::cb_error& err)
{
    std::string remote_addr("unknown");
    std::string remote_port = "";
    if(auto c = this->_conn.lock()){
        remote_addr = c->remote_address();
        remote_port = remote_addr.find("unix:") == 0 ? 
            "" : ":" + std::to_string(c->remote_port());
    }
    //auto con_addr_in = (sockaddr_in*)this->_ev_req->conn->saddr;
    std::string log_val = fmt::format(
        "[{}{}] [{}] [{}] [Status {} {}, {}]\n{}",
        remote_addr, remote_port,

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

    if(a->options().stack_trace_enabled && err.has_stack()){
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
    if(a->options().stack_trace_enabled)
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

inline void response::_prepare_headers()
{
    EVMVC_TRACE(_log, "_prepare_headers");
    
    if(_started)
        throw std::runtime_error(
            "unable to prepare headers after response is started!"
        );
        
    auto c = _conn.lock();
    if(!c){
        _log->warn("Unable to lock the connection");
        return;
    }
    
    if(this->_status == -1){
        EVMVC_DBG(_log, "Status not set, setting status to 200 OK");
        this->_status = 200;
    }
    
    #if EVMVC_BUILD_DEBUG
    std::string dbg_hdrs;
    #endif //EVMVC_BUILD_DEBUG
    
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
    
    if(bufferevent_write(c->bev(), sl, smsgl+15))
        return this->_reply_end();
    
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
            hl[it.first.size()+2+itv.size()] = '\r';
            hl[it.first.size()+2+itv.size()+1] = '\n';
            
            #if EVMVC_BUILD_DEBUG
            dbg_hdrs += 
                std::string(hl, it.first.size()+2+itv.size()+2);
            #endif //EVMVC_BUILD_DEBUG
            
            if(bufferevent_write(c->bev(), hl, it.first.size()+2+itv.size()+2))
                return this->_reply_end();
        }
    }
    
    // write cookies headers
    for(auto& it : *_cookies->_out_hdrs.get()){
        for(auto& itv : it.second){
            memcpy(hl, it.first.c_str(), it.first.size());
            hl[it.first.size()] = ':';
            hl[it.first.size()+1] = ' ';
            memcpy(hl+ it.first.size()+2, itv.c_str(), itv.size());
            hl[it.first.size()+2+itv.size()] = '\r';
            hl[it.first.size()+2+itv.size()+1] = '\n';
            
            #if EVMVC_BUILD_DEBUG
            dbg_hdrs += 
                std::string(hl, it.first.size()+2+itv.size()+2);
            #endif //EVMVC_BUILD_DEBUG
            
            if(bufferevent_write(c->bev(), hl, it.first.size()+2+itv.size()+2))
                return this->_reply_end();
        }
    }
    
    #if EVMVC_BUILD_DEBUG
        _log->trace("Headers sent:\n{}", dbg_hdrs);
    #endif //EVMVC_BUILD_DEBUG
    
    if(bufferevent_write(c->bev(), "\r\n", 2))
        return this->_reply_end();
    if(bufferevent_flush(c->bev(), EV_WRITE, BEV_FLUSH))
        return this->_reply_end();
}

inline void response::_reply_raw(const char* data, size_t len)
{
    EVMVC_TRACE(_log, "_reply_raw");
    if(auto c = this->_conn.lock()){
        if(bufferevent_write(c->bev(), data, len))
            return this->_reply_end();
    }else{
        return this->_reply_end();
    }
}

inline void response::_reply_end()
{
    EVMVC_TRACE(_log, "_reply_end");

    _ended = true;
    auto c = this->_conn.lock();
    if(!c)
        return _log->error(MD_ERR("Connection closed!"));
        
    c->complete_response();
}


inline void response::send_file(
    const bfs::path& filepath,
    const md::string_view& enc, 
    md::callback::async_cb cb)
{
    auto c = this->_conn.lock();
    if(!c){
        if(cb)
            cb(MD_ERR("Connection closed!"));
        return;
    }
    
    if(filepath.empty()){
        _log->error(MD_ERR(
            "Filepath is empty!"
        ));
        
        this->send_status(evmvc::status::bad_request);
        return;
    }
    
    boost::system::error_code ec;
    if(!bfs::exists(filepath, ec)){
        if(ec){
            _log->error(MD_ERR(
                "Unable to verify file '{}' existence!\n{}",
                filepath.string(), ec.message()
            ));
            
            this->send_status(evmvc::status::bad_request);
            return;
        }
        this->send_status(evmvc::status::bad_request);
        return;
    }
    
    FILE* file_desc = fopen(filepath.c_str(), "r");
    if(!file_desc){
        _log->error(MD_ERR(
            "fopen failed, errno: {}", errno
        ));
        
        this->send_status(evmvc::status::bad_request);
        return;
    }
    
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
                    throw MD_ERR("deflateInit2 failed!");
                }
                
                this->headers().set(
                    evmvc::field::content_encoding, 
                    wsize == EVMVC_ZLIB_GZIP_WSIZE ? "gzip" : "deflate"
                );
            }
        }
    }
    
    //TODO: get file encoding
    if(this->_status == -1)
        this->status(200);
    this->encoding(enc == "" ? "utf-8" : enc).type(
        filepath.extension().c_str()
    );
    c->send_file(reply);
}





}//::evmvc
