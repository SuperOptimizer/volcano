# volcano
volcano is a simple c library for working with the vesuvius project. volcano aims to be a one stop shop for many common small tasks. Support is coming for using volcano as glue between many of the different steps in the full solve pipeline

volcano is composed of many "mini libraries", themselves single header libraries that handle one specific task.

These mini libraries include
- minichamfer, for calculating the chamfer distance between two point clouds
- minicurl, for an easy API to download from the data server
- minihistogram, for generating a histogram csv from a 2d slice or 3d chunk
- minimath, for working with 2d and 3d float data and doing some common preprocessing work
- minimesh, for generating a mesh from a 3d volume. Currently marching cubes is the only supported method
- mininrrd, for reading nrrd files
- miniobj, for reading and writing .obj files
- miniply, for reading and writing .ply files
- minippm, for reading and writing .ppm files
- minisnic, for running SNIC superpixel segmentation on a 3d volume (thanks @spelufo!)
- minitiff, for reading 2d and 3d tiff files
- minivcps, for reading vcps and vcano files
- minivol, for working with large on disk volumes
- minizarr, for reading and writing zarr volumes

volcano has a minimal set of 3rd party dependencies
- BearSSL, to allow for curl to use SSL
- cblosc2, for compression and decompression of zarr volumes
- curl, for downloading from the data server
- json.h, for parsing json
- stb, for various small tasks
- miniz, for other compression needs

volcano can be used in 3 ways:
- as a single header library, where a user does `#include "volcano.h"` to utilize volcano and all of its mini libraries as inline code in the header
- as a static library, where the user can statically link against a .a or .lib files
- as a shared library, where the user can dynamically link against .dll or .so files
- coming soon! python bindings

