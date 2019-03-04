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

#ifndef _libevmvc_child_server_h
#define _libevmvc_child_server_h

#include "stable_headers.h"
#include "utils.h"
#include "configuration.h"

namespace evmvc {
namespace _internal {
    int _ssl_new_cache_entry(SSL* ssl, SSL_SESSION* sess);
    SSL_SESSION* _ssl_get_cache_entry(
        SSL* ssl, const unsigned char* sid, int sid_len, int* copy
    );
    void _ssl_remove_cache_entry(SSL_CTX* ctx, SSL_SESSION* sess);
    
    int _ssl_sni_servername(SSL* s, int *al, void *arg);
    //int _ssl_client_hello(SSL* s, int *al, void *arg);
    
    
}//::_internal

class child_server
    : public std::enable_shared_from_this<child_server>
{
    static int nxt_id()
    {
        static int _id = -1;
        return ++_id;
    }
public:
    child_server(const server_options& config, const md::log::sp_logger& log)
        : _id(std::hash<std::string>{}(config.name)),
        _config(config),
        _log(log->add_child(
                "child server-" + config.name
        ))
    {
        EVMVC_DEF_TRACE("child_server {:p} created", (void*)this);
    }
    
    ~child_server()
    {
        EVMVC_DEF_TRACE("child_server {:p} released", (void*)this);
    }
    
    size_t id() const { return _id;}
    std::string name() const { return _config.name;}
    const std::vector<std::string>& aliases() const { return _config.aliases;}
    
    running_state status() const
    {
        return _status;
    }
    bool stopped() const { return _status == running_state::stopped;}
    bool starting() const { return _status == running_state::starting;}
    bool running() const { return _status == running_state::running;}
    bool stopping() const { return _status == running_state::stopping;}
    
    const server_options& config() const { return _config;}
    md::log::sp_logger log() const { return _log;}
    SSL_CTX* ssl_ctx() const { return _ssl_ctx;}
    
    const struct timeval& atimeo() const { return _config.atimeo;}
    struct timeval& atimeo() { return _config.atimeo;}
    const struct timeval& rtimeo() const { return _config.rtimeo;}
    struct timeval& rtimeo() { return _config.rtimeo;}
    const struct timeval& wtimeo() const { return _config.wtimeo;}
    struct timeval& wtimeo() { return _config.wtimeo;}
    
    void start()
    {
        if(!stopped())
            throw MD_ERR(
                "Server must be in stopped state to start listening again"
            );
        _status = running_state::starting;
        
        for(auto& l : _config.listeners)
            if(l.ssl){
                _init_ssl_ctx();
                break;
            }
        
        _status = running_state::running;
    }
    
    void stop()
    {
        if(stopped() || stopping())
            throw MD_ERR(
                "Child server must be in running state to be able to stop it."
            );
        this->_status = running_state::stopping;
        
        if(_ssl_ctx)
            SSL_CTX_free(_ssl_ctx);
        _ssl_ctx = nullptr;
        
        this->_status = running_state::stopped;
    }
    
    
private:
    
    void _init_ssl_ctx()
    {
        static int session_id_context = 1;
        
        long cache_mode;
        unsigned char c;
        
        if(!RAND_poll())
            _log->fatal(MD_ERR("RAND_poll failed"));
        
        if(!RAND_bytes(&c, 1))
            _log->fatal(MD_ERR("RAND_bytes failed"));
        
        _ssl_ctx = SSL_CTX_new(TLS_server_method());
        if(!_ssl_ctx)
            _log->fatal(MD_ERR("SSL_CTX_new out of memory!"));
        
        SSL_CTX_set_options(
            _ssl_ctx,
            _config.ssl.ssl_opts | 
            SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
            SSL_MODE_RELEASE_BUFFERS | SSL_OP_NO_COMPRESSION
        );
        SSL_CTX_set_timeout(_ssl_ctx, _config.ssl.ssl_ctx_timeout);
        
        //SSL_CTX_set_options(_ssl_ctx, _config.ssl.ssl_opts);
        
        // init ecdh
        if(_config.ssl.named_curve.size() > 0){
            EC_KEY* ecdh = nullptr;
            int nid = 0;
            
            nid = OBJ_sn2nid(_config.ssl.named_curve.c_str());
            if(nid == 0)
                _log->fatal(MD_ERR(
                    "ECDH init failed with unknown curve: " +
                    std::string(_config.ssl.named_curve)
                ));
            
            ecdh = EC_KEY_new_by_curve_name(nid);
            if(!ecdh)
                _log->fatal(MD_ERR(
                    "ECDH init failed for curve: " +
                    std::string(_config.ssl.named_curve)
                ));
            
            SSL_CTX_set_tmp_ecdh(_ssl_ctx, ecdh);
            EC_KEY_free(ecdh);
        }else{
            EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
            if(!ecdh)
                _log->fatal(MD_ERR(
                    "ECDH init failed for curve: NID_X9_62_prime256v1"
                ));
            
            SSL_CTX_set_tmp_ecdh(_ssl_ctx, ecdh);
            EC_KEY_free(ecdh);
        }
        
        // init dbparams
        if(_config.ssl.dhparams.size() > 0){
            FILE* fh;
            DH* dh;
            
            fh = fopen(_config.ssl.dhparams.c_str(), "r");
            if(!fh)
                _log->error(MD_ERR(
                    "DH init failed: unable to open file: {}",
                    _config.ssl.dhparams
                ));
            else{
                dh = PEM_read_DHparams(fh, nullptr, nullptr, nullptr);
                if(!dh)
                    _log->error(MD_ERR(
                        "DH init failed: unable to parse file: {}",
                        _config.ssl.dhparams
                    ));
                else{
                    SSL_CTX_set_tmp_dh(_ssl_ctx, dh);
                    DH_free(dh);
                }
                
                fclose(fh);
            }
        }
        
        // ciphers
        if(_config.ssl.ciphers.size() > 0){
            if(SSL_CTX_set_cipher_list(
                _ssl_ctx, _config.ssl.ciphers.c_str()) == 0
            )
                _log->fatal(MD_ERR("set_cipher_list"));
        }
        
        if(!_config.ssl.cafile.empty())
            SSL_CTX_load_verify_locations(
                _ssl_ctx, _config.ssl.cafile.c_str(), _config.ssl.capath.c_str()
            );
        
        X509_STORE_set_flags(
            SSL_CTX_get_cert_store(_ssl_ctx),
            _config.ssl.store_flags
        );
        SSL_CTX_set_verify(
            _ssl_ctx, (int)_config.ssl.verify_mode, _config.ssl.x509_verify_cb
        );
        if(_config.ssl.x509_chk_issued_cb)
            X509_STORE_set_check_issued(
                SSL_CTX_get_cert_store(_ssl_ctx),
                _config.ssl.x509_chk_issued_cb
            );
        if(_config.ssl.verify_depth)
            SSL_CTX_set_verify_depth(_ssl_ctx, _config.ssl.verify_depth);
        
        switch(_config.ssl.cache_type){
            case ssl_cache_type::disabled:
                cache_mode = SSL_SESS_CACHE_OFF;
                break;
            default:
                cache_mode = SSL_SESS_CACHE_SERVER;
                break;
        };
        
        // verify if cert_file exists
        struct stat fst;
        if(stat(_config.ssl.cert_file.c_str(), &fst))
            _log->fatal(MD_ERR(
                "bad cert_file, errno: " + std::to_string(errno)
            ));
        
        SSL_CTX_use_certificate_chain_file(
            _ssl_ctx, _config.ssl.cert_file.c_str()
        );
        
        const char* key = _config.ssl.cert_key_file.size() > 0 ? 
            _config.ssl.cert_key_file.c_str() : _config.ssl.cert_file.c_str();
        
        if(_config.ssl.decrypt_cb){
            EVP_PKEY* pkey = _config.ssl.decrypt_cb(key);
            if(!pkey)
                _log->fatal(MD_ERR("decrypt_cb(key)"));
            SSL_CTX_use_PrivateKey(_ssl_ctx, pkey);
            EVP_PKEY_free(pkey);
        }else{
            if(stat(_config.ssl.cert_key_file.c_str(), &fst))
                _log->fatal(MD_ERR(
                    "bad cert_key_file, errno: " + std::to_string(errno)
                ));
            
            SSL_CTX_use_PrivateKey_file(_ssl_ctx, key, SSL_FILETYPE_PEM);
        }
            
        SSL_CTX_set_session_id_context(
            _ssl_ctx,
            (const unsigned char*)&session_id_context,
            sizeof(session_id_context)
        );
        
        SSL_CTX_set_app_data(_ssl_ctx, this);
        SSL_CTX_set_session_cache_mode(_ssl_ctx, cache_mode);
        
        if(cache_mode != SSL_SESS_CACHE_OFF){
            SSL_CTX_sess_set_cache_size(
                _ssl_ctx,
                _config.ssl.cache_size ? _config.ssl.cache_size : 1024
            );
            
            if(_config.ssl.cache_type == ssl_cache_type::builtin ||
                _config.ssl.cache_type == ssl_cache_type::user
            ){
                SSL_CTX_sess_set_new_cb(
                    _ssl_ctx, _internal::_ssl_new_cache_entry
                );
                SSL_CTX_sess_set_get_cb(
                    _ssl_ctx, _internal::_ssl_get_cache_entry
                );
                SSL_CTX_sess_set_remove_cb(
                    _ssl_ctx, _internal::_ssl_remove_cache_entry
                );
                
                if(_config.ssl.cache_init_cb)
                    _config.ssl.cache_args =
                        _config.ssl.cache_init_cb(this->shared_from_this());
            }
        }
        
        //if(vhosts.size() > 0)
        SSL_CTX_set_tlsext_servername_callback(
            _ssl_ctx, _internal::_ssl_sni_servername
        );
        //SSL_CTX_set_client_hello_cb(_ssl_ctx, _ssl_client_hello, this);
    }
    
    evmvc::running_state _status = running_state::stopped;
    size_t _id;
    server_options _config;
    md::log::sp_logger _log;
    
    SSL_CTX* _ssl_ctx = nullptr;
    
    
};



}//::evmvc
#endif//_libevmvc_child_server_h
