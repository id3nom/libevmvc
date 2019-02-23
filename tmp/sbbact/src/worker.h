#ifndef BBACT_WORKER_H
#define BBACT_WORKER_H

#include "stable_headers.h"
#include "globals.h"
#include "sockets_util.h"

#include <event2/bufferevent_ssl.h>
#include <openssl/dh.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

/*
typedef SSL_SESSION             evhtp_ssl_sess_t;
typedef SSL                     evhtp_ssl_t;
typedef SSL_CTX                 evhtp_ssl_ctx_t;
typedef X509                    evhtp_x509_t;
typedef X509_STORE_CTX          evhtp_x509_store_ctx_t;
typedef const unsigned char     evhtp_ssl_data_t;

*/

#define PIPE_WRITE_FD 1
#define PIPE_READ_FD 0

class channel
{
public:
    int ptoc[2] = {0,0};
    int ctop[2] = {0,0};
    struct event* rcmd_ev = nullptr;
    // access control messages (ancillary data) socket
    int usock = -1;
    std::string usock_path = "";
    struct event* rcmsg_ev = nullptr;
    
};

class worker_t;
typedef std::shared_ptr<worker_t> worker;

typedef EVP_PKEY* (*bbact_ssl_decrypt_cb)(const char* privfile);
typedef void* (*bbact_ssl_scache_init)(worker_t*);

enum class ssl_cache_type
{
    disabled = 0,
    internal,
    user,
    builtin
};

class ssl_config_t
{
public:
    std::string pemfile;
    std::string privfile;
    std::string cafile;
    std::string capath;
    
    std::string ciphers;
    std::string named_curve;
    std::string dhparams;
    
    long ssl_opts = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1;
    long ssl_ctx_timeout = 0;
    
    int verify_peer = 0;
    int verify_depth = 0;
    
    SSL_verify_cb x509_verify_cb = nullptr;
    X509_STORE_CTX_check_issued_fn x509_chk_issued_cb = nullptr;
    bbact_ssl_decrypt_cb decrypt_cb = nullptr;
    
    unsigned long store_flags = 0;
    
    ssl_cache_type scache_type = ssl_cache_type::disabled;
    long scache_timeout = 0;
    long scache_size = 0;
    bbact_ssl_scache_init scache_init = nullptr;
    
    void* args = nullptr;
};


int ssl_servername(SSL* ssl, int* unused, void* arg)
{
    // const char         * sname;
    // evhtp_connection_t * connection;
    // evhtp_t            * evhtp;
    // evhtp_t            * evhtp_vhost;

    // if (evhtp_unlikely(ssl == NULL)) {
    //     return SSL_TLSEXT_ERR_NOACK;
    // }

    // if (!(sname = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name))) {
    //     return SSL_TLSEXT_ERR_NOACK;
    // }

    // if (!(connection = SSL_get_app_data(ssl))) {
    //     return SSL_TLSEXT_ERR_NOACK;
    // }

    // if (!(evhtp = connection->htp)) {
    //     return SSL_TLSEXT_ERR_NOACK;
    // }

    // if ((evhtp_vhost = htp__request_find_vhost_(evhtp, sname))) {
    //     SSL_CTX * ctx = SSL_get_SSL_CTX(ssl);

    //     connection->htp = evhtp_vhost;

    //     HTP_FLAG_ON(connection, EVHTP_CONN_FLAG_VHOST_VIA_SNI);

    //     SSL_set_SSL_CTX(ssl, evhtp_vhost->ssl_ctx);
    //     SSL_set_options(ssl, SSL_CTX_get_options(ctx));

    //     if ((SSL_get_verify_mode(ssl) == SSL_VERIFY_NONE) ||
    //         (SSL_num_renegotiations(ssl) == 0)) {
    //         SSL_set_verify(ssl, SSL_CTX_get_verify_mode(ctx),
    //             SSL_CTX_get_verify_callback(ctx));
    //     }

    //     return SSL_TLSEXT_ERR_OK;
    // }

    return SSL_TLSEXT_ERR_NOACK;
}     /* htp__ssl_servername_ */

int ssl_new_scache_ent(SSL* ssl, SSL_SESSION* sess);
SSL_SESSION* ssl_get_scache_ent(
    SSL* ssl, const unsigned char* sid, int sid_len, int* copy);
void ssl_remove_scache_ent(SSL_CTX* ctx, SSL_SESSION* sess);


class vhost_t;
typedef std::shared_ptr<vhost_t> vhost;

class worker_t
{
public:
    
    ~worker_t()
    {
        if(cfg)
            delete cfg;
        
        if(ssl_ctx)
            SSL_CTX_free(ssl_ctx);
        ssl_ctx = nullptr;
    }
    
