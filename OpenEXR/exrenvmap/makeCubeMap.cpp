///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
//
//      function makeCubeMap() -- makes cube-face environment maps
//
//-----------------------------------------------------------------------------

#include <makeCubeMap.h>

#include <resizeImage.h>
#include <ImfRgbaFile.h>
#include <ImfTiledRgbaFile.h>
#include <ImfStandardAttributes.h>
#include "Iex.h"
#include <iostream>
#include <algorithm>
#include <string>
#include <string.h>


#include "namespaceAlias.h"
using namespace CustomImf;
using namespace std;
using namespace IMATH_NAMESPACE;

namespace {

void
makeCubeMapSingleFile (EnvmapImage &image1,
                       Header &header,
                       RgbaChannels channels,
                       const char outFileName[],
                       int tileWidth,
                       int tileHeight,
                       LevelMode levelMode,
                       LevelRoundingMode roundingMode,
                       Compression compression,
                       int mapWidth,
                       float filterRadius,
                       int numSamples,
                       bool verbose)
{
    if (levelMode == RIPMAP_LEVELS)
        throw IEX_NAMESPACE::NoImplExc ("Cannot generate ripmap cube-face environments.");

    //
    // Open the file that will contain the cube-face map,
    // and write the header.
    //

    int mapHeight = mapWidth * 6;

    header.dataWindow() = Box2i (V2i (0, 0), V2i (mapWidth - 1, mapHeight - 1));
    header.displayWindow() = header.dataWindow();
    header.compression() = compression;

    addEnvmap (header, ENVMAP_CUBE);

    TiledRgbaOutputFile out (outFileName,
                             header,
                             channels,
                             tileWidth, tileHeight,
                             levelMode,
                             roundingMode);
    if (verbose)
        cout << "writing file " << outFileName << endl;

    //
    // Generate the pixels for the various levels of the cube-face map,
    // and store them in the file.  The pixels for the highest-resolution
    // level are generated by resampling the original input image; for
    // each of the other levels, the pixels are generated by resampling
    // the previous level.
    //

    EnvmapImage image2;
    EnvmapImage *iptr1 = &image1;
    EnvmapImage *iptr2 = &image2;
    
    for (int level = 0; level < out.numLevels(); ++level)
    {
        if (verbose)
            cout << "level " << level << endl;

        Box2i dw = out.dataWindowForLevel (level);
        resizeCube (*iptr1, *iptr2, dw, filterRadius, numSamples);

        out.setFrameBuffer (&iptr2->pixels()[0][0], 1, dw.max.x + 1);

        for (int tileY = 0; tileY < out.numYTiles (level); ++tileY)
            for (int tileX = 0; tileX < out.numXTiles (level); ++tileX)
                out.writeTile (tileX, tileY, level);

        swap (iptr1, iptr2);
    }

    if (verbose)
        cout << "done." << endl;
}


void
makeCubeMapSixFiles (EnvmapImage &image1,
                     Header &header,
                     RgbaChannels channels,
                     const char outFileName[],
                     int tileWidth,
                     int tileHeight,
                     Compression compression,
                     int mapWidth,
                     float filterRadius,
                     int numSamples,
                     bool verbose)
{
    static const char *faceNames[] =
        {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};

    size_t pos = strchr (outFileName, '%') - outFileName;

    int mapHeight = mapWidth * 6;
    const Box2i dw (V2i (0, 0), V2i (mapWidth - 1, mapHeight - 1));
    const Box2i faceDw (V2i (0, 0), V2i (mapWidth - 1, mapWidth - 1));

    EnvmapImage image2;
    resizeCube (image1, image2, dw, filterRadius, numSamples);
    const Rgba *pixels = &(image2.pixels())[0][0];

    for (int i = 0; i < 6; ++i)
    {
        string name = string (outFileName).replace (pos, 1, faceNames[i]);

        if (verbose)
            cout << "writing file " << name << endl;

        TiledRgbaOutputFile out (name.c_str(),
                                 tileWidth, tileHeight,
                                 ONE_LEVEL, ROUND_DOWN,
                                 faceDw,        // displayWindow
                                 faceDw,        // dataWindow
                                 channels,
                                 1,             // pixelAspectRatio
                                 V2f (0, 0),    // screenWindowCenter
                                 1,             // screenWindowWidth
                                 INCREASING_Y,  // lineOrder
                                 compression);

        out.setFrameBuffer (pixels, 1, dw.max.x + 1);

        for (int tileY = 0; tileY < out.numYTiles(); ++tileY)
            for (int tileX = 0; tileX < out.numXTiles(); ++tileX)
                    out.writeTile (tileX, tileY);

        pixels += mapWidth * mapWidth;
    }

    if (verbose)
        cout << "done." << endl;
}

} //namespace


void
makeCubeMap (EnvmapImage &image1,
             Header &header,
             RgbaChannels channels,
             const char outFileName[],
             int tileWidth,
             int tileHeight,
             LevelMode levelMode,
             LevelRoundingMode roundingMode,
             Compression compression,
             int mapWidth,
             float filterRadius,
             int numSamples,
             bool verbose)
{
    if (strchr (outFileName, '%'))
    {
        makeCubeMapSixFiles (image1,
                             header,
                             channels,
                             outFileName,
                             tileWidth,
                             tileHeight,
                             compression,
                             mapWidth,
                             filterRadius,
                             numSamples,
                             verbose);
    }
    else
    {
        makeCubeMapSingleFile (image1,
                               header,
                               channels,
                               outFileName,
                               tileWidth,
                               tileHeight,
                               levelMode,
                               roundingMode,
                               compression,
                               mapWidth,
                               filterRadius,
                               numSamples,
                               verbose);
    }
}
