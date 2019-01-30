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

#ifndef _libevmvc_utils_h
#define _libevmvc_utils_h

#include <string>
#include <sstream>
#include <algorithm> 
#include <cctype>
#include <locale>
#include <string>
#include <numeric>
#include <type_traits>
#include <functional>
#include <chrono>

namespace evmvc {

inline void lower_case(std::string& str)
{
    std::transform(
        str.begin(), str.end(), str.begin(),
        [](unsigned char c){ return std::tolower(c); }
    );
}

inline std::string lower_case_copy(std::string str)
{
    lower_case(str);
    return str;
}


inline bool iequals(const std::string& a, const std::string& b)
{
    return (
        (a.size() == b.size()) && 
        std::equal(
            a.begin(), a.end(),
            b.begin(), 
            [](const char& ca, const char& cb)-> bool {
                return ca == cb || std::tolower(ca) == std::tolower(cb);
            }
        )
    );
}

class no_grouping : public std::numpunct_byname<char> {
    std::string do_grouping() const { return ""; }
public:
    no_grouping() : numpunct_byname("") {}
};

template <typename T>
inline std::string num_to_str(T number, bool grouping = true)
{
    std::stringstream ss;
    if(!grouping)
        ss.imbue(std::locale(std::locale(""), new no_grouping));
    
    ss << number;
    return ss.str();
}

template <typename T>
inline T str_to_num(const std::string& text)
{
    std::stringstream ss(text);
    T result;
    return ss >> result ? result : 0;
}

// trim from start (in place)
inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
inline void trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
inline std::string ltrim_copy(std::string s)
{
    ltrim(s);
    return s;
}

// trim from end (copying)
inline std::string rtrim_copy(std::string s)
{
    rtrim(s);
    return s;
}

// trim from both ends (copying)
inline std::string trim_copy(std::string s)
{
    trim(s);
    return s;
}

template< typename T >
inline std::string join(const T& lst, const std::string& separator)
{
    static_assert(
        std::is_same<typename T::value_type, std::string>::value,
        "Type T must be a container of std::string type"
    );
    return
        std::accumulate(lst.begin(), lst.end(), std::string(),
            [&separator](const std::string& a, const std::string& b) -> std::string { 
                return a + (a.length() > 0 ? separator : "") + b;
            }
        );
}

inline void replace_substring(
    std::string& s, const std::string& f, const std::string& t)
{
    for (auto pos = s.find(f);            // find first occurrence of f
        pos != std::string::npos;         // make sure f was found
        s.replace(pos, f.size(), t),      // replace with t, and
        pos = s.find(f, pos + t.size()))  // find next occurrence of f
    {}
}

inline std::string replace_substring_copy(
    std::string s, const std::string& f, const std::string& t)
{
    replace_substring(s, f, t);
    return s;
}

//std::string hex_to_str(const uint8_t* data, int len);
constexpr char hexmap[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

inline std::string hex_to_str(const uint8_t* data, int len)
{
    std::string s(len * 2, ' ');
    for (int i = 0; i < len; ++i) {
        s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
        s[2 * i + 1] = hexmap[data[i] & 0x0F];
    }
    return s;
}


} //ns evmvc
#endif //_boost_beast_mvc_utils_h