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

#ifndef _libevmvc_response_data_h
#define _libevmvc_response_data_h

#include "stable_headers.h"

namespace evmvc {

class response_data_base_t
{
public:
    virtual std::string to_string() const = 0;
};
typedef std::shared_ptr<response_data_base_t> response_data_base;

template<typename T>
class response_data_t
    : public response_data_base_t
{
public:
    response_data_t(const T& v)
        : _v(v)
    {
    }
    
    const T& value() const
    {
        return _v;
    }
    
    template<
        typename U,
        typename TT = T,
        typename std::enable_if<
            std::is_convertible<TT, U>::value
        , int32_t>::type = -1
    >
    U value() const
    {
        return (U)_v;
    }
    
    std::string to_string() const
    {
        std::stringstream ss;
        ss << _v;
        return ss.str();
    }
    
    template<typename U>
    operator U() const
    {
        return value<U>();
    }
    
private:
    T _v;
};
template<typename T>
using response_data = std::shared_ptr<response_data_t<T>>;
typedef std::unordered_map<std::string, response_data_base> response_data_map_t;
typedef std::shared_ptr<response_data_map_t> response_data_map;

// bool conversion
template<>
inline std::string response_data_t<bool>::to_string() const
{
    return _v ? "true" : "false";
}

// text conversion
template<>
inline std::string response_data_t<std::string>::to_string() const
{
    return _v;
}
template<>
inline std::string response_data_t<md::string_view>::to_string() const
{
    return _v.data();
}
template<>
inline std::string response_data_t<const char*>::to_string() const
{
    return _v;
}

// number convertions
template<> inline std::string response_data_t<int16_t>::to_string() const
{
    return md::num_to_str(_v, false);
}
template<> inline std::string response_data_t<int32_t>::to_string() const
{
    return md::num_to_str(_v, false);
}
template<> inline std::string response_data_t<int64_t>::to_string() const
{
    return md::num_to_str(_v, false);
}

template<> inline std::string response_data_t<uint16_t>::to_string() const
{
    return md::num_to_str(_v, false);
}
template<> inline std::string response_data_t<uint32_t>::to_string() const
{
    return md::num_to_str(_v, false);
}
template<> inline std::string response_data_t<uint64_t>::to_string() const
{
    return md::num_to_str(_v, false);
}

template<> inline std::string response_data_t<float>::to_string() const
{
    return md::num_to_str(_v, false);
}
template<> inline std::string response_data_t<double>::to_string() const
{
    return md::num_to_str(_v, false);
}


}//::evmvc
#endif //_libevmvc_response_data_h
