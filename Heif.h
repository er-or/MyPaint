// Heif.h
//
// Reads the layers out of HEIF files whose pictures are stored uncompressed
// (ISO/IEC 23001-17, item type 'unci'), which is how Windows Paint saves its
// .paint files. Plain code, meant to be read and edited by hand, like Img and Zip.
//
// A HEIF file is a tree of "boxes" (a size, a four-letter type, then contents).
// Inside 'meta' it lists items, where each item's bytes are ('iloc'), and each
// item's properties ('iprp'), such as its size and how its pixels are laid out.
// Paint's files hold one 'unci' item per layer, and an overlay item ('iovl'),
// the file's main picture, that stacks the layers on a canvas.
//
// What a .paint file holds, from the one sample studied: five layers of
// 8-bit red, green, blue and alpha, deflate-compressed, an overlay putting
// them all at 0, 0 on a white canvas, and Exif with the resolution. No layer
// names, opacity, visibility or blend modes were found in it.
//
// Not handled: HEVC/AV1-coded pictures (photos), tiles, sample formats other
// than 8-bit unsigned integers, and grids. Those return false.
//
// Specs: ISO/IEC 14496-12 (boxes), 23008-12 (HEIF), 23001-17 (uncompressed).

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Heif {

struct Layer {
    int x = 0, y = 0;            // where it sits on the canvas
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;   // w * h pixels of red, green, blue, alpha
};

struct Picture {
    int w = 0, h = 0;                 // the canvas
    uint8_t fill[4] = { 0, 0, 0, 0 }; // the canvas colour under the layers (red, green, blue, alpha)
    std::vector<Layer> layers;        // bottom first
};

// Reads a whole file held in memory. False if it isn't one this can read;
// error then says why.
bool readLayers(const std::vector<uint8_t>& file, Picture& out, std::string* error = nullptr);

}   // namespace Heif