    void init_ssl(ssl_config_t* cfg)
    {
        static int session_id_context = 1;
        this->cfg = cfg;
        
        long cache_mode;
        unsigned char c;
        
        if(RAND_poll() != 1)
            error_exit("RAND_poll");
        
        if(RAND_bytes(&c, 1) != 1)
            error_exit("RAND_bytes");
        
        ssl_ctx = SSL_CTX_new(TLS_server_method());
        if(!ssl_ctx)
            error_exit("SSL_CTX_new out of memory!");
        
        SSL_CTX_set_options(
            ssl_ctx,
            cfg->ssl_opts | 
            SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
            SSL_MODE_RELEASE_BUFFERS | SSL_OP_NO_COMPRESSION
        );
        SSL_CTX_set_timeout(ssl_ctx, cfg->ssl_ctx_timeout);
        
        //SSL_CTX_set_options(ssl_ctx, cfg->ssl_opts);
        
        // init ecdh
        if(cfg->named_curve.size() > 0){
            EC_KEY* ecdh = nullptr;
            int nid = 0;
            
            nid = OBJ_sn2nid(cfg->named_curve.c_str());
            if(nid == 0)
                error_exit(
                    "ECDH init failed with unknown curve: " +
                    std::string(cfg->named_curve)
                );
            
            ecdh = EC_KEY_new_by_curve_name(nid);
            if(!ecdh)
                error_exit(
                    "ECDH init failed for curve: " +
                    std::string(cfg->named_curve)
                );
            
            SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh);
            EC_KEY_free(ecdh);
        }else{
            EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
            if(!ecdh)
                error_exit(
                    "ECDH init failed for curve: NID_X9_62_prime256v1"
                );
            
            SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh);
            EC_KEY_free(ecdh);
        }
        
        // init dbparams
        if(cfg->dhparams.size() > 0){
            FILE* fh;
            DH* dh;
            
            fh = fopen(cfg->dhparams.c_str(), "r");
            if(!fh)
                std::cerr << "DH init failed: unable to open file: " <<
                    cfg->dhparams << std::endl;
            else{
                dh = PEM_read_DHparams(fh, nullptr, nullptr, nullptr);
                if(!dh)
                    std::cerr << "DH init failed: unable to parse file: " <<
                        cfg->dhparams << std::endl;
                else{
                    SSL_CTX_set_tmp_dh(ssl_ctx, dh);
                    DH_free(dh);
                }
                
                fclose(fh);
            }
        }
        
        // ciphers
        if(cfg->ciphers.size() > 0){
            if(SSL_CTX_set_cipher_list(ssl_ctx, cfg->ciphers.c_str()) == 0)
                error_exit("set_cipher_list");
        }
        
        if(!cfg->cafile.empty())
            SSL_CTX_load_verify_locations(
                ssl_ctx, cfg->cafile.c_str(), cfg->capath.c_str()
            );
        
        X509_STORE_set_flags(
            SSL_CTX_get_cert_store(ssl_ctx),
            cfg->store_flags
        );
        SSL_CTX_set_verify(ssl_ctx, cfg->verify_peer, cfg->x509_verify_cb);
        if(cfg->x509_chk_issued_cb)
            X509_STORE_set_check_issued(
                SSL_CTX_get_cert_store(ssl_ctx),
                cfg->x509_chk_issued_cb
            );
        if(cfg->verify_depth)
            SSL_CTX_set_verify_depth(ssl_ctx, cfg->verify_depth);
        
        switch(cfg->scache_type){
            case ssl_cache_type::disabled:
                cache_mode = SSL_SESS_CACHE_OFF;
                break;
            default:
                cache_mode = SSL_SESS_CACHE_SERVER;
                break;
        };
        
        // verify if pemfile exists
        struct stat fst;
        if(stat(cfg->pemfile.c_str(), &fst))
            error_exit("bad pemfile, errno: " + std::to_string(errno));
        
        SSL_CTX_use_certificate_chain_file(ssl_ctx, cfg->pemfile.c_str());
        
        const char* key = cfg->privfile.size() > 0 ? 
            cfg->privfile.c_str() : cfg->pemfile.c_str();
        
        if(cfg->decrypt_cb){
            EVP_PKEY* pkey = cfg->decrypt_cb(key);
            if(!pkey)
                error_exit("decrypt_cb(key)");
            SSL_CTX_use_PrivateKey(ssl_ctx, pkey);
            EVP_PKEY_free(pkey);
        }else{
            if(stat(cfg->privfile.c_str(), &fst))
                error_exit("bad privfile, errno: " + std::to_string(errno));

            SSL_CTX_use_PrivateKey_file(ssl_ctx, key, SSL_FILETYPE_PEM);
        }
            
        SSL_CTX_set_session_id_context(
            ssl_ctx,
            (const unsigned char*)&session_id_context,
            sizeof(session_id_context)
        );
        
        SSL_CTX_set_app_data(ssl_ctx, this);
        SSL_CTX_set_session_cache_mode(ssl_ctx, cache_mode);
        
        if(cache_mode != SSL_SESS_CACHE_OFF){
            SSL_CTX_sess_set_cache_size(
                ssl_ctx, cfg->scache_size ? cfg->scache_size : 1024
            );
            
            if(cfg->scache_type == ssl_cache_type::builtin ||
                cfg->scache_type == ssl_cache_type::user
            ){
                SSL_CTX_sess_set_new_cb(ssl_ctx, ssl_new_scache_ent);
                SSL_CTX_sess_set_get_cb(ssl_ctx, ssl_get_scache_ent);
                SSL_CTX_sess_set_remove_cb(ssl_ctx, ssl_remove_scache_ent);
                
                if(cfg->scache_init)
                    void* scache_args = cfg->scache_init(this);
            }
        }
        
        if(vhosts.size() > 0)
            SSL_CTX_set_tlsext_servername_callback(ssl_ctx, ssl_servername);
    }
    
    int wid = -1;
    int pid = -1;
    channel chan;
    ssl_config_t* cfg = nullptr;
    SSL_CTX* ssl_ctx = nullptr;
    
    int bev_flags = BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS;
    
    std::vector<vhost> vhosts;
    
    struct timeval recv_timeo = {3,0};
    struct timeval send_timeo = {3,0};
    
};

