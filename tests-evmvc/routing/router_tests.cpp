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

#include <gmock/gmock.h>
#include "evmvc/evmvc.h"

namespace evmvc { namespace tests {


class router_test: public testing::Test
{
public:
};


TEST_F(router_test, routes)
{
    try{
        evmvc::app_options opts;
        opts.use_default_logger = false;
        opts.log_console_level = 
            opts.log_file_level = evmvc::log_level::off;
        
        evmvc::sp_app srv = std::make_shared<evmvc::app>(
            nullptr,
            std::move(opts)
        );

        evmvc::sp_router r = 
            std::make_shared<evmvc::router>(srv);
        
        std::string rt_val;
        // // # simple route that will match url "/abc/123" and "/abc/123/"
        // // /abc-a/123
        // r->get("/abc-a/123",
        // [&rt_val](
        //      const evmvc::request& /*req*/, evmvc::response& /*res*/,
        //      async_cb cb
        // ){
        //     rt_val = "abc-a";
        // });
        
        // // # simple route that will match url's "/abc-b/123", "/abc-b/123/def",
        // // # "/abc-b/123/456/" but not "/abc-b/123/def/other_path"
        // // /abc-b/123/*
        // r->get("/abc-b/123/*",
        // [&rt_val](
        //      const evmvc::request& /*req*/, evmvc::response& /*res*/
        //      async_cb cb){
        //     rt_val = "abc-b";
        // });
        
        // # simple route that will match url's "/abc-c/123", "/abc-c/123/def",
        // # "/abc-c/123/456/" and any sub path "/abc-c/123/def/sub/path/..."
        // /abc-c/123/**
        r->get("/abc-c/123/**",
        [&rt_val](const evmvc::sp_request /*req*/, evmvc::sp_response /*res*/,
            async_cb cb
        ){
            rt_val = "abc-c";
            
            cb(nullptr);
        });
        
        // // # optional parameter can be defined
        // // # by enclosing the parameter in square brackets
        // // /abc-d/123/:p1/[:p2]
        // r->get("/abc-d/123/:p1/[:p2]",
        // [&rt_val](const evmvc::request& /*req*/, evmvc::response& /*res*/,
        //      async_cb cb
        // ){
        //     rt_val = "abc-d";
        // });
        
        // # ECMAScript regex can be use to validate parameter by enclosing
        // # the rules inside parentheses following the parameter name
        // /abc-e/123/:p1(\\d+)/[:p2]
        r->get("/abc-e/123/:p1(\\d+)/:[p2]",
        [&rt_val](
            const evmvc::sp_request /*req*/, evmvc::sp_response /*res*/,
            async_cb cb
        ){
            rt_val = "abc-e";
            
            cb(nullptr);
        });
        
        // # regex parameter can be optional as well
        // /abc-f/123/[:p1(\\d+)]
        r->get("/abc-f/123/:[p1(\\d+)]",
        [&rt_val](const evmvc::sp_request /*req*/, evmvc::sp_response /*res*/,
            async_cb cb
        ){
            rt_val = "abc-f";

            cb(nullptr);
        });
        
        // # all parameters following an optional parameter must be optional
        // /abc-g/123/:p1(\\d+)/[:p2]/[:p3]
        r->get("/abc-g/123/:p1(\\d+)/:[p2]/:[p3]",
        [&rt_val](const evmvc::sp_request req, evmvc::sp_response /*res*/,
            async_cb next
        ){
            rt_val = "abc-g";
            
            const auto& p1 = req->route_param("p1");
            auto p1val = p1->get<int32_t>();
            ASSERT_EQ(p1val, 4);
            
            if(req->route_param("p2")){
                auto p2val = req->get_route_param<std::string>("p2");
                ASSERT_STREQ(p2val.c_str(), "arg2");
                
                if(req->route_param("p3")){
                    ASSERT_STREQ(
                        req->route_param("p3")->get<std::string>().c_str(),
                        "arg3"
                    );
                }
            }
            
            next(nullptr);
        });
        
        //_internal::app_request* ar = nullptr;
        evhtp_request_t* ev_req = nullptr;
        // evmvc::sp_http_cookies c =
        //     std::make_shared<evmvc::http_cookies>(
        //         nullptr, ev_req
        //     );
        // evmvc::sp_response res = std::make_shared<evmvc::response>(
        //     srv, srv->log(), ev_req, c
        // );
        
        auto rr = r->resolve_url(evmvc::method::get, "/abc-c/123/asdflkj/asdf");
        if(!rr)
            FAIL();
        
        auto res = _internal::create_http_response(
            rr->log(), ev_req, rr->_route, rr->params
        );
        
        rr->execute(res,
        [r, &rr, res, ev_req,/* &res,*/ &rt_val](auto error){
            
            ASSERT_EQ(rt_val, "abc-c");
            
            rt_val.clear();
            rr = r->resolve_url(evmvc::method::get, "/abc-g/123/a4/arg2/arg3");
            if(rr)
                FAIL();
            rr = r->resolve_url(evmvc::method::get, "/abc-g/123/4/arg2/arg3");
            if(!rr)
                FAIL();
            rr->execute(res,
            [r, &rr, &rt_val](auto error){
                
                ASSERT_EQ(rt_val, "abc-g");
                
            });
        });
        
    }catch(const std::exception& err){
        std::cout << "Error: " << err.what() << std::endl;
        FAIL();
    }
}

}} //ns evevmvc::tests
