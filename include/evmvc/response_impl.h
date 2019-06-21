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

#include "view_engine.h"

#include <boost/date_time/posix_time/posix_time.hpp>


#define EVMVC_MAX_RES_STATUS_LINE_LEN 47
// max header field size is 8KiB
#define EVMVC_MAX_RES_HEADER_LINE_LEN 8192

namespace evmvc {

inline response_t::response_t(
    uint64_t id,
    request req,
    wp_connection conn,
    md::log::logger log,
    const route& rt,
    url uri,
    const http_cookies& http_cookies_t)
    : _id(id),
    _req(req),
    _conn(conn),
    _log(log->add_child(
        "res-" + md::num_to_str(id, false) + uri.path()
    )),
    _rt(rt),
    _headers(std::make_shared<response_headers>()),
    _cookies(http_cookies_t),
    _started(false), _ended(false),
    _status(-1), _type(""), _enc(""),
    _paused(false),
    _resuming(false),
    _res_data(std::make_shared<evmvc::response_data_map_t>()),
    _err(nullptr), _err_status(evmvc::status::ok)
{
    EVMVC_DEF_TRACE("response_t {} {:p} created", _id, (void*)this);
    
    evmvc::app sa = this->get_app();
    for(auto mit : *sa->app_data()){
        _res_data->emplace(mit);
    }
}


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


inline sp_connection response_t::connection() const { return _conn.lock();}
inline bool response_t::secure() const { return _conn.lock()->secure();}

inline evmvc::app response_t::get_app() const
{
    return this->get_router()->get_app();
}
inline evmvc::router response_t::get_router() const
{
    return this->get_route()->get_router();
}


inline void response_t::pause()
{
    if(_paused)
        return;
    _paused = true;
    this->log()->debug("Connection paused");
    if(auto c = _conn.lock())
        c->set_conn_flag(conn_flags::paused);
}

inline void response_t::resume()
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


inline void response_t::set_error(
    const md::callback::cb_error& err,
    evmvc::status status_code)
{
    if(_err)
        this->_log->warn(
            "Overriding current error: '{}' with error: '{}'",
            _err, err
        );
    if(!err)
        return clear_error();
    
    _err = err;
    _err_status = status_code;
    
    evmvc::app a = this->get_route()->get_router()->get_app();
    set_data("_err_status", (uint16_t)_err_status);
    set_data("_err_status_desc", evmvc::statuses::status(_err_status));
    set_data("_err_message", std::string(_err.c_str()));
    
    if(a->options().stack_trace_enabled && _err.has_stack()){
        set_data("_err_has_stack",true);
        set_data(
            "_err_stack",
            fmt::format(
                "\n\n{}:{}\n{}\n\n{}",
                evmvc::html_escape(_err.file()),
                _err.line(),
                evmvc::html_escape(_err.func()),
                evmvc::html_escape(_err.stack())
            )
        );
    }else{
        set_data("_err_has_stack",false);
    }
}


inline void response_t::error(
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
    
    evmvc::app a = this->get_route()->get_router()->get_app();

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
    try{
        if((int16_t)err_status < 400)
            this->log()->info(log_val);
        else if((int16_t)err_status < 500)
            this->log()->warn(log_val);
        else
            this->log()->error(log_val);
    }catch(std::exception err){
        this->log()->error(MD_ERR(err.what()));
    }
    
    this->status(err_status).html(err_msg);
}

inline void response_t::_prepare_headers()
{
    EVMVC_TRACE(_log, "_prepare_headers");
    
    if(_started)
        throw std::runtime_error(
            "unable to prepare headers after response_t is started!"
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
            if(it.first.size() + itv.size() + 5 > EVMVC_MAX_RES_HEADER_LINE_LEN)
                throw MD_ERR(
                    "Header line is larger than the allowed maximum!\n"
                    "Max: '{}', Current: '{}'\nHeader line value: '{}'",
                    EVMVC_MAX_RES_HEADER_LINE_LEN,
                    it.first.size() + itv.size() + 5,
                    itv.c_str()
                );
            
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
            if(it.first.size() + itv.size() + 5 > EVMVC_MAX_RES_HEADER_LINE_LEN)
                throw MD_ERR(
                    "Header line is larger than the allowed maximum!\n"
                    "Max: '{}', Current: '{}'\nHeader line value: '{}'",
                    EVMVC_MAX_RES_HEADER_LINE_LEN,
                    it.first.size() + itv.size() + 5,
                    itv.c_str()
                );
            
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

inline void response_t::_reply_raw(const char* data, size_t len)
{
    EVMVC_TRACE(_log, "_reply_raw");
    if(auto c = this->_conn.lock()){
        if(bufferevent_write(c->bev(), data, len))
            return this->_reply_end();
    }else{
        return this->_reply_end();
    }
}

inline void response_t::_reply_end()
{
    EVMVC_TRACE(_log, "_reply_end");

    _ended = true;
    auto c = this->_conn.lock();
    if(!c)
        return _log->error(MD_ERR("Connection closed!"));
        
    c->complete_response();
}


inline void response_t::send_file(
    const bfs::path& filepath,
    const md::string_view& enc, 
    md::callback::async_cb cb)
{
    static std::locale out_loc = std::locale(
        std::locale::classic(),
        new boost::posix_time::time_facet(
            "%a, %d %b %Y %H:%M:%S GMT"
        )
    );
    static std::locale in_loc = std::locale(
        std::locale::classic(),
        new boost::posix_time::time_input_facet(
            "%a, %d %b %Y %H:%M:%S GMT"
        )
    );
    
    // static boost::posix_time::time_facet* tout_facet =
    //     new boost::posix_time::time_facet(
    //         "%a, %d %b %Y %H:%M:%S GMT"
    //     );
    // static boost::posix_time::time_input_facet* tin_facet =
    //     new boost::posix_time::time_input_facet(
    //         "%a, %d %b %Y %H:%M:%S GMT"
    //     );
    
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
    
    boost::posix_time::ptime fmtime;
    std::string fetag;
    //size_t fsize;
    {
        struct stat fstat;
        stat(filepath.c_str(), &fstat);
        fmtime = boost::posix_time::from_time_t(fstat.st_mtime);
        evmvc::get_etag(fstat, fetag);
        //fsize = fstat.st_size;
    }
    
    if(_req->headers().exists(evmvc::field::if_none_match)){
        std::string retag = _req->headers().get(
            evmvc::field::if_none_match
            )->value();
        md::trim(retag);
        
        if(_req->headers().exists(evmvc::field::if_modified_since)){
            boost::posix_time::ptime rmtime;
            
            std::string srmtime = _req->headers().get(
                evmvc::field::if_modified_since
            )->value();
            
            std::stringstream ss;
            ss.write(srmtime.c_str(), srmtime.size());
            //ss.imbue(std::locale(std::locale::classic(), tin_facet));
            ss.imbue(in_loc);
            ss >> rmtime;
            
            if(
                retag == fetag &&
                !rmtime.is_not_a_date_time() &&
                rmtime == fmtime
            ){
                return this->send_status(evmvc::status::not_modified);
            }
        }
    }
    
    std::stringstream ss;
    //ss.imbue(std::locale(ss.getloc(), tout_facet));
    ss.imbue(out_loc);
    ss << fmtime;
    
    this->_headers->set(evmvc::field::last_modified, ss.str());
    this->_headers->set(evmvc::field::etag, fetag);
    this->_headers->set(evmvc::field::cache_control, "max-age=2592000");
    
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

inline void response_t::render(
    const std::string& view_path, md::callback::async_cb cb)
{
    auto self = this->shared_from_this();
    
    view_engine::render(
    this->shared_from_this(), view_path,
    [self, cb](md::callback::cb_error err, std::string data){
        if(err)
            return cb(err);
        self->html(data);
        cb(nullptr);
    });
}

template<>
inline std::string response_t::get_data<std::string>(
    md::string_view name, const std::string& def_val)
{
    if(name == "_evmvc_version")
        return evmvc::html_version();
    
    auto it = _res_data->find(name.to_string());
    if(it == _res_data->end())
        return def_val;
    return it->second->to_string();
}

template<>
inline std::string response_t::get_data<std::string>(md::string_view name)
{
    auto it = _res_data->find(name.to_string());
    if(it == _res_data->end())
        throw MD_ERR("Data '{}' not found!", name);
    return it->second->to_string();
}



}//::evmvc