class vhost_t
{
public:
    std::string hostname;
    ::worker worker;
};

enum class connection_flags
{
    error = (1 << 1),
    owner = (1 << 2),
    vhost_via_sni = (1 << 3),
    paused = (1 << 4),
    connected = (1 << 5),
    waiting = (1 << 6),
    free_conn = (1 << 7),
    keepalive = (1 << 8)
};

void connection_readcb(struct bufferevent* bev, void* arg);
void connection_writecb(struct bufferevent* bev, void* arg);
void connection_eventcb(struct bufferevent* bev, short events, void* arg);
void connection_resume_cb(int fd, short events, void* args);

class request_t
{
public:
    std::string hostname;
    ::vhost vhost;
};
typedef std::shared_ptr<request_t> request;

class connection_t
{
public:
    connection_t(worker_t* w, int sock_fd)
        : worker(w), sock(sock_fd)
    {
        if(w->ssl_ctx){
            std::clog << "creating ssl bufferevent\n";
            ssl = SSL_new(w->ssl_ctx);
            bev = bufferevent_openssl_socket_new(
                ev_base(),
                sock_fd,
                ssl,
                BUFFEREVENT_SSL_ACCEPTING,
                w->bev_flags
            );
            
            SSL_set_app_data(ssl, this);
            
        }else{
            bev = bufferevent_socket_new(ev_base(), sock, w->bev_flags);
        }
        
        struct timeval* rtimeo = nullptr;
        if(recv_timeo.tv_sec || recv_timeo.tv_usec)
            rtimeo = &recv_timeo;
        else if(w->recv_timeo.tv_sec || w->recv_timeo.tv_usec)
            rtimeo = &w->recv_timeo;
        
        struct timeval* wtimeo = nullptr;
        if(send_timeo.tv_sec || send_timeo.tv_usec)
            wtimeo = &send_timeo;
        else if(w->send_timeo.tv_sec || w->send_timeo.tv_usec)
            wtimeo = &w->send_timeo;
        
        bufferevent_set_timeouts(bev, rtimeo, wtimeo);
        
        resume_ev = event_new(
            ev_base(), -1, EV_READ | EV_PERSIST,
            connection_resume_cb, this
        );
        event_add(resume_ev, nullptr);
        
        bufferevent_setcb(bev,
            connection_readcb,
            nullptr, //connection_writecb,
            connection_eventcb,
            this
        );
        bufferevent_enable(bev, EV_READ);
    }
    ~connection_t()
    {
        if(resume_ev)
            event_free(resume_ev);
        
        if(bev){
            if(ssl){
                SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
                SSL_shutdown(ssl);
            }
            bufferevent_free(bev);
            ssl = nullptr;
            bev = nullptr;
        }
    }
    
    int parse_hostname(const char* data, size_t len)
    {
        if((flags & (uint16_t)connection_flags::vhost_via_sni) && ssl){
            const char* host = SSL_get_servername(
                ssl, TLSEXT_NAMETYPE_host_name
            );
            // on hostname callback
            req->hostname = std::string(host);
            return 0;

        }else{
            for(auto vh : worker->vhosts)
                if(!strcasecmp(vh->hostname.c_str(), data)){
                    this->vhost = req->vhost = vh;
                    return 0;
                }
        }
        return -1;
    }
    
    ::worker_t* worker;
    ::vhost vhost;
    int sock = -1;
    
    uint16_t flags = 0;
    
