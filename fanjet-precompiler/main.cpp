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

#include <string>
#include <iostream>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

int main(int argc, char** argv)
{
    po::options_description desc("fanjet");
    
    desc.add_options()
    ("help,h", "produce help message")
    ("namespace,n",
        po::value<std::string>()->required(),
        "set namespace for the specified views directory"
    )
    ("include,i",
        po::value<std::string>()->required(),
        "views generated include filename"
    )
    ("src,s",
        po::value<std::string>()->required(),
        "views source directory to precompile"
    )
    ("dest,d",
        po::value<std::string>()->required(),
        "views precompiled destination directory"
    )
    ;
    
    po::positional_options_description p;
    p.add("src", 1);
    p.add("dest", 2);
    
    po::variables_map vm;
    try{
        po::store(
            po::command_line_parser(argc, argv)
            .options(desc)
            .positional(p)
            .run(),
            vm
        );
        
        po::notify(vm);
        if(vm.count("help")){
            std::cout 
                << "Usage: fanjet [options] src-path dest-path\n"
                << desc << std::endl;
            return -1;
        }
        
        std::cout
            << "processing, "
            << "src: '" << vm["views-src-dir"].as<std::string>() << "', "
            << "dest: '" << vm["views-dest-dir"].as<std::string>() << "'"
            << std::endl;
        
        return 0;
    }catch(const std::exception& err){
        if(vm.count("help")){
            std::cout 
                << "Usage: fanjet [options] views-src-dir views-dest-dir\n"
                << desc << std::endl;
            return -1;
        }
        
        std::cout << err.what() << "\n\n"
            << "Usage: fanjet [options] views-src-dir views-dest-dir\n"
            << desc << std::endl;
        return -1;
    }
}