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

#ifndef _DEBUG
#define _DEBUG
#endif

#include "evmvc/evmvc.h"

#include <sys/time.h>
extern "C" {
#include <event2/event.h>
}

#include <sys/types.h>
#include <unistd.h>

#include "examples_views.h"

void _on_event_log(int severity, const char *msg)
{
    if(severity == _EVENT_LOG_DEBUG)
        md::log::default_logger()->debug(msg);
    else
        md::log::default_logger()->error(msg);
}

void _on_event_fatal_error(int err)
{
    md::log::default_logger()->fatal(
        "Exiting because libevent send a fatal error: '{}'",
        err
    );
}

evmvc::app srv(evmvc::app a = nullptr)
{
    static evmvc::wp_app _a;
    if(a)
        _a = a;
    return _a.lock();
}

void _sig_received(int sig)
{
    auto srv = ::srv();
    if(srv)
        srv->stop([](md::callback::cb_error err){
            event_base_loopbreak(evmvc::global::ev_base());
        });
    else
        event_base_loopbreak(evmvc::global::ev_base());
}

void register_app_cbs();

int main(int argc, char** argv)
{
    event_enable_debug_mode();
    event_set_log_callback(_on_event_log);
    event_set_fatal_callback(_on_event_fatal_error);
    
    struct event_base* _ev_base = event_base_new();
    
    evmvc::app_options opts;
    opts.stack_trace_enabled = true;
    opts.log_console_level = md::log::log_level::trace;
    opts.log_file_level = md::log::log_level::off;
    if(argc > 1)
        opts.worker_count = md::str_to_num<int>(argv[1]);
    if(argc > 2)
        opts.log_console_level = 
            (md::log::log_level)md::str_to_num<int>(argv[2]);
    
    auto l_opts = evmvc::listen_options();
    l_opts.address = "0.0.0.0";
    l_opts.port = 8080;
    l_opts.ssl = false;
    
    auto sl_opts = evmvc::listen_options();
    sl_opts.address = "0.0.0.0";
    sl_opts.port = 4343;
    sl_opts.ssl = true;
    
    auto srv_opts = evmvc::server_options("localhost");
    srv_opts.listeners.emplace_back(l_opts);
    srv_opts.listeners.emplace_back(sl_opts);
    
    // to generate certificates:
    // openssl req -x509 -nodes -newkey rsa:4096 -keyout key.pem
    //     -out cert.pem -days 365
    srv_opts.ssl.cert_file = "./cert.pem";
    srv_opts.ssl.cert_key_file = "./key.pem";
    
    opts.servers.emplace_back(srv_opts);
    
    evmvc::app srv = std::make_shared<evmvc::app_t>(
        _ev_base,
        std::move(opts)
    );
    ::srv(srv);
    
    // show app_t pid
    pid_t pid = getpid();
    srv->log()->info(
        "Starting evmvc_web_server, pid: {}",
        pid
    );
    srv->log()->info(evmvc::version());
    
    register_app_cbs();
    
    // will return 1 for child process.
    // will return -1 on error.
    // will return 0 for main process.
    if(auto res = srv->start(argc, argv)){
        if(res == 1)
            return 0;
        else
            return -1;
    }
    
    //signal(SIGINT, _sig_received);
    struct sigaction sa;
    sa.sa_handler = _sig_received;
    sigaction(SIGINT, &sa, nullptr);
    
    event_base_loop(_ev_base, 0);
    
    srv.reset();
    event_base_free(_ev_base);
    
    return 0;
}