    SSL* ssl = nullptr;
    
    struct event* resume_ev = nullptr;
    struct bufferevent* bev = nullptr;
    
    struct timeval recv_timeo = {0,0};
    struct timeval send_timeo = {0,0};
    
    request req;
};

std::vector<worker>& workers()
{
    static std::vector<worker> _workers;
    return _workers;
}


int ssl_new_scache_ent(SSL* ssl, SSL_SESSION* sess)
{
    // evhtp_connection_t * connection;
    // evhtp_ssl_cfg_t    * cfg;
    // evhtp_ssl_data_t   * sid;
    // unsigned int         slen;

    // connection = (evhtp_connection_t *)SSL_get_app_data(ssl);
    // if (connection->htp == NULL) {
    //     return 0;     /* We cannot get the ssl_cfg */
    // }

    // cfg = connection->htp->ssl_cfg;
    // sid = (evhtp_ssl_data_t *)SSL_SESSION_get_id(sess, &slen);

    // SSL_set_timeout(sess, cfg->scache_timeout);

    // if (cfg->scache_add) {
    //     return (cfg->scache_add)(connection, sid, slen, sess);
    // }

    return 0;
}

SSL_SESSION* ssl_get_scache_ent(
    SSL* ssl, const unsigned char* sid, int sid_len, int* copy)
{
    // evhtp_connection_t * connection;
    // evhtp_ssl_cfg_t    * cfg;
    SSL_SESSION* sess;

    // connection = (evhtp_connection_t * )SSL_get_app_data(ssl);

    // if (connection->htp == NULL) {
    //     return NULL;     /* We have no way of getting ssl_cfg */
    // }

    // cfg  = connection->htp->ssl_cfg;
    // sess = NULL;

    // if (cfg->scache_get) {
    //     sess = (cfg->scache_get)(connection, sid, sid_len);
    // }

    // *copy = 0;

    return sess;
}

void ssl_remove_scache_ent(SSL_CTX* ctx, SSL_SESSION* sess)
{
    // evhtp_t          * htp;
    // evhtp_ssl_cfg_t  * cfg;
    // evhtp_ssl_data_t * sid;
    // unsigned int       slen;

    // htp = (evhtp_t *)SSL_CTX_get_app_data(ctx);
    // cfg = htp->ssl_cfg;
    // sid = (evhtp_ssl_data_t *)SSL_SESSION_get_id(sess, &slen);

    // if (cfg->scache_del) {
    //     (cfg->scache_del)(htp, sid, slen);
    // }
}


void connection_readcb(struct bufferevent* bev, void* arg)
{
    connection_t* c = (connection_t*)arg;
    
    if(bev == nullptr)
        return;
    
    while(true){
        size_t avail =
            evbuffer_get_length(bufferevent_get_input(bev));
        if(avail == 0)
            break;
        
        void* buf = evbuffer_pullup(
            bufferevent_get_input(bev), avail
        );
        
        std::clog << "Worker: " << c->worker->wid
            << " recv:\n~~~\n"
            << std::string((const char*)buf, avail)
            << "\n~~~\n\n";
        
        evbuffer_drain(bufferevent_get_input(bev), avail);
    }
    
    std::string msg(
        "<html><body><h1>It works! from worker: " +
        std::to_string(c->worker->wid) +
        "</h1></body></html>"
    );
    
    std::string hdr(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " + std::to_string(msg.size()) + "\r\n"
        "\r\n"
    );
    hdr += msg;
    
    bufferevent_write(bev, hdr.c_str(), hdr.size());
    
    /*
    size_t avail = evbuffer_get_length(
        bufferevent_get_input(bev)
    );
    if(avail == 0)
        return;
    if(c->flags & (uint16_t)connection_flags::paused)
        return;
    void* buf = evbuffer_pullup(bufferevent_get_input(bev), avail);
    */
    
    // evhtp_connection_t * c = arg;
    // void               * buf;
    // size_t               nread;
    // size_t               avail;

    // if (evhtp_unlikely(bev == NULL)) {
    //     return;
    // }
    
    // avail = HTP_LEN_INPUT(bev);
    
    // if (evhtp_unlikely(avail == 0)) {
    //     return;
    // }
    
    // if (c->flags & EVHTP_CONN_FLAG_PAUSED) {
    //     log_debug("connection is paused, returning");
    //     return;
    // }
    
    // if (c->request) {
    //     c->cr_status = EVHTP_RES_OK;
    // }
    
    // buf = evbuffer_pullup(bufferevent_get_input(bev), avail);
    
    // evhtp_assert(buf != NULL);
    // evhtp_assert(c->parser != NULL);

    // nread = htparser_run(c->parser, &request_psets, (const char *)buf, avail);

    // log_debug("nread = %zu", nread);

    // if (!(c->flags & EVHTP_CONN_FLAG_OWNER)) {
    //     /*
    //      * someone has taken the ownership of this connection, we still need to
    //      * drain the input buffer that had been read up to this point.
    //      */

    //     log_debug("EVHTP_CONN_FLAG_OWNER set, removing contexts");

    //     evbuffer_drain(bufferevent_get_input(bev), nread);
    //     evhtp_safe_free(c, evhtp_connection_free);

    //     return;
    // }

    // if (c->request) {
    //     switch (c->cr_status) {
    //         case EVHTP_RES_DATA_TOO_LONG:
    //             htp__hook_connection_error_(c, -1);

    //             evhtp_safe_free(c, evhtp_connection_free);
    //             return;
    //         default:
    //             break;
    //     }
    // }

    // evbuffer_drain(bufferevent_get_input(bev), nread);

    // if (c->request && c->cr_status == EVHTP_RES_PAUSE) {
    //     log_debug("Pausing connection");

    //     evhtp_request_pause(c->request);
    // } else if (htparser_get_error(c->parser) != htparse_error_none) {
    //     log_debug("error %d, freeing connection",
    //         htparser_get_error(c->parser));

    //     evhtp_safe_free(c, evhtp_connection_free);
    // } else if (nread < avail) {
    //     /* we still have more data to read (piped request probably) */
    //     log_debug("Reading more data via resumption");

    //     evhtp_connection_resume(c);
    // }
}

