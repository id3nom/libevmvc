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

#ifndef _libevmvc_response_h
#define _libevmvc_response_h

#include "stable_headers.h"

namespace evmvc {

class response
{
    
};

// class response_base
//     : public std::enable_shared_from_this<response_base>
// {
// public:
//     response_base()
//         : _ended(false)
//     {
//     }
    
//     bool ended() const noexcept { return _ended;}
//     void end() noexcept
//     {
//         _ended = true;
//     }
    
//     virtual void send_bad_request(evmvc::string_view why) = 0;
    
// protected:
//     bool _ended;
// };

// template<class ReqBody, class ReqAllocator, class Send>
// class response
//     : public response_base
// {
// public:
//     response(
//         const http::request<ReqBody, http::basic_fields<ReqAllocator>>& req,
//         Send& send)
//         : response_base(),
//         _req(req), _send(send)
//     {
//     }
    
//     void send_bad_request(evmvc::string_view why)
//     {
//         http::response<http::string_body> res{
//             http::status::bad_request,
//             _req.version()
//         };
//         res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
//         res.set(http::field::content_type, "text/html");
//         res.keep_alive(_req.keep_alive());
//         res.body() = why.to_string();
//         res.prepare_payload();
        
//         _send(std::move(res));
//     }
    
    
    
// private:
//     //http::request<http::string_body> _req;
//     http::request<ReqBody, http::basic_fields<ReqAllocator>> _req;
//     Send& _send;
// };

} //ns evmvc
#endif //_libevmvc_response_h
