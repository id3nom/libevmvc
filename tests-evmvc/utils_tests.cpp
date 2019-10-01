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

#include <sys/types.h>
#include <sys/socket.h>

#define EVMVC_COUT std::cout << "[--------->] " <<
namespace evmvc { namespace tests {


class utils_test: public testing::Test
{
public:
};

TEST_F(utils_test, encode_uri)
{
    // Reserved Characters
    std::string set1 = ";,/?:@&=+$#";
    // Unreserved Marks
    std::string set2 = "-_.!~*'()";
    // Alphanumeric Characters + Space
    std::string set3 = "ABC abc 123";
    
    EVMVC_COUT std::endl << std::endl;
    // ;,/?:@&=+$#
    EVMVC_COUT evmvc::encode_uri(set1) << std::endl;
    ASSERT_STREQ(evmvc::encode_uri(set1).c_str(), ";,/?:@&=+$#");
    // -_.!~*'()
    EVMVC_COUT evmvc::encode_uri(set2) << std::endl;
    ASSERT_STREQ(evmvc::encode_uri(set2).c_str(), "-_.!~*'()");
    // ABC%20abc%20123 (the space gets encoded as %20)
    EVMVC_COUT evmvc::encode_uri(set3) << std::endl;
    ASSERT_STREQ(evmvc::encode_uri(set3).c_str(), "ABC%20abc%20123");
    
    
    EVMVC_COUT std::endl << std::endl;
    // %3B%2C%2F%3F%3A%40%26%3D%2B%24%23
    EVMVC_COUT evmvc::encode_uri_component(set1) << std::endl;
    ASSERT_STREQ(
        evmvc::encode_uri_component(set1).c_str(),
        "%3B%2C%2F%3F%3A%40%26%3D%2B%24%23"
    );
    // -_.!~*'()
    EVMVC_COUT evmvc::encode_uri_component(set2) << std::endl;
    ASSERT_STREQ(evmvc::encode_uri_component(set2).c_str(), "-_.!~*'()");
    // ABC%20abc%20123 (the space gets encoded as %20)
    EVMVC_COUT evmvc::encode_uri_component(set3) << std::endl;
    ASSERT_STREQ(evmvc::encode_uri_component(set3).c_str(), "ABC%20abc%20123");
    
    
    EVMVC_COUT std::endl << std::endl;
    EVMVC_COUT evmvc::decode_uri(
        evmvc::encode_uri(set1)) << std::endl;
    ASSERT_STREQ(
        evmvc::decode_uri(evmvc::encode_uri(set1)).c_str(),
        set1.c_str()
    );
    EVMVC_COUT evmvc::decode_uri(
        evmvc::encode_uri(set2)) << std::endl;
    ASSERT_STREQ(
        evmvc::decode_uri(evmvc::encode_uri(set2)).c_str(),
        set2.c_str()
    );
    EVMVC_COUT evmvc::decode_uri(
        evmvc::encode_uri(set3)) << std::endl;
    ASSERT_STREQ(
        evmvc::decode_uri(evmvc::encode_uri(set3)).c_str(),
        set3.c_str()
    );
    
    
    EVMVC_COUT std::endl << std::endl;
    EVMVC_COUT evmvc::decode_uri_component(
        evmvc::encode_uri_component(set1)) << std::endl;
    ASSERT_STREQ(
        evmvc::decode_uri_component(evmvc::encode_uri_component(set1)).c_str(),
        set1.c_str()
    );
    EVMVC_COUT evmvc::decode_uri_component(
        evmvc::encode_uri_component(set2)) << std::endl;
    ASSERT_STREQ(
        evmvc::decode_uri_component(evmvc::encode_uri_component(set2)).c_str(),
        set2.c_str()
    );
    EVMVC_COUT evmvc::decode_uri_component(
        evmvc::encode_uri_component(set3)) << std::endl;
    ASSERT_STREQ(
        evmvc::decode_uri_component(evmvc::encode_uri_component(set3)).c_str(),
        set3.c_str()
    );
    
}



}} //ns evevmvc::tests