void connection_writecb(struct bufferevent* bev, void* arg)
{
    // evhtp_connection_t * conn;
    // uint64_t             keepalive_max;
    // const char         * errstr;

    // evhtp_assert(bev != NULL);

    // log_debug("writecb");

    // if (evhtp_unlikely(arg == NULL)) {
    //     log_error("No data associated with the bufferevent %p", bev);

    //     evhtp_safe_free(bev, bufferevent_free);
    //     return;
    // }

    // errstr = NULL;
    // conn   = (evhtp_connection_t *)arg;

    // do {
    //     if (evhtp_unlikely(conn->request == NULL)) {
    //         errstr = "no request associated with connection";
    //         break;
    //     }

    //     if (evhtp_unlikely(conn->parser == NULL)) {
    //         errstr = "no parser registered with connection";
    //         break;
    //     }

    //     if (evhtp_likely(conn->type == evhtp_type_server)) {
    //         if (evhtp_unlikely(conn->htp == NULL)) {
    //             errstr = "no context associated with the server-connection";
    //             break;
    //         }

    //         keepalive_max = conn->htp->max_keepalive_requests;
    //     } else {
    //         keepalive_max = 0;
    //     }
    // } while (0);

    // if (evhtp_unlikely(errstr != NULL)) {
    //     log_error("shutting down connection: %s", errstr);

    //     evhtp_safe_free(conn, evhtp_connection_free);
    //     return;
    // }

    // /* connection is in a paused state, no further processing yet */
    // if (conn->flags & EVHTP_CONN_FLAG_PAUSED) {
    //     log_debug("is paused");
    //     return;
    // }

    // /* run user-hook for on_write callback before further analysis */
    // htp__hook_connection_write_(conn);

    // if (conn->flags & EVHTP_CONN_FLAG_WAITING) {
    //     log_debug("Disabling WAIT flag");

    //     HTP_FLAG_OFF(conn, EVHTP_CONN_FLAG_WAITING);

    //     if (HTP_IS_READING(bev) == false) {
    //         log_debug("enabling EV_READ");

    //         bufferevent_enable(bev, EV_READ);
    //     }

    //     if (HTP_LEN_INPUT(bev)) {
    //         log_debug("have input data, will travel");

    //         htp__connection_readcb_(bev, arg);
    //         return;
    //     }
    // }

    // /* if the connection is not finished, OR there is data ready to output
    //  * (can only happen if a user-defined connection_write hook added data
    //  * manually, since this is called only when all data has been flushed)
    //  * just return and wait.
    //  */
    // if (!(conn->cr_flags & EVHTP_REQ_FLAG_FINISHED) || HTP_LEN_OUTPUT(bev)) {
    //     log_debug("not finished");
    //     return;
    // }

    // /*
    //  * if there is a set maximum number of keepalive requests configured, check
    //  * to make sure we are not over it. If we have gone over the max we set the
    //  * keepalive bit to 0, thus closing the connection.
    //  */
    // if (keepalive_max > 0) {
    //     if (++conn->num_requests >= keepalive_max) {
    //         HTP_FLAG_OFF(conn->request, EVHTP_REQ_FLAG_KEEPALIVE);
    //     }
    // }

    // if (conn->cr_flags & EVHTP_REQ_FLAG_KEEPALIVE) {
    //     htp_type type;

    //     log_debug("keep-alive on");
    //     /* free up the current request, set it to NULL, making
    //      * way for the next request.
    //      */
    //     evhtp_safe_free(conn->request, htp__request_free_);

    //     /* since the request is keep-alive, assure that the connection
    //      * is aware of the same.
    //      */
    //     HTP_FLAG_ON(conn, EVHTP_CONN_FLAG_KEEPALIVE);

    //     conn->body_bytes_read = 0;

    //     if (conn->type == evhtp_type_server) {
    //         if (conn->htp->parent != NULL
    //             && !(conn->flags & EVHTP_CONN_FLAG_VHOST_VIA_SNI)) {
    //             /* this request was served by a virtual host evhtp_t structure
    //              * which was *NOT* found via SSL SNI lookup. In this case we want to
    //              * reset our connections evhtp_t structure back to the original so
    //              * that subsequent requests can have a different 'Host' header.
    //              */
    //             conn->htp = conn->htp->parent;
    //         }
    //     }

    //     switch (conn->type) {
    //         case evhtp_type_client:
    //             type = htp_type_response;
    //             break;
    //         case evhtp_type_server:
    //             type = htp_type_request;
    //             break;
    //         default:
    //             log_error("Unknown connection type");

    //             evhtp_safe_free(conn, evhtp_connection_free);
    //             return;
    //     }

    //     htparser_init(conn->parser, type);
    //     htparser_set_userdata(conn->parser, conn);

    //     return;
    // } else {
    //     log_debug("goodbye connection");
    //     evhtp_safe_free(conn, evhtp_connection_free);

    //     return;
    // }

    // return;
}

