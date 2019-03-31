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
#include <evmvc/fanjet/fanjet.h>

namespace po = boost::program_options;
namespace bfs = boost::filesystem;

void march_dir(
    std::vector<evmvc::fanjet::document>& docs,
    const bfs::path& root,
    const std::string& ns,
    const bfs::path& src,
    const bfs::path& dest
);

void process_fanjet_file(
    std::vector<evmvc::fanjet::document>& docs,
    const bfs::path& root,
    const std::string& ns,
    const bfs::path& src,
    const bfs::path& dest
);

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
            << "ns: '" << vm["namespace"].as<std::string>() << "', "
            << "src: '" << vm["src"].as<std::string>() << "', "
            << "dest: '" << vm["dest"].as<std::string>() << "'"
            << std::endl;
        
        std::vector<evmvc::fanjet::document> docs;
        march_dir(
            docs,
            vm["src"].as<std::string>(),
            vm["namespace"].as<std::string>(),
            vm["src"].as<std::string>(),
            vm["dest"].as<std::string>()
        );
        
        
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


void march_dir(
    std::vector<evmvc::fanjet::document>& docs,
    const bfs::path& root,
    const std::string& ns,
    const bfs::path& src,
    const bfs::path& dest)
{
    if(!bfs::exists(dest)){
        bfs::create_directories(dest);
    }
    
    for(bfs::directory_entry& x : bfs::directory_iterator(src)){
        switch(x.status().type()){
            case bfs::file_type::directory_file:
                march_dir(
                    docs,
                    root,
                    ns,
                    x.path(),
                    dest / *x.path().rbegin()
                );
                break;
                
            case bfs::file_type::regular_file:
                process_fanjet_file(
                    docs,
                    root,
                    ns,
                    x.path(),
                    dest / x.path().filename()
                );
                break;
                
            default:
                throw MD_ERR(
                    "Only directory and regular file are supported!"
                );
        }
    }
}

void process_fanjet_file(
    std::vector<evmvc::fanjet::document>& docs,
    const bfs::path& root,
    const std::string& ns,
    const bfs::path& src,
    const bfs::path& dest)
{
    bfs::ifstream fin(src);
    std::ostringstream ostrm;
    ostrm << fin.rdbuf();
    std::string fan_src = ostrm.str();
    fin.close();
    
    std::string view_path = 
        src.parent_path().string().substr(
            root.size()
        );
    view_path += 
        src.filename().string().substr(
            src.extension().string().size()
        );
    
    evmvc::fanjet::document doc = 
        evmvc::fanjet::parser::generate(
            ns,
            view_path,
            fan_src
        );
    
}