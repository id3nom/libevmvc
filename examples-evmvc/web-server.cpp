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

#include "evmvc/evmvc.h"

int main(int argc, char** argv)
{
    struct event_base* _ev_base = event_base_new();
    
    evmvc::sp_app srv = std::make_shared<evmvc::app>("./");
    
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
        
        res.send_file(path, [path](evmvc::cb_error err){
            if(err)
                std::cerr << fmt::format(
                    "send-file for file '{0}', failed!\n{1}\n",
                    path, err.c_str()
                );
        });
    });

    srv->get("/cookies/set",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        evmvc::http_cookies::options opts;
        opts.expires = 
                date::sys_days{date::year(2021)/01/01} +
                std::chrono::hours{23} + std::chrono::minutes{59} +
                std::chrono::seconds{59};
        opts.path = "/cookies";
        
        res.cookies().set("cookie-a", "abc", opts);
        res.cookies().set("cookie-b", "def");
        
        res.status(evmvc::status::ok).end();
    });
    
    srv->get("/cookies/get",
    [](const evmvc::request& req, evmvc::response& res, auto nxt){
        res.status(evmvc::status::ok).send(
            fmt::format("cookie-a: {0}, cookie-b: {1}", 
                res.cookies().get<std::string>("cookie-a"),
                res.cookies().get<std::string>("cookie-b")
            )
        );
    });

    
    srv->listen(_ev_base);
    
    event_base_loop(_ev_base, 0);
    return 0;
}