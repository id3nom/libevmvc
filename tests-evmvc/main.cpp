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

int main(int argc, char** argv)
{
    // initializing globals
    std::vector<md::log::sinks::sp_logger_sink> sinks;
    
    auto out_sink = std::make_shared<md::log::sinks::console_sink>(true);
    out_sink->set_level(md::log::log_level::trace);
    sinks.emplace_back(out_sink);
    
    auto _log = std::make_shared<md::log::logger>(
    "/", sinks.begin(), sinks.end()
    );
    _log->set_level(md::log::log_level::trace);
    md::log::default_logger() = _log;
    
    testing::InitGoogleMock(&argc, argv);
    int r = RUN_ALL_TESTS();
    
    return r;
}