void connection_eventcb(struct bufferevent* bev, short events, void* arg)
{
    connection_t* c = (connection_t*)arg;
    
    if(events & BEV_EVENT_EOF ||
        events & BEV_EVENT_ERROR ||
        events & BEV_EVENT_TIMEOUT
    ){
        std::clog << "worker: " << c->worker->wid << ", freeing bufferevent, "
            << "events: " << events << std::endl;
        
        delete c;
    }
    
    // evhtp_connection_t * c = arg;

    // log_debug("%p %p eventcb %s%s%s%s", arg, (void *)bev,
    //     events & BEV_EVENT_CONNECTED ? "connected" : "",
    //     events & BEV_EVENT_ERROR     ? "error"     : "",
    //     events & BEV_EVENT_TIMEOUT   ? "timeout"   : "",
    //     events & BEV_EVENT_EOF       ? "eof"       : "");

    // if (c->hooks && c->hooks->on_event) {
    //     (c->hooks->on_event)(c, events, c->hooks->on_event_arg);
    // }

    // if ((events & BEV_EVENT_CONNECTED)) {
    //     log_debug("CONNECTED");

    //     if (evhtp_likely(c->type == evhtp_type_client)) {
    //         HTP_FLAG_ON(c, EVHTP_CONN_FLAG_CONNECTED);

    //         bufferevent_setcb(bev,
    //             htp__connection_readcb_,
    //             htp__connection_writecb_,
    //             htp__connection_eventcb_, c);
    //     }

    //     return;
    // }

    // #ifndef EVHTP_DISABLE_SSL
    // if (c->ssl && !(events & BEV_EVENT_EOF)) {
    //     #ifdef EVHTP_DEBUG
    //     unsigned long sslerr;

    //     while ((sslerr = bufferevent_get_openssl_error(bev))) {
    //         log_error("SSL ERROR %lu:%i:%s:%i:%s:%i:%s",
    //             sslerr,
    //             ERR_GET_REASON(sslerr),
    //             ERR_reason_error_string(sslerr),
    //             ERR_GET_LIB(sslerr),
    //             ERR_lib_error_string(sslerr),
    //             ERR_GET_FUNC(sslerr),
    //             ERR_func_error_string(sslerr));
    //     }
    //     #endif

    //     /* XXX need to do better error handling for SSL specific errors */
    //     HTP_FLAG_ON(c, EVHTP_CONN_FLAG_ERROR);

    //     if (c->request) {
    //         HTP_FLAG_ON(c->request, EVHTP_REQ_FLAG_ERROR);
    //     }
    // }

    // #endif

    // if (events == (BEV_EVENT_EOF | BEV_EVENT_READING)) {
    //     log_debug("EOF | READING");

    //     if (errno == EAGAIN) {
    //         /* libevent will sometimes recv again when it's not actually ready,
    //          * this results in a 0 return value, and errno will be set to EAGAIN
    //          * (try again). This does not mean there is a hard socket error, but
    //          * simply needs to be read again.
    //          *
    //          * but libevent will disable the read side of the bufferevent
    //          * anyway, so we must re-enable it.
    //          */
    //         log_debug("errno EAGAIN");

    //         if (HTP_IS_READING(bev) == false) {
    //             bufferevent_enable(bev, EV_READ);
    //         }

    //         errno = 0;

    //         return;
    //     }
    // }

    // /* set the error mask */
    // HTP_FLAG_ON(c, EVHTP_CONN_FLAG_ERROR);

    // /* unset connected flag */
    // HTP_FLAG_OFF(c, EVHTP_CONN_FLAG_CONNECTED);

    // htp__hook_connection_error_(c, events);

    // if (c->flags & EVHTP_CONN_FLAG_PAUSED) {
    //     /* we are currently paused, so we don't want to free just yet, let's
    //      * wait till the next loop.
    //      */
    //     HTP_FLAG_ON(c, EVHTP_CONN_FLAG_FREE_CONN);
    // } else {
    //     evhtp_safe_free(c, evhtp_connection_free);
    // }
}

