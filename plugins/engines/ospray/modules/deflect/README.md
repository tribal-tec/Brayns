The Deflect Pixel Op Module for OSPRay
======================================

This module implements a
[pixel op](https://github.com/ospray/OSPRay#pixel-operation) for OSPRay that
streams each rendered tile to a [Deflect](https://github.com/BlueBrain/Deflect)
server like [Tide](https://github.com/BlueBrain/Tide). The streamed tiles can be
compressed with a variable JPEG quality.

Building
--------

```
git clone --recursive https://github.com/BlueBrain/DeflectPixelOp
cd DeflectPixelOp
mkdir build
cd build
cmake .. -GNinja
```

Usage
-----

- Point LD_LIBRARY_PATH to the folder which contains
  'libospray_module_deflect.so'
- Run OSPRay application either with command line '--module deflect' or do
  'ospLoadModule("deflect")' programmatically
- Create and attach the pixel op to the framebuffer
```
OSPPixelOp pixelop = ospNewPixelOp("DeflectPixelOp");
ospSetPixelOp(frameBuffer, pixelOp);
```
- To update settings for the streaming, you can commit the pixel op like this
```
ospSet1i(pixelop, "compression", 1); // 1 is on, 0 is off
ospSet1i(pixelop, "quality", 42); // from 0 (worst) to 100 (best) quality
ospCommit(pixelop);
```
