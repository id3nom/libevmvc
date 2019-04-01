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
@ns     test
@name   test03
@layout _layout

@header{
    
}

@set("title", "test03 - test")
@set("hr", "<hr/>")

@fmt(
    "test string: '{}'",
    "testing encoded format",
    (int)23, (std::string)val
)

<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="/html/css/uikit.min.css" />
    <link rel="stylesheet" href="/html/css/app.css" />
    
    <title>@get("title", "some default value")</title>
</head>
<body>
    @fmt-raw(
        "<a href=\"{}\">hyperlink text {}</a>",
        "/index",
        (long)23
    )
    @::(long)22 ;
    <a href="@::index_val;"> hyperlink text @:(long)23;  </a>
    @:(long)23;
    @get-raw("hr", "<br/>")
    
    "testing @:test;" <div>@:(int)23;</div>@:(int)23;
    
    @>header;
    
    
    @body

    @>footer;
</body>
