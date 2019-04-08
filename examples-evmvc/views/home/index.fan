@*
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
*@
@ns     
@name   
@layout _layout
@inherits{
    protected @utils = helpers/utils
}

@set("title", "index - web-server example")

In index.fan page <br/>

@> req->params("test");

@utils();
@{
    @utils::util_func01();
}

<a href="/test">test</a><br/>
<a href="/html/login.html">login</a><br/>
<a href="/download-file/">/download-file/:[filename]</a><br/>
<a href="/echo/blabla">/echo/:val</a><br/>
<a href="/send-json">/send-json</a><br/>
<a href="/send-file?path=xyz">/send-file?path=xyz</a><br/>
<a href="/cookies/set/">/cookies/set/:[name]/:[val]/:[path]</a><br/>
<a href="/cookies/get/">/cookies/get/:[name]</a><br/>
<a href="/cookies/clear/">/cookies/clear/:[name]/:[path]</a><br/>
<a href="/error">/error</a><br/>
<a href="/exit">/exit</a><br/>
<a href="/set_timeout/">/set_timeout/:[timeout(\\d+)]</a><br/>
<a href="/set_interval/">/set_interval/:[name]/:[interval(\\d+)]</a><br/>
<a href="/clear_interval/">/clear_interval/:[name]</a><br/>


@header{
    int j;
    
}