void register_app_cbs()
{
    auto srv = ::srv();
    
    auto md_parse = [](evmvc::response /*res*/, std::string& str){
        str = evmvc::html_escape(str);
        
        str.insert(0, "<div data-markup=\"md\">");
        str.append("</div>");
    };
    // auto tex_parse = [](std::string& str){
    //     str = evmvc::html_escape(str);
        
    //     str.insert(0, "<div data-markup=\"tex\">");
    //     str.append("</div>");
    // };
    auto tex_parse = [](evmvc::response /*res*/, std::string& str){
        str = evmvc::html_escape(str);
        
        str.insert(0, "<div><div data-markup=\"tex\">");
        str.append(
            "</div>"
            "<iframe data-markup=\"tex\" "
            "src=\"about:blank\" frameborder=\"0\" scrolling=\"no\" "
            "onload=\"resizeIframe(this)\"></iframe></div>"
        );
    };
    
    // parse markdown markup
    evmvc::view_engine::register_language_parser("md", md_parse);
    evmvc::view_engine::register_language_parser("markdown", md_parse);
    // parse latex markup
    evmvc::view_engine::register_language_parser("tex", tex_parse);
    evmvc::view_engine::register_language_parser("latex", tex_parse);
    
    // register fanjet views
    examples::register_engine();
    
    
    srv->get("/views/missing",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->render("examples::home/missing", nxt);
    });
    srv->get("/views/fail",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->render("examples::home/fail", nxt);
    });
    srv->get("/views/index",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->set_data("msg", "bonjour");
        res->render("examples::home/index", nxt);
    });
    
    
    srv->get("/exit",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->send_status(evmvc::status::ok);
        
        evmvc::set_timeout(
            [](auto ev){
                ::srv()->stop([](md::callback::cb_error /*err*/){
                    //event_base_loopbreak(evmvc::global::ev_base());
                });
            },
            1000
        );
    });
    
    
    srv->get("/test",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->status(evmvc::status::ok).send(
            req->query("val", "testing 1, 2...")
        );
        nxt(nullptr);
    });
    
    srv->get("/download-file/:[filename]",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->download(
            "cert.pem",
            req->params().get<std::string>("filename", "test.txt")
        );
        nxt(nullptr);
    });
    
    srv->get("/echo/:val",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->status(evmvc::status::ok).send(
            //(md::string_view)req->params()<md::string_view>("val")
            req->params("val")
        );
        nxt(nullptr);
    });
    
    srv->get("/send-json",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        evmvc::json json_val = evmvc::json::parse(
            "{\"menu\": {"
            "\"id\": \"file\","
            "\"value\": \"File\","
            "\"popup\": {"
            "    \"menuitem\": ["
            "    {\"value\": \"New\", \"onclick\": \"CreateNewDoc()\"},"
            "    {\"value\": \"Open\", \"onclick\": \"OpenDoc()\"},"
            "    {\"value\": \"Close\", \"onclick\": \"CloseDoc()\"}"
            "    ]"
            "}"
            "}}"
        );
        res->status(evmvc::status::ok).send(json_val);
        nxt(nullptr);
    });
    
    srv->get("/send-file",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        std::string path = req->query("path");
        
        res->log()->info("sending file: '{0}'\n", path);
        
        res->send_file(path, "", [path, res](md::callback::cb_error err){
            if(err)
                res->log()->error(
                    "send-file for file '{0}', failed!\n{1}\n",
                    path, err.c_str()
                );
        });
    });
    
    srv->get("/cookies/set/:[name]/:[val]/:[path]",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        evmvc::http_cookies_t::options opts;
        opts.expires = 
                date::sys_days{date::year(2021)/01/01} +
                std::chrono::hours{23} + std::chrono::minutes{59} +
                std::chrono::seconds{59};
        opts.path = "/cookies";
        
        res->cookies().set("cookie-a", "abc", opts);
        
        opts = {};
        opts.path = req->params("path", "/");
        res->cookies().set(
            req->params("name", "cookie-b"),
            req->params("val", "def"),
            opts
        );
        
        res->status(evmvc::status::ok).send(
            "route: /cookies/set/:[name]/:[val]/:[path]"
        );
    });
    
    srv->get("/cookies/get/:[name]",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->status(evmvc::status::ok).send(
            fmt::format("route: {}\ncookie-a: {}, {}: {}", 
                "/cookies/get/:[name]",
                // res->cookies().get<std::string>("cookie-a"),
                req->cookies<std::string>("cookie-a"),
                req->params("name", "cookie-b"),
                // res->cookies().get<std::string>(
                //     req->params("name", "cookie-b"),
                //     "do not exists!"
                // )
                req->cookies(
                    req->params("name", "cookie-b"),
                    "do not exists!"
                )
            )
        );
    });
    
    srv->get("/cookies/clear/:[name]/:[path]",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        evmvc::http_cookies_t::options opts;
        opts.path = req->params("path", "");
        
        res->cookies().clear(
            req->params("name", "cookie-b"),
            opts
        );
        res->status(evmvc::status::ok).send(
            "route: /cookies/clear/:[name]/:[path]"
        );
    });
    
    srv->get("/error",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->error(
            MD_ERR("testing error sending.")
        );
    });
    
    auto frtr = std::make_shared<evmvc::file_router>(
        srv,
        EVMVC_PROJECT_SOURCE_DIR "/examples-evmvc/html/",
        "/html"
    );
    auto pol = evmvc::policies::new_filter_policy();
    pol->add_rule(evmvc::policies::new_user_filter(
        evmvc::policies::filter_type::access,
        [](evmvc::policies::filter_rule_ctx ctx,
            evmvc::policies::validation_cb cb)
        {
            // int to = rand()%(25-5 + 1) + 5;
            // ctx->req->log()->info("waiting {}ms...", to);
            //
            // evmvc::set_timeout([ctx, cb](auto ew){
            //     cb(nullptr);
            // }, to);
            evmvc::set_timeout([ctx, cb](auto ew){
                cb(nullptr);
            }, 0);
        })
    );
    frtr->register_policy(pol);
    
    srv->register_router(
        std::static_pointer_cast<evmvc::router_t>(frtr)
    );
    
    
    srv->post("/forms/login",
    [](const evmvc::request req, evmvc::response res, auto nxt){
        res->redirect("/html/login-results.html");
    });
    
    srv->get("/set_timeout/:[timeout(\\d+)]",
    [&srv](
        const evmvc::request req, evmvc::response res, auto nxt
    ){
        //res->pause();
        evmvc::set_timeout(
            [req, res](auto ew){
                //res->resume([res](auto err){
                    res->status(evmvc::status::ok).send(
                        "route: /set_timeout/:[timeout(\\d+)]"
                    );
                //});
            },
            req->params("timeout", 40000)
        );
    });
    
    srv->get("/set_interval/:[name]/:[interval(\\d+)]",
    [](
        const evmvc::request req, evmvc::response res, auto nxt
    ){
        res->status(evmvc::status::ok).send(
            "route: /set_interval/:[name]/:[interval(\\d+)]"
        );
        std::string name = req->params("name", "abc");
        evmvc::set_interval(
            name,
            [name](auto ew){
                md::log::info("'{}' interval reached!", name);
            },
            req->params("interval", 3000)
        );
    });
    srv->get("/clear_interval/:[name]",
    [](
        const evmvc::request req, evmvc::response res, auto nxt
    ){
        evmvc::clear_interval(
            req->params("name", "abc")
        );
        
        res->status(evmvc::status::ok).send(
            "route: /clear_interval/:[name]"
        );
    });

}