void connection_resume_cb(int fd, short events, void* args)
{
    connection_t* c = (connection_t*)args;
    
    // evhtp_connection_t * c = arg;

    // log_debug("resumecb");

    // /* unset the pause flag */
    // HTP_FLAG_OFF(c, EVHTP_CONN_FLAG_PAUSED);

    // if (c->request) {
    //     log_debug("cr status = OK %d", c->cr_status);
    //     c->cr_status = EVHTP_RES_OK;
    // }

    // if (c->flags & EVHTP_CONN_FLAG_FREE_CONN) {
    //     log_debug("flags == FREE_CONN");
    //     evhtp_safe_free(c, evhtp_connection_free);

    //     return;
    // }

    // /* XXX this is a hack to show a potential fix for issues/86, the main indea
    //  * is that you call resume AFTER you have sent the reply (not BEFORE).
    //  *
    //  * When it has been decided this is a proper fix, the pause bit should be
    //  * changed to a state-type flag.
    //  */

    // if (HTP_LEN_OUTPUT(c->bev)) {
    //     log_debug("SET WAITING");

    //     HTP_FLAG_ON(c, EVHTP_CONN_FLAG_WAITING);

    //     if (HTP_IS_WRITING(c->bev) == false) {
    //         log_debug("ENABLING EV_WRITE");

    //         bufferevent_enable(c->bev, EV_WRITE);
    //     }
    // } else {
    //     log_debug("SET READING");

    //     if (HTP_IS_READING(c->bev) == false) {
    //         log_debug("ENABLING EV_READ");

    //         bufferevent_enable(c->bev, EV_READ | EV_WRITE);
    //     }

    //     if (HTP_LEN_INPUT(c->bev)) {
    //         log_debug("calling readcb directly");
    //         htp__connection_readcb_(c->bev, c);
    //     }
    // }
    
}













void worker_recv_sock(worker_t* w, int sock)
{
    socket_set_opts(sock);
    connection_t* con = new connection_t(w, sock);
    /*
    event* sev = event_new(
        ev_base(), sock, EV_READ | EV_PERSIST,
    [](int fd, short events, void* args)->void{
        worker_t* w = (worker_t*)args;
        
        if((events & EV_READ) != EV_READ)
            return;
        
        char buf[4096];
        while(true){
            ssize_t len = read(fd, buf, 4096);
            if(len == -1){
                int err = errno;
                if(err == EAGAIN || err == EWOULDBLOCK)
                    break;
                error_exit("worker sock read: " + std::to_string(err));
            }
            if(len == 0)
                return;
            if(len > 0){
                std::clog << "Worker: " <<  w->wid
                    << " recv:\n~~~\n"
                    << std::string((const char*)buf, avail)
                    << "\n~~~\n\n";
            }
        }
        
        std::string msg(
            "<html><body><h1>It works! from worker: " +
            std::to_string(w->wid) +
            "</h1></body></html>"
        );
        
        std::string hdr(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: " + std::to_string(msg.size()) + "\r\n"
            "\r\n"
        );
        hdr += msg;
        
        writen(fd, hdr.c_str(), hdr.size());
    }, w);
    event_add(sev, nullptr);
    */
    
    /*
    struct bufferevent* bev = bufferevent_socket_new(
        ev_base(), sock,
        BEV_OPT_CLOSE_ON_FREE
    );
    bufferevent_setcb(bev,
        // readcb
        [](struct bufferevent* bev, void* ctx)->void{
            if(bev == nullptr)
                return;
            
            worker_t* w = (worker_t*)ctx;
            
            while(true){
                size_t avail =
                    evbuffer_get_length(bufferevent_get_input(bev));
                if(avail == 0)
                    break;
                
                void* buf = evbuffer_pullup(
                    bufferevent_get_input(bev), avail
                );
                
                std::clog << "Worker: " <<  w->wid
                    << " recv:\n~~~\n"
                    << std::string((const char*)buf, avail)
                    << "\n~~~\n\n";
                
                evbuffer_drain(bufferevent_get_input(bev), avail);
            }
            
            std::string msg(
                "<html><body><h1>It works! from worker: " +
                std::to_string(w->wid) +
                "</h1></body></html>"
            );
            
            std::string hdr(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Content-Length: " + std::to_string(msg.size()) + "\r\n"
                "\r\n"
            );
            hdr += msg;
            
            bufferevent_write(bev, hdr.c_str(), hdr.size());
        },
        
        // writecb
        nullptr,
        // [](struct bufferevent* bev, void* ctx)->void{
        //     //bufferevent_free(bev);
            
        // },
        
        // eventcb
        [](struct bufferevent* bev, short what, void* ctx)->void{
            if(what & BEV_EVENT_EOF || what & BEV_EVENT_ERROR ||
                what & BEV_EVENT_TIMEOUT
            ){
                worker_t* w = (worker_t*)ctx;
                
                std::clog << "worker: " << w->wid << ", freeing bufferevent, "
                    << "events: " << what << std::endl;
                
                bufferevent_free(bev);
            }
        },
        
        w
    );
    bufferevent_enable(bev, EV_READ);
    */
}

