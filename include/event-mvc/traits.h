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


#ifndef _libevent_mvc_traits_h
#define _libevent_mvc_traits_h

#include <type_traits>
#include <functional>

#ifndef __cpp_lib_transformation_trait_aliases
namespace std{
    template< class T >
    using remove_cv_t       = typename std::remove_cv<T>::type;

    template< class T >
    using remove_const_t    = typename std::remove_const<T>::type;

    template< class T >
    using remove_volatile_t = typename std::remove_volatile<T>::type;
}
#endif

namespace std{
#if __cplusplus <= 201103L
    // template< std::size_t Len, std::size_t Align = /*default-alignment*/ >
    // using aligned_storage_t = typename aligned_storage<Len, Align>::type;
    template<size_t _Len, size_t _Align =
        __alignof__(typename __aligned_storage_msa<_Len>::__type)>
        using aligned_storage_t = typename aligned_storage<_Len, _Align>::type;

    template< bool B, class T = void >
    using enable_if_t = typename enable_if<B,T>::type;

    template< class T >
    using decay_t = typename decay<T>::type;
#endif
#if __cplusplus <= 201402L
    
    template <typename F, typename... Args>
    struct is_invocable :
        std::is_constructible<
            std::function<void(Args ...)>,
            std::reference_wrapper<typename std::remove_reference<F>::type>
        >
    {
    };

    template <typename R, typename F, typename... Args>
    struct is_invocable_r :
        std::is_constructible<
            std::function<R(Args ...)>,
            std::reference_wrapper<typename std::remove_reference<F>::type>
        >
    {
    };
}
#endif

namespace event{ namespace mvc {

template <class... Args>
struct select_last
{
};
template <typename T>
struct select_last<T>
{
    using type = T;
};
template <class T, class... Args>
struct select_last<T, Args...>
{
    using type = typename select_last<Args...>::type;
};


}}// ns: event::mvc



#endif //_libevent_mvc_traits_h
