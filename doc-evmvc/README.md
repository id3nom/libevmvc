[TOC]

# About libevmvc

## Routing

* simple route that will match url "/abc/123" and "/abc/123/"
<br/>
/abc/123

* simple route that will match url's "/abc/123", "/abc/123/def", "/abc/123/456/" but not "/abc/123/def/other_path"
<br/>
/abc/123/*

* simple route that will match url's "/abc/123", "/abc/123/def", "/abc/123/456/" and any sub path "/abc/123/def/sub/path/..."
<br/>
/abc/123/**

* optional parameter can be defined by enclosing the parameter in square brackets
<br/>
/abc/123/:p1/:[p2]

* PCRE regex can be use to validate parameter by enclosing the rules inside parentheses following the parameter name
<br/>
/abc/123/:p1(\\d+)/:[p2]

* regex parameter can be optional as well
<br/>
/abc/123/:[p1(\\d+)]

* all parameters following an optional parameter must be optional
<br/>
/abc/123/:p1(\\d+)/:[p2]/:[p3]


## Fanjet language

### Fanjet engine directory structure

~~~
    .../views/
        ├─ layouts/ (default layout dir)
        │   ├─ *.fan (Fanjet root layout files, default layout is named '_layout.fan')
        │   ├─ subdir_a/
        │   │   └─ *.fan (Fanjet root layouts files)
        │   │   
        │   └─ subdir_b/
        │       └─ *.fan (Fanjet root layouts files)
        │       
        ├─ partials/ (partials can be rendered with the @>view/path/to/render; directive)
        │   └─ *.fan (Fanjet root partials files)
        │
        ├─ helpers/ (optional helper root directory)
        │   └─ *.fan (Fanjet root helper files)
        │
        ├─ usr_ns_a/ (user defined namespace)
        │   ├─ layouts/ (optional layouts dir)
        │   │   └─ *.fan (Fanjet 'usr_ns_a/' layout files, default layout is named '_layout.fan')
        │   │   
        │   ├─ partials/
        │   │   └─ *.fan (Fanjet 'usr_ns_a/' partials files)
        │   │
        │   ├─ helpers/
        │   │   └─ *.fan (Fanjet 'usr_ns_a/' helpers files)
        │   │
        │   └─ *.fan (Fanjet 'usr_ns_a/*' files)
        │
        ├─ usr_ns_b/ ... (same as '/usr_ns_a')
        │
        └─ *.fan (Fanjet root files)
        
~~~

### layouts directory

Files in the layouts directory are template used to render enclosing html
to render the body use the render body directive '@>>;', this will asynchrounously render the view

### partials directory

partials view can be rendered at any time using the render partial directive '@>...;'
the user must replace the '...' value with the partial view name to render.
the directive must be ended with a semicolon.

### helpers directory

Files in the helpers directory are automatically inherited by all file in the same directory
or in any subdirectory of the hierarchy.

### search paths

The search for a specific view is always done from current directory to top directory
e.g.: the user whant to render the following partial view '@>user_info;'
first the 'user_info' view is search in the current view 'partials' directory './partials/user_info'
if the view is not found than the parent directory will be probe '../partials/user_info' and so on,
until the view is found or the views 'root' directory is reached.

### Fanjet directives

| Start tag     | End tag   | comment |
| ---------     | --------- | ------- |
| @@            |           | escape sequence, will be processed as the '@' char. |
| @{            | }         | cpp code block. |
| @**           | \n        | line comment |
| @*            | *@        | multiline comment |
| @namespace    | \n        | view namespace to use the directory structure let the directive empty |
| @ns           | \n        | view namespace to use the directory structure let the directive empty |
| @name         | \n        | view name to use the filename without it's extension let the directive empty |
| @layout       | \n        | layout used with that view, to use the default layout let the directive empty |
| @header       | ;         | view header definition |
| @inherits     | ;         | view inherits override settings |
    
    "@region",
    "@endregion",
    
    "@::", "@:",
    
    "@if", "else", "if",
    "@switch",
    
    "@while",
    "@for",
    "@do",
    "@try", "catch", "try",
    
    "@funi", "@func",
    "@<", "@await",
    
    "@(",
    "@>",
