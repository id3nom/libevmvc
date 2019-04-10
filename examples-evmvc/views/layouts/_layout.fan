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
@name   _layout

<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <title>@get("title", "web-server example")</title>

    <link rel="stylesheet" href="/html/css/uikit.min.css" />
    <link rel="stylesheet" href="/html/css/app.css" />
    
    <script src="/html/scripts/jquery-3.3.1.min.js"></script>
    <script src="/html/scripts/markdown.js"></script>
    
    <script src="https://cdn.jsdelivr.net/npm/latex.js@0.11.1/dist/latex.min.js"></script>
  
    
    <script>
    "use strict";
    $(() => {
        var mds = document.querySelectorAll("[data-markup=\"md\"]");
        mds.forEach((v, i) => {
            v.innerHTML = markdown.toHTML(unescape(v.innerHTML));
        });
        
        var texs = document.querySelectorAll("[data-markup=\"tex\"]");
        texs.forEach((v, i) => {
            try{
                var texgen = new latexjs.HtmlGenerator({hyphenate: false});
                texgen = latexjs.parse(
                    unescape(v.innerHTML),
                    {generator: texgen}
                );
                
                document.body.appendChild(
                    texgen.stylesAndScripts(
                        "https://cdn.jsdelivr.net/npm/latex.js@0.11.1/dist/"
                    )
                );
                v.innerHTML = "";
                v.appendChild(texgen.domFragment());
                
            }catch(err){
                v.innerHTML = err.message;
            }
        });
    });
    </script>
</head>
<body>
    
    @body
    
    <br/>
    <br/>
    <br/>
    
    @>examples::user_info;<br/>

    
</body>
</html>

