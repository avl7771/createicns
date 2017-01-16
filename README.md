# createicns

## Use

This is a generator for the Apple Icon Image Format (.icns) which reads PNGs
without changing them.

It's run like this: `createicons x.iconset` and outputs a file `x.icns`.

The input is a .iconset directory with files conforming to the naming scheme
for .iconset directories. It reads a 'complete' set of PNG icons as described
here:
<https://developer.apple.com/library/content/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/Optimizing/Optimizing.html>

To generate a .iconset directory from an existing x.icns file, use
`iconutil -c iconset x.icns`

This tool is similar to running `iconutil -c icns x.iconset`, except it
doesn't change the PNG images in any way.

## Installation

`createicns` has only been tested on macOS. Use `make` to compile it.
