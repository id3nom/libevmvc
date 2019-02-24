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

#ifndef _libevmvc_ssl_utils_h
#define _libevmvc_ssl_utils_h

#include "stable_headers.h"
#include "utils.h"
#include "logging.h"

#include <set>

namespace evmvc {

long parse_ssl_options(const evmvc::json& jopts)
{
    if(jopts.empty())
        return SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
            SSL_MODE_RELEASE_BUFFERS | SSL_OP_NO_COMPRESSION;
    
    static std::map<std::string, unsigned int> _opts_map = {
        {"SSL_OP_LEGACY_SERVER_CONNECT", SSL_OP_LEGACY_SERVER_CONNECT},
        {"SSL_OP_TLSEXT_PADDING", SSL_OP_TLSEXT_PADDING},
        {"SSL_OP_SAFARI_ECDHE_ECDSA_BUG", SSL_OP_SAFARI_ECDHE_ECDSA_BUG},
        
        {"SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS",
            SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS},

        {"SSL_OP_NO_QUERY_MTU", SSL_OP_NO_QUERY_MTU},
        {"SSL_OP_COOKIE_EXCHANGE", SSL_OP_COOKIE_EXCHANGE},
        {"SSL_OP_NO_TICKET", SSL_OP_NO_TICKET},
        {"SSL_OP_CISCO_ANYCONNECT", SSL_OP_CISCO_ANYCONNECT},

        {"SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION",
            SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION},
        {"SSL_OP_NO_COMPRESSION", SSL_OP_NO_COMPRESSION},
        {"SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION",
            SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION},
        {"SSL_OP_NO_ENCRYPT_THEN_MAC", SSL_OP_NO_ENCRYPT_THEN_MAC},

        {"SSL_OP_CIPHER_SERVER_PREFERENCE", SSL_OP_CIPHER_SERVER_PREFERENCE},
        {"SSL_OP_TLS_ROLLBACK_BUG", SSL_OP_TLS_ROLLBACK_BUG},

        {"SSL_OP_NO_SSLV3", SSL_OP_NO_SSLv3},
        {"SSL_OP_NO_TLSV1", SSL_OP_NO_TLSv1},
        {"SSL_OP_NO_TLSV1_2", SSL_OP_NO_TLSv1_2},
        {"SSL_OP_NO_TLSV1_1", SSL_OP_NO_TLSv1_1},
        
        {"SSL_OP_NO_DTLSV1", SSL_OP_NO_DTLSv1},
        {"SSL_OP_NO_DTLSV1_2", SSL_OP_NO_DTLSv1_2},
        
        {"SSL_OP_NO_SSL_MASK", SSL_OP_NO_SSL_MASK},
        {"SSL_OP_NO_DTLS_MASK", SSL_OP_NO_DTLS_MASK},

        {"SSL_OP_NO_RENEGOTIATION", SSL_OP_NO_RENEGOTIATION},
        {"SSL_OP_CRYPTOPRO_TLSEXT_BUG", SSL_OP_CRYPTOPRO_TLSEXT_BUG},

        {"SSL_OP_ALL", SSL_OP_ALL}
    };
    
    long opts = 0;
    if(jopts.is_array()){
        for(auto& jopt : jopts){
            if(jopt.is_string()){
                auto it = _opts_map.find(
                    boost::to_upper_copy(jopt.get<std::string>())
                );
                if(it != _opts_map.end())
                    opts |= it->second;
            }else if(jopt.is_number())
                opts |= (long)jopt;
        }
    }else if(jopts.is_number())
        opts |= (long)jopts;
    else if(jopts.is_string()){
        std::string sopts = jopts;
        std::vector<std::string> vopts;
        std::set<char> delims = {',', '|'};
        boost::split(
            vopts, sopts, boost::is_any_of(delims)
        );
        
        for(auto sopt : vopts){
            auto it = _opts_map.find(boost::to_upper_copy(sopt));
            if(it != _opts_map.end())
                opts |= it->second;
        }
    }
    
    return opts;
}
    
    
}//::evmvc
#endif //_libevmvc_ssl_utils_h