int start_worker(worker w)
{
    // will send a SIGINT when the parent dies
    prctl(PR_SET_PDEATHSIG, SIGINT);
    int ppid = getppid();
    std::cout << "starting worker " << w->pid
        << ", parent pid: " << ppid << "\n";
    
    // init event_base
    ev_base();
    
    // on read command
    w->chan.rcmd_ev = event_new(
        ev_base(), w->chan.ptoc[PIPE_READ_FD],
        EV_READ | EV_PERSIST,
    [](int fd, short events, void* args)->void{
        worker_t* w = (worker_t*)args;
        
        char buf[4096];
        while(true){
            ssize_t len = read(fd, buf, 4096);
            if(len == -1){
                int err = errno;
                if(err == EAGAIN || err == EWOULDBLOCK)
                    break;
                
                std::cerr << "read failed with: " << err << "\n";
                raise(SIGINT);
                return;
            }
            if(len > 0){
                std::cout << "recv from listener: '" << 
                    std::string(buf, len)
                    << "'\n";
                std::string msg("tnks listener for the msg");
                writen(w->chan.ctop[PIPE_WRITE_FD], msg.c_str(), msg.size());
            }
            if(len == 0)
                break;
        }
    }, w.get());
    event_add(w->chan.rcmd_ev, nullptr);
    
    
    // on read cmsg
    w->chan.rcmsg_ev = event_new(
        ev_base(), w->chan.usock, EV_READ | EV_PERSIST,
    [](int fd, short events, void* args)->void{
        worker_t* w = (worker_t*)args;
        
        while(true){
            int data;
            struct msghdr msgh;
            struct iovec iov;
            
            union
            {
                char buf[CMSG_SPACE(sizeof(int))];
                struct cmsghdr align;
            } ctrl_msg;
            struct cmsghdr* cmsgp;
            
            msgh.msg_name = NULL;
            msgh.msg_namelen = 0;
            
            msgh.msg_iov = &iov;
            msgh.msg_iovlen = 1;
            iov.iov_base = &data;
            iov.iov_len = sizeof(int);
            
            msgh.msg_control = ctrl_msg.buf;
            msgh.msg_controllen = sizeof(ctrl_msg.buf);
            
            int nr = recvmsg(fd, &msgh, 0);
            if(nr == 0)
                return;
            if(nr == -1){
                int terrno = errno;
                if(terrno == EAGAIN || terrno == EWOULDBLOCK)
                    return;
                
                error_exit("recvmsg: " + std::to_string(terrno));
            }
            
            if (nr > 0)
                fprintf(stderr, "Received data = %d\n", data);
            
            cmsgp = CMSG_FIRSTHDR(&msgh);
            /* Check the validity of the 'cmsghdr' */
            if (cmsgp == NULL || cmsgp->cmsg_len != CMSG_LEN(sizeof(int)))
                error_exit("bad cmsg header / message length");
            if (cmsgp->cmsg_level != SOL_SOCKET)
                error_exit("cmsg_level != SOL_SOCKET");
            if (cmsgp->cmsg_type != SCM_RIGHTS)
                error_exit("cmsg_type != SCM_RIGHTS");
            
            int sock = *((int*)CMSG_DATA(cmsgp));
            worker_recv_sock(w, sock);
        }
        
    }, w.get());
    event_add(w->chan.rcmsg_ev, nullptr);


    
    event_base_loop(ev_base(), 0);
    // event_del(tev);
    // event_free(tev);
    event_del(w->chan.rcmd_ev);
    event_free(w->chan.rcmd_ev);
    event_base_free(ev_base());
    
    std::cout << "worker closing\n";
    
    return 0;
}

#endif//BBACT_WORKER_H