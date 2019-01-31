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

#include "stable_headers.h"

#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>

#ifndef _libevmvc_stack_debug_h
#define _libevmvc_stack_debug_h

namespace evmvc { namespace _miscs{

/** Print a demangled stack backtrace of the caller function to FILE* out. */
inline std::string get_stacktrace(
    //FILE* out = stderr,
    unsigned int max_frames = 63)
{
    std::string out;
    out += "stack trace:\n";
    //fprintf(out, "stack trace:\n");

    // storage array for stack trace address data
    void* addrlist[max_frames+1];
    
    // retrieve current stack addresses
    int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

    if(addrlen == 0){
        out += "  <empty, possibly corrupt>\n";
        //fprintf(out, "  <empty, possibly corrupt>\n");
        return out;
    }
    
    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(addrlist, addrlen);
    
    for(int i = 0; i < addrlen; ++i){
        printf("[%d] %p, %s\n\n", i, addrlist[i], symbollist[i]);
    }
    
    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char* funcname = (char*)malloc(funcnamesize);
    
    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for(int i = 1; i < addrlen; ++i){
        char *begin_name = 0, *begin_offset = 0, *end_offset = 0;
        
        // find parentheses and +address offset surrounding the mangled name:
        // ./module(function+0x15c) [0x8048a6d]
        for(char *p = symbollist[i]; *p; ++p){
            if(*p == '(')
                begin_name = p;
            else if(*p == '+')
                begin_offset = p;
            else if(*p == ')' && begin_offset){
                end_offset = p;
                break;
            }
        }

        if(begin_name && begin_offset && end_offset &&
            begin_name < begin_offset
        ){
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';

            // mangled name is now in [begin_name, begin_offset) and caller
            // offset in [begin_offset, end_offset). now apply
            // __cxa_demangle():
            
            int status;
            char* ret = abi::__cxa_demangle(
                begin_name, funcname, &funcnamesize, &status
            );
            
            if(status == 0){
                funcname = ret; // use possibly realloc()-ed string
                out += fmt::format(
                    "  {} :\n    {}+{}\n",
                    symbollist[i], funcname, begin_offset
                );
                // fprintf(
                //     out,
                //     "  %s : %s+%s\n",
                //     symbollist[i], funcname, begin_offset
                // );
            }else{
                // demangling failed. Output function name as a C function with
                // no arguments.
                out += fmt::format(
                    "  {} :\n    {}()+{}\n",
                    symbollist[i], begin_name, begin_offset
                );
                // fprintf(
                //     out,
                //     "  %s : %s()+%s\n",
                //     symbollist[i], begin_name, begin_offset
                // );
            }
        }else{
            // couldn't parse the line? print the whole line.
            out += fmt::format("  {}\n", symbollist[i]);
            //fprintf(out, "  %s\n", symbollist[i]);
        }
        
        /* find first occurence of '(' or ' ' in message[i] and assume
        * everything before that is the file name. (Don't go beyond 0 though
        * (string terminator)*/
        size_t p = 0;
        while(symbollist[i][p] != '(' && symbollist[i][p] != ' '
                && symbollist[i][p] != 0)
            ++p;
        
        char syscom[256];
        //last parameter is the file name of the symbol
        sprintf(
            syscom,
            "addr2line -Cf -e %.*s %p",
            (int)p,
            symbollist[i],
            addrlist[i]
        );
        system(syscom);
    }
    
    free(funcname);
    free(symbollist);
    
    return out;
}

}} //ns evmvc::_miscs
#endif //_libevmvc_stack_debug_h
