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

#ifndef _libevmvc_files_h
#define _libevmvc_files_h

#include "stable_headers.h"
#include "utils.h"

namespace evmvc {

class http_file;
typedef std::shared_ptr<http_file> sp_http_file;

class http_files;
typedef std::shared_ptr<http_files> sp_http_files;

class http_file
{
    friend class http_files;
    
public:
    http_file(
        const std::string& name,
        const std::string& filename,
        const std::string& mime_type,
        const bfs::path& temp_path,
        size_t size)
        : _name(name),
        _filename(filename),
        _mime_type(mime_type),
        _temp_path(temp_path),
        _size(size)
    {
    }
    
    ~http_file()
    {
        boost::system::error_code ec;
        if(!_temp_path.empty() && bfs::exists(_temp_path, ec)){
            if(ec){
                md::log::default_logger()->warn(MD_ERR(
                    "Unable to verify file '{}' existence!\n{}",
                    _temp_path.string(), ec.message()
                ));
                return;
            }
            bfs::remove(_temp_path, ec);
            if(ec)
                md::log::default_logger()->warn(MD_ERR(
                    "Unable to remove file '{}'!\n{}",
                    _temp_path.string(), ec.message()
                ));
        }
    }
    
    const std::string& name() const { return _name;}
    const std::string& filename() const { return _filename;}
    const std::string& mime_type() const { return _mime_type;}
    size_t size() const { return _size;}
    
    void save(bfs::path new_filepath, bool overwrite = false)
    {
        if(bfs::is_directory(new_filepath))
            new_filepath /= _filename;
        
        bfs::copy_file(
            _temp_path, new_filepath,
            overwrite ?
                bfs::copy_option::overwrite_if_exists :
                bfs::copy_option::fail_if_exists
        );
    }
    
private:
    std::string _name;
    std::string _filename;
    std::string _mime_type;
    bfs::path _temp_path;
    size_t _size;
    
};

class http_files
{
    friend class evmvc::request;
    
public:
    http_files()
    {
    }
    
    size_t size() const { return _files.size();}
    
    sp_http_file get(size_t idx) const
    {
        if(idx >= _files.size())
            throw MD_ERR("Index is out of bound!");
        return _files[idx];
    }
    
    sp_http_file operator [](size_t idx) const
    {
        if(idx >= _files.size())
            throw MD_ERR("Index is out of bound!");
        return _files[idx];
    }
    
    sp_http_file get(md::string_view name) const
    {
        for(auto& f : _files)
            if(strcasecmp(f->_name.c_str(), name.data()) == 0)
                return f;
        return sp_http_file();
    }
    
    
private:
    std::vector<sp_http_file> _files;
    
};
    
} //ns evmvc
#endif //_libevmvc_files_h
