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

#include "app.h"
#include "response.h"

namespace evmvc {


evmvc::sp_app request::get_app() const
{
    return this->get_router()->get_app();
}
evmvc::sp_router request::get_router() const
{
    return this->get_route()->get_router();
}

// std::string request::protocol() const
// {
//     //TODO: add trust proxy options
//     sp_header h = _headers->get("X-Forwarded-Proto");
//     if(h)
//         return h->value();
//    
//     return this->get_app()->options().enable_ssl ? "https" : "http";
// }

void request::_load_multipart_params(
    std::shared_ptr<_internal::multipart_subcontent> ms)
{
    for(auto ct : ms->contents){
        if(ct->type == _internal::multipart_content_type::subcontent){
            _load_multipart_params(
                std::static_pointer_cast<
                    _internal::multipart_subcontent
                >(ct)
            );
            
        }else if(ct->type == _internal::multipart_content_type::file){
            std::shared_ptr<_internal::multipart_content_file> mcf = 
                std::static_pointer_cast<
                    _internal::multipart_content_file
                >(ct);
            if(!_files)
                _files = std::make_shared<http_files>();
            _files->_files.emplace_back(
                std::make_shared<http_file>(
                    mcf->name, mcf->filename,
                    mcf->mime_type, mcf->temp_path,
                    mcf->size
                )
            );
        }else if(ct->type == _internal::multipart_content_type::form){
            std::shared_ptr<_internal::multipart_content_form> mcf = 
                std::static_pointer_cast<
                    _internal::multipart_content_form
                >(ct);
            _body_params->emplace_back(
                std::make_shared<http_param>(
                    mcf->name, mcf->value
                )
            );
        }
    }
}




}//::evmvc
