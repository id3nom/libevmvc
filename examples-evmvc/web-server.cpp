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

struct exit_app_params
{
    event_base* ev_base;
    event* ev;
};

void exit_app(int, short, void* arg)
{
    //struct event* tev = (struct event*)arg;
    //struct event_base* ev_base =
    //    (struct event_base*)arg;// event_get_base(tev);
    struct exit_app_params* params = (struct exit_app_params*)arg;
    struct timeval tv = {1,0};
    event_base_loopexit(params->ev_base, &tv);
    event_free(params->ev);
    delete(params);
}

int main(int argc, char** argv)
{
    struct event_base* _ev_base = event_base_new();
    
    evmvc::app_options opts;
    evmvc::sp_app srv = std::make_shared<evmvc::app>(std::move(opts));
    
    srv->get("/test",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        res.status(evmvc::status::ok).send(
            req.query_param_as<std::string>("val", "testing 1, 2...")
        );
        nxt(nullptr);
    });
    
    srv->get("/download-file/:[filename]",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        res.status(evmvc::status::ok)
            .download("the file path",
            req.route_param_as<std::string>("filename", "test.txt"));
        nxt(nullptr);
    });
    
    srv->get("/echo/:val",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        res.status(evmvc::status::ok).send(
            req.route_param_as<std::string>("val")
        );
        nxt(nullptr);
    });
    
    srv->get("/send-json",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
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
        res.status(evmvc::status::ok).send(json_val);
        nxt(nullptr);
    });
    
    srv->get("/send-file",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        auto path = req.query_param_as<std::string>("path");
        
        std::clog << fmt::format("sending file: '{0}'\n", path);
        
        res.send_file(path, "", [path](evmvc::cb_error err){
            if(err)
                std::cerr << fmt::format(
                    "send-file for file '{0}', failed!\n{1}\n",
                    path, err.c_str()
                );
        });
    });
    
    srv->get("/cookies/set/:[name]/:[val]/:[path]",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        evmvc::http_cookies::options opts;
        opts.expires = 
                date::sys_days{date::year(2021)/01/01} +
                std::chrono::hours{23} + std::chrono::minutes{59} +
                std::chrono::seconds{59};
        opts.path = "/cookies";
        
        res.cookies().set("cookie-a", "abc", opts);
        
        opts = {};
        opts.path = req.route_param_as<std::string>("path", "/");
        res.cookies().set(
            req.route_param_as<std::string>("name", "cookie-b"),
            req.route_param_as<std::string>("val", "def"),
            opts
        );
        
        res.status(evmvc::status::ok).send(
            "route: /cookies/set/:[name]/:[val]/:[path]"
        );
    });
    
    srv->get("/cookies/get/:[name]",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        res.status(evmvc::status::ok).send(
            fmt::format("route: {}\ncookie-a: {}, {}: {}", 
                "/cookies/get/:[name]",
                res.cookies().get<std::string>("cookie-a"),
                req.route_param_as<std::string>("name", "cookie-b"),
                res.cookies().get<std::string>(
                    req.route_param_as<std::string>("name", "cookie-b"),
                    "do not exists!"
                )
            )
        );
    });
    
    srv->get("/cookies/clear/:[name]/:[path]",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        evmvc::http_cookies::options opts;
        opts.path = req.route_param_as<std::string>("path", "");
        
        res.cookies().clear(
            req.route_param_as<std::string>("name", "cookie-b"),
            opts
        );
        res.status(evmvc::status::ok).send(
            "route: /cookies/clear/:[name]/:[path]"
        );
    });
    
    srv->get("/error",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        res.error(
            EVMVC_ERR("testing error sending.")
        );
    });
    
    srv->get("/exit",
    [&_ev_base, &srv](
        const evmvc::request& req, evmvc::response& res, auto nxt){
        
        res.send_status(evmvc::status::ok);
        struct timeval tv = {1,0};
        struct exit_app_params* params = new exit_app_params();
        params->ev_base = _ev_base;
        params->ev = event_new(_ev_base, -1, 0, exit_app, params);
        
        event_add(params->ev, &tv);
        
        srv.reset();
    });
    
    srv->add_router(
        std::static_pointer_cast<evmvc::router>(
            std::make_shared<evmvc::file_router>(
                EVMVC_PROJECT_SOURCE_DIR "/examples-evmvc/html/",
                "/html"
            )
        )
    );
    
    srv->post("/forms/login",
    [&_ev_base, &srv](
        const evmvc::request& req, evmvc::response& res, auto nxt){
        
        //multipart/form-data; boundary=---------------------------334625884604427441415954120
        
        
        res.send_status(evmvc::status::internal_server_error);
    });
    
    srv->listen(_ev_base);
    
    event_base_loop(_ev_base, 0);
    
    event_base_free(_ev_base);
    
    return 0;
}