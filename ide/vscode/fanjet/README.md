
### resources:

*textmate ref*

* https://macromates.com/manual/en/language_grammars
* https://macromates.com/manual/en/regular_expressions
* https://macromates.com/textmate/manual/references

*vscode ref*

* https://code.visualstudio.com/api/working-with-extensions/publishing-extension
* https://github.com/Microsoft/vscode/tree/master/extensions


## vsce

[vsce](https://github.com/Microsoft/vsce), short for "Visual Studio Code Extensions", is a command line tool for packaging, publishing and managing VS Code extensions.

### Installation

Make sure you have [Node.js](https://nodejs.org/) installed. Then run:

```bash
npm install -g vsce
```
### Usage

You can use `vsce` to easily [package](#packaging-extensions) and [publish](#publishing-extensions) your extensions:

```bash
$ cd myExtension
$ vsce package
# myExtension.vsix generated
$ vsce publish
# <publisherID>.myExtension published to VS Code MarketPlace
```

`vsce` can also search, get metadata of and unpublish extensions. For a reference on all the available `vsce` commands, run `vsce --help`.
