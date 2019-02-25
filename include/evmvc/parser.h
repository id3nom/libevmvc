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

#ifndef _libevmvc_http_parser_h
#define _libevmvc_http_parser_h

#include "stable_headers.h"
#include "utils.h"
#include "logging.h"

#include "headers.h"

#define EVMVC_EOL_SIZE 2
namespace evmvc {


class parser
{
public:
    parser(wp_connection conn, const sp_logger& log)
        : _conn(conn), _log(log)
    {
    }
    
    sp_connection get_connection() const { return _conn.lock();}
    sp_logger log() const { return _log;}
    
    virtual void reset() = 0;
    virtual size_t parse(const char* buf, size_t len, cb_error& ec) = 0;
    
protected:
    wp_connection _conn;
    sp_logger _log;
};

class http_parser
    : public parser
{
    enum class error
    {
        none = 0,
        too_big,
        inval_method,
        inval_reqline,
        inval_schema,
        inval_proto,
        inval_ver,
        inval_hdr,
        inval_chunk_sz,
        inval_chunk,
        inval_state,
        user,
        status,
        generic
    };
    
    enum class flag
    {
        none                    = 0,
        chunked                 = (1 << 0),
        connection_keep_alive   = (1 << 1),
        connection_close        = (1 << 2),
        trailing                = (1 << 3),
    };
    
    enum class state
    {
        parse_req_line = 0,
        parse_header_section,
        parse_body_section,
        end
    };
    
    
public:
    http_parser(wp_connection conn, const sp_logger& log)
        : parser(conn, log->add_child("parser"))
    {
    }
    
    struct bufferevent* bev() const;
    
    void reset()
    {
        EVMVC_DBG(_log, "parser reset");
        
        _flags = http_parser::flag::none;
        _status = http_parser::state::parse_req_line;
        
        _body_size = 0;
        _total_bytes_read = 0;
        _bytes_read = 0;
        
        _hdrs.reset();
        _hdrs_eol_count = 0;
    }
    
    size_t parse(const char* in_data, size_t in_len, cb_error& ec)
    {
        EVMVC_DBG(_log,
            "parsing, len: {}\n{}", in_len, std::string(in_data, in_len)
        );
        
        if(in_len == 0)
            return 0;
        
        if(_status == http_parser::state::end)
            reset();
        
        _bytes_read = 0;
        
        size_t sol_idx = 0;
        ssize_t eol_idx = find_eol(in_data, in_len, 0);
        if(eol_idx == -1)
            return 0;
        
        const char* line = in_data + sol_idx;
        size_t line_len = eol_idx - sol_idx;
        
        while(eol_idx > -1){
            switch(_status){
                case http_parser::state::parse_req_line:{
                    if(line_len == 0){
                        ec = EVMVC_ERR("Bad request line format");
                        return _bytes_read;
                    }
                    
                    ssize_t em_idx = find_ch(line, line_len, ' ', 0);
                    if(em_idx == -1){
                        ec = EVMVC_ERR("Bad request line format");
                        return _bytes_read;
                    }
                    ssize_t eu_idx = find_ch(line, line_len, ' ', em_idx+1);
                    if(eu_idx == -1){
                        ec = EVMVC_ERR("Bad request line format");
                        return _bytes_read;
                    }
                    
                    _method = data_substr(line, 0, em_idx);
                    _uri = data_substring(line, em_idx+1, eu_idx);
                    _http_ver = data_substring(line, eu_idx+1, line_len);
                    
                    _url = url(_uri);
                    
                    _bytes_read += eol_idx + EVMVC_EOL_SIZE;
                    _status = http_parser::state::parse_header_section;
                    _hdrs = std::make_shared<header_map>();
                    break;
                }
                case http_parser::state::parse_header_section:{
                    if(line_len == 0){
                        //TODO: validate required header section
                        auto it = _hdrs->find("host");
                        if(it == _hdrs->end() || it->second.size() == 0)
                            _log->success(
                                "REQ received,"
                                "host: '{}', method: '{}', uri: '{}'",
                                "Not Found",
                                _method,
                                _uri
                            );
                        else
                            _log->success(
                                "REQ received,"
                                "host: '{}', method: '{}', uri: '{}'",
                                it->second[0],
                                _method,
                                _uri
                            );
                        _status = http_parser::state::parse_body_section;
                        _bytes_read += EVMVC_EOL_SIZE;
                        send_test_response();
                        break;
                    }
                    
                    ssize_t sep = find_ch(line, line_len, ':', 0);
                    if(sep == -1){
                        ec = EVMVC_ERR("Bad header line format");
                        return _bytes_read;
                    }
                    
                    std::string hn(line, sep);
                    EVMVC_TRACE(_log,
                        "Inserting header, name: '{}', value: '{}'",
                        hn, data_substring(line, sep+1, line_len)
                    );
                    auto it = _hdrs->find(hn);
                    if(it != _hdrs->end())
                        it->second.emplace_back(
                            data_substring(line, sep+1, line_len)
                            //std::string(line + sep +1, line_len - sep)
                        );
                    else
                        _hdrs->emplace(std::make_pair(
                            std::move(hn),
                            std::vector<std::string>{
                                data_substring(line, sep+1, line_len)
                                //std::string(line + sep +1, line_len - sep)
                            }
                        ));
                    
                    _bytes_read += line_len + EVMVC_EOL_SIZE;
                    break;
                }
                case http_parser::state::parse_body_section:{
                    _bytes_read += line_len + EVMVC_EOL_SIZE;
                    break;
                }
                default:
                    break;
            }
            
            sol_idx = eol_idx + EVMVC_EOL_SIZE;
            eol_idx = find_eol(in_data, in_len, sol_idx);
            if(eol_idx == -1)
                break;
            
            line = in_data + sol_idx;
            line_len = eol_idx - sol_idx;
        }
        
        return _bytes_read;
    }
    
private:
    void send_test_response();
    
    http_parser::error _err = http_parser::error::none;
    http_parser::flag _flags = http_parser::flag::none;
    http_parser::state _status = http_parser::state::parse_req_line;
    
    size_t _body_size = 0;
    uint64_t _total_bytes_read = 0;
    uint64_t _bytes_read = 0;
    
    
    std::string _method;
    std::string _uri;
    evmvc::url _url;
    std::string _http_ver;
    
    std::string _scheme;
    
    std::shared_ptr<header_map> _hdrs;
    size_t _hdrs_eol_count = 0;
    
    size_t _buf_size = 0;
    char* _buf[8192];
    
};


}//::evmvc

#endif//_libevmvc_http_parser_h