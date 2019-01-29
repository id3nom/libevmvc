/*
    This file is part of libpq-async++
    Copyright (C) 2011-2018 Michel Denommee (and other contributing authors)
    
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "evmvc_config.h"

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <deque>
#include <vector>

#include "fmt/format.h"

#ifndef _libevmvc_stable_headers_h
#define _libevmvc_stable_headers_h

#include <boost/logic/tribool.hpp>

#if defined(EVENT_MVC_USE_STD_STRING_VIEW)
    #include <string_view>
#else
    #include <boost/utility/string_view.hpp>
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
    #include "nlohmann/json.hpp"
#pragma GCC diagnostic pop

namespace evmvc {

#ifdef EVENT_MVC_USE_STD_STRING_VIEW
    /// The type of string view used by the library
    using string_view = std::string_view;
    
    /// The type of basic string view used by the library
    template<class CharT, class Traits>
    using basic_string_view = std::basic_string_view<CharT, Traits>;

#else
    /// The type of string view used by the library
    using string_view = boost::string_view;
    
    /// The type of basic string view used by the library
    template<class CharT, class Traits>
    using basic_string_view = boost::basic_string_view<CharT, Traits>;

#endif

typedef nlohmann::json json;


class app;
typedef std::shared_ptr<app> sp_app;

} //ns evmvc
#endif //_libevmvc_stable_headers_h
