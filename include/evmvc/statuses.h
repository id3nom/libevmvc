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

#ifndef _libevmvc_statuses_h
#define _libevmvc_statuses_h

#include "stable_headers.h"

namespace evmvc {

enum class status
    : uint16_t
{
    continue_status = 100,
    switching_protocols = 101,
    processing = 102,
    early_hints = 103,
    ok = 200,
    created = 201,
    accepted = 202,
    non_authoritative_information = 203,
    no_content = 204,
    reset_content = 205,
    partial_content = 206,
    multi_status = 207,
    already_reported = 208,
    im_used = 226,
    multiple_choices = 300,
    moved_permanently = 301,
    found = 302,
    see_other = 303,
    not_modified = 304,
    use_proxy = 305,
    unused = 306,
    temporary_redirect = 307,
    permanent_redirect = 308,
    bad_request = 400,
    unauthorized = 401,
    payment_required = 402,
    forbidden = 403,
    not_found = 404,
    method_not_allowed = 405,
    not_acceptable = 406,
    proxy_authentication_required = 407,
    request_timeout = 408,
    conflict = 409,
    gone = 410,
    length_required = 411,
    precondition_failed = 412,
    payload_too_large = 413,
    uri_too_long = 414,
    unsupported_media_type = 415,
    range_not_satisfiable = 416,
    expectation_failed = 417,
    i_m_a_teapot = 418,
    misdirected_request = 421,
    unprocessable_entity = 422,
    locked = 423,
    failed_dependency = 424,
    too_early = 425,
    upgrade_required = 426,
    precondition_required = 428,
    too_many_requests = 429,
    request_header_fields_too_large = 431,
    unavailable_for_legal_reasons = 451,
    internal_server_error = 500,
    not_implemented = 501,
    bad_gateway = 502,
    service_unavailable = 503,
    gateway_timeout = 504,
    http_version_not_supported = 505,
    variant_also_negotiates = 506,
    insufficient_storage = 507,
    loop_detected = 508,
    bandwidth_limit_exceeded = 509,
    not_extended = 510,
    network_authentication_required = 511
};

class statuses
{
    statuses() = delete;
public:
    
    static const evmvc::json& redirect()
    {
        static evmvc::json _redirect = {
            {300, true},
            {301, true},
            {302, true},
            {303, true},
            {305, true},
            {307, true},
            {308, true},
        };
        
        return _redirect;
    }

    static const evmvc::json& empty()
    {
        static evmvc::json _empty = {
            {204, true},
            {205, true},
            {304, true}
        };
        
        return _empty;
    }

    static const evmvc::json& retry()
    {
        static evmvc::json _retry = {
            {502, true},
            {503, true},
            {504, true}
        };
        
        return _retry;
    }
    
    static evmvc::string_view category(int16_t code)
    {
        if(code < 200)
            return "Info";
        else if(code < 300)
            return "Success";
        else if(code < 400)
            return "Redirection";
        else if(code < 500)
            return "Client error";
        return "Server error";
    }
    static evmvc::string_view category(evmvc::status s)
    {
        return category((int16_t)s);
    }
    
    static evmvc::string_view color(int16_t code)
    {
        if(code < 300)
            return "green";
        if(code < 400)
            return "orange";
        if(code < 500)
            return "royalblue";
            
        return "red";
    }
    static evmvc::string_view color(evmvc::status s)
    {
        return color((int16_t)s);
    }
    
