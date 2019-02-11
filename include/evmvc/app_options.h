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

#ifndef _libevmvc_app_options_h
#define _libevmvc_app_options_h

#include "stable_headers.h"
#include "logging.h"

namespace evmvc {

class app_options
{
public:
    app_options()
        : base_dir(bfs::current_path()),
        view_dir(base_dir / "views"),
        temp_dir(base_dir / "temp"),
        cache_dir(base_dir / "cache"),
        log_dir(base_dir / "logs"),
        secure(false),
        use_default_logger(true),
        
        log_console_level(log_level::warning),
        log_console_enable_color(true),
        log_file_level(log_level::warning),
        log_file_max_size(1048576 * 5),
        log_file_max_files(7),
        stack_trace_enabled(false)
    {
    }

    app_options(const bfs::path& base_directory)
        : base_dir(base_directory),
        view_dir(base_dir / "views"),
        temp_dir(base_dir / "temp"),
        cache_dir(base_dir / "cache"),
        log_dir(base_dir / "logs"),
        secure(false),
        use_default_logger(true),
        
        log_console_level(log_level::warning),
        log_console_enable_color(true),
        log_file_level(log_level::warning),
        log_file_max_size(1048576 * 5),
        log_file_max_files(7),
        stack_trace_enabled(false)
    {
    }
    
    app_options(const evmvc::app_options& other)
        : base_dir(other.base_dir), 
        view_dir(other.view_dir),
        temp_dir(other.temp_dir),
        cache_dir(other.cache_dir),
        log_dir(other.log_dir),
        secure(other.secure),
        use_default_logger(other.use_default_logger),
        
        log_console_level(other.log_console_level),
        log_console_enable_color(other.log_console_enable_color),
        log_file_level(other.log_file_level),
        log_file_max_size(other.log_file_max_size),
        log_file_max_files(other.log_file_max_files),
        stack_trace_enabled(other.stack_trace_enabled)
    {
    }
    
    app_options(evmvc::app_options&& other)
        : base_dir(std::move(other.base_dir)),
        view_dir(std::move(other.view_dir)),
        temp_dir(std::move(other.temp_dir)),
        cache_dir(std::move(other.cache_dir)),
        log_dir(std::move(other.log_dir)),
        secure(other.secure),
        use_default_logger(other.use_default_logger),
        
        log_console_level(other.log_console_level),
        log_console_enable_color(other.log_console_enable_color),
        log_file_level(other.log_file_level),
        log_file_max_size(other.log_file_max_size),
        log_file_max_files(other.log_file_max_files),
        stack_trace_enabled(other.stack_trace_enabled)
    {
    }
    
    bfs::path base_dir;
    bfs::path view_dir;
    bfs::path temp_dir;
    bfs::path cache_dir;
    bfs::path log_dir;
    
    bool secure;
    
    bool use_default_logger;
    
    log_level log_console_level;
    bool log_console_enable_color;
    
    log_level log_file_level;
    size_t log_file_max_size;
    size_t log_file_max_files;
    
    bool stack_trace_enabled;
};

} // ns: evmvmc
#endif //_libevmvc_app_options_h
