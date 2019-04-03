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
    bool dbg,
    std::vector<evmvc::fanjet::ast::document>& docs,
    const bfs::path& root,
    const std::string& ns,
    const bfs::path& src/*,
    const bfs::path& dest*/
);

void process_fanjet_file(
    bool dbg,
    std::vector<evmvc::fanjet::ast::document>& docs,
    const bfs::path& root,
    const std::string& ns,
    const bfs::path& src/*,
    const bfs::path& dest*/
);

void save_docs(
    const std::string& include_src,
    const std::vector<evmvc::fanjet::ast::document>& docs,
    const bfs::path& dest
);

int main(int argc, char** argv)
{
    po::options_description desc("fanjet");
    
    std::string verbosity_values;
    
    desc.add_options()
    ("help,h", "produce help message")
    ("verbosity,v",
        po::value(&verbosity_values)->implicit_value(""),
        "verbosity level, "
        "v: info, vv: debug, vvv: trace."
    )
    ("debug",
        "generate non optimized debug output."
    )
    ("namespace,n",
        po::value<std::string>()->required(),
        "set namespace for the specified views directory."
    )
    ("include,i",
        po::value<std::string>()->required(),
        "views generated include filename."
    )
    ("src,s",
        po::value<std::string>()->required(),
        "views source directory to precompile."
    )
    ("dest,d",
        po::value<std::string>()->required(),
        "views precompiled destination directory."
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
        
        if(std::any_of(
            begin(verbosity_values), end(verbosity_values), [](auto&c) {
                return c != 'v';
            }
        ))
            throw std::runtime_error("Invalid verbosity level");
        // log level override
        int log_val = -1;
        int vvs = verbosity_values.size();
        if(vm.count("verbosity"))
            vvs += 2;
        if(vvs > 4)
            vvs = 4;
        if(vvs > 0)
            log_val = 
                (int)md::log::log_level::error + vvs;
        if(log_val == -1)
            log_val = (int)md::log::log_level::warning;
        
        std::vector<md::log::sinks::sp_logger_sink> sinks;
        auto out_sink = std::make_shared<md::log::sinks::console_sink>(true);
        out_sink->set_level((md::log::log_level)log_val);
        sinks.emplace_back(out_sink);
        
        auto _log = std::make_shared<md::log::logger>(
        "/", sinks.begin(), sinks.end()
        );
        _log->set_level((md::log::log_level)log_val);
        md::log::default_logger() = _log;
        
        if(md::log::default_logger()->should_log(md::log::log_level::warning)){
            std::cout
                << "fanjet 0.1.0 Copyright (c) 2019 Michel Dénommée\n"
                << std::endl;
        }
        md::log::info(
            "processing\n  ns: '{}'\n  src: '{}'\n  dest: '{}'",
            vm["namespace"].as<std::string>(),
            vm["src"].as<std::string>(),
            vm["dest"].as<std::string>()
        );
        
        bfs::path dest = vm["dest"].as<std::string>();
        if(!bfs::exists(dest))
            bfs::create_directories(dest);
        
        bool dbg = vm.count("debug") > 0;
        
        std::vector<evmvc::fanjet::ast::document> docs;
        march_dir(
            dbg,
            docs,
            vm["src"].as<std::string>(),
            vm["namespace"].as<std::string>(),
            vm["src"].as<std::string>()/*,
            vm["dest"].as<std::string>()*/
        );
        

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        auto ct = std::ctime(&now_time);
        std::stringstream ss;
        ss << "generated by libevmvc fanjet tool v0.1.0 on: " << ct;
        std::string gen_notice = ss.str();
        
        std::string include_src;
        evmvc::fanjet::parser::generate_code(
            gen_notice, include_src, docs, dbg
        );
        
        save_docs(
            include_src,
            docs,
            vm["dest"].as<std::string>()
            );
        
        return 0;
    }catch(const std::exception& err){
        if(vm.count("help")){
            std::cout 
                << "Usage: fanjet [options] src-path dest-path\n"
                << desc << std::endl;
            return -1;
        }
        
        std::stringstream ss;
        ss << desc;
        
        md::log::error(
            "{}\n\nUsage: fanjet [options] src-path dest-path\n{}",
            err.what(), ss.str()
        );
        return -1;
    }
}


void march_dir(
    bool dbg,
    std::vector<evmvc::fanjet::ast::document>& docs,
    const bfs::path& root,
    const std::string& ns,
    const bfs::path& src/*,
    const bfs::path& dest*/)
{
    for(bfs::directory_entry& x : bfs::directory_iterator(src)){
        switch(x.status().type()){
            case bfs::file_type::directory_file:
                march_dir(
                    dbg,
                    docs,
                    root,
                    ns,
                    x.path()/*,
                    dest / *x.path().rbegin()*/
                );
                break;
                
            case bfs::file_type::regular_file:
                process_fanjet_file(
                    dbg,
                    docs,
                    root,
                    ns,
                    x.path()/*,
                    dest / x.path().filename()*/
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
    bool dbg,
    std::vector<evmvc::fanjet::ast::document>& docs,
    const bfs::path& root,
    const std::string& ns,
    const bfs::path& src/*,
    const bfs::path& dest*/)
{
    try{
        md::log::debug("parsing source: '{}'", src.string());
        
        bfs::ifstream fin(src);
        std::ostringstream ostrm;
        ostrm << fin.rdbuf();
        std::string fan_src = ostrm.str();
        fin.close();
        
        std::string view_path = 
            src.parent_path().string().substr(
                root.size()
            );
        if(*(view_path.crend()) != '/')
            view_path += "/";
        view_path += 
            src.filename().string().substr(
                0, src.filename().size() - src.extension().string().size()
            );
        
        evmvc::fanjet::ast::document doc = 
            evmvc::fanjet::parser::generate_doc(
                ns,
                view_path,
                fan_src,
                dbg
            );
        docs.emplace_back(doc);
    }catch(const std::exception& error){
        std::string err_msg = fmt::format(
            "{}\nsrc: {}",
            error.what(), src.string()
        );
        
        throw MD_ERR(err_msg);
    }
}

void save_docs(
    const std::string& include_src,
    const std::vector<evmvc::fanjet::ast::document>& docs,
    const bfs::path& dest)
{
    if(!bfs::exists(dest))
        bfs::create_directories(dest);
    
    for(auto d : docs){
        bfs::path ifn = dest / d->i_filename;
        if(bfs::exists(ifn))
            bfs::remove(ifn);
        
        bfs::ofstream fout(ifn);
        fout << d->i_src;
        fout.close();
    }
}