    static const evmvc::json& codes()
    {
        static evmvc::json _codes = {
            {"100", "Continue"},
            {"101", "Switching Protocols"},
            {"102", "Processing"},
            {"103", "Early Hints"},
            {"200", "OK"},
            {"201", "Created"},
            {"202", "Accepted"},
            {"203", "Non-Authoritative Information"},
            {"204", "No Content"},
            {"205", "Reset Content"},
            {"206", "Partial Content"},
            {"207", "Multi-Status"},
            {"208", "Already Reported"},
            {"226", "IM Used"},
            {"300", "Multiple Choices"},
            {"301", "Moved Permanently"},
            {"302", "Found"},
            {"303", "See Other"},
            {"304", "Not Modified"},
            {"305", "Use Proxy"},
            {"306", "(Unused)"},
            {"307", "Temporary Redirect"},
            {"308", "Permanent Redirect"},
            {"400", "Bad Request"},
            {"401", "Unauthorized"},
            {"402", "Payment Required"},
            {"403", "Forbidden"},
            {"404", "Not Found"},
            {"405", "Method Not Allowed"},
            {"406", "Not Acceptable"},
            {"407", "Proxy Authentication Required"},
            {"408", "Request Timeout"},
            {"409", "Conflict"},
            {"410", "Gone"},
            {"411", "Length Required"},
            {"412", "Precondition Failed"},
            {"413", "Payload Too Large"},
            {"414", "URI Too Long"},
            {"415", "Unsupported Media Type"},
            {"416", "Range Not Satisfiable"},
            {"417", "Expectation Failed"},
            {"418", "I'm a teapot"},
            {"421", "Misdirected Request"},
            {"422", "Unprocessable Entity"},
            {"423", "Locked"},
            {"424", "Failed Dependency"},
            {"425", "Too Early"},
            {"426", "Upgrade Required"},
            {"428", "Precondition Required"},
            {"429", "Too Many Requests"},
            {"431", "Request Header Fields Too Large"},
            {"451", "Unavailable For Legal Reasons"},
            {"500", "Internal Server Error"},
            {"501", "Not Implemented"},
            {"502", "Bad Gateway"},
            {"503", "Service Unavailable"},
            {"504", "Gateway Timeout"},
            {"505", "HTTP Version Not Supported"},
            {"506", "Variant Also Negotiates"},
            {"507", "Insufficient Storage"},
            {"508", "Loop Detected"},
            {"509", "Bandwidth Limit Exceeded"},
            {"510", "Not Extended"},
            {"511", "Network Authentication Required"}
        };
        return _codes;
    }
    
    static std::string status(int16_t code)
    {
        std::string sc = evmvc::num_to_str(code, false);
        auto it = codes().find(sc);
        if(it == codes().end())
            return "Invalid status code!";
        else
            return *it;
    }
    static std::string status(evmvc::status s)
    {
        return status((int16_t)s);
    }
    
    static int16_t code(evmvc::string_view msg)
    {
        static evmvc::json _msgs = {
            {"continue", 100},
            {"switching protocols", 101},
            {"processing", 102},
            {"early hints", 103},
            {"ok", 200},
            {"created", 201},
            {"accepted", 202},
            {"non-authoritative information", 203},
            {"no content", 204},
            {"reset content", 205},
            {"partial content", 206},
            {"multi-status", 207},
            {"already reported", 208},
            {"im used", 226},
            {"multiple choices", 300},
            {"moved permanently", 301},
            {"found", 302},
            {"see other", 303},
            {"not modified", 304},
            {"use proxy", 305},
            {"(unused)", 306},
            {"temporary redirect", 307},
            {"permanent redirect", 308},
            {"bad request", 400},
            {"unauthorized", 401},
            {"payment required", 402},
            {"forbidden", 403},
            {"not found", 404},
            {"method not allowed", 405},
            {"not acceptable", 406},
            {"proxy authentication required", 407},
            {"request timeout", 408},
            {"conflict", 409},
            {"gone", 410},
            {"length required", 411},
            {"precondition failed", 412},
            {"payload too large", 413},
            {"uri too long", 414},
            {"unsupported media type", 415},
            {"range not satisfiable", 416},
            {"expectation failed", 417},
            {"i'm a teapot", 418},
            {"misdirected request", 421},
            {"unprocessable entity", 422},
            {"locked", 423},
            {"failed dependency", 424},
            {"too early", 425},
            {"upgrade required", 426},
            {"precondition required", 428},
            {"too many requests", 429},
            {"request header fields too large", 431},
            {"unavailable for legal reasons", 451},
            {"internal server error", 500},
            {"not implemented", 501},
            {"bad gateway", 502},
            {"service unavailable", 503},
            {"gateway timeout", 504},
            {"http version not supported", 505},
            {"variant also negotiates", 506},
            {"insufficient storage", 507},
            {"loop detected", 508},
            {"bandwidth limit exceeded", 509},
            {"not extended", 510},
            {"network authentication required", 511}
        };
        
        auto it = _msgs.find(msg);
        if(it == _msgs.end())
            return -1;
        else
            return *it;
    }
    
};

std::string to_string(evmvc::status sc)
{
    return evmvc::statuses::status((int16_t)sc);
}

} //ns evmvc
#endif //_libevmvc_statuses_h
