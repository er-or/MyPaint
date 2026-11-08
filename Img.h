// Img.h
//
// A picture held as four planes, red, green, blue and alpha, one byte per pixel
// each, in the spirit of Img.js. Everything here is plain loops over those
// planes, meant to be read and edited by hand rather than to be clever.
//
// MyPaint uses it for layer blend modes. Thresholding, and tracing the shapes
// that come out of it into polygons, are meant to join it later.
//
// Nothing here knows about Windows or GDI+: pixels come in and go out as plain
// 32-bit 0xAARRGGBB values (alpha not premultiplied), which is what GDI+ hands
// over from Bitmap::LockBits with PixelFormat32bppARGB.

#pragma once
#include <cstdint>
#include <vector>

// The blend modes of SVG, CSS, Inkscape and OpenRaster, in the order Inkscape
// lists them. Each says how a layer's colour (the source) mixes with the colour
// already beneath it (the backdrop). Normal just covers it.
enum class BlendMode {
    Normal,
    Darken, Multiply, ColorBurn,              // darker: white makes no change
    Lighten, Screen, ColorDodge,              // lighter: black makes no change
    Overlay, SoftLight, HardLight,            // contrast: middle grey makes no change
    Difference, Exclusion,                    // comparing the two
    Hue, Saturation, Color, Luminosity,       // one quality of the colour from each
    Count
};

class Img {
public:
    int w = 0, h = 0;
    std::vector<uint8_t> r, g, b, a;   // row after row: pixel (x, y) is at index y * w + x

    Img() = default;
    Img(int width, int height);

    int width() const  { return w; }
    int height() const { return h; }
    int at(int x, int y) const { return y * w + x; }

    // ── I/O ──────────────────────────────────────────────────────────────
    // stride is the number of bytes from one row to the next, as LockBits gives it
    static Img fromArgb(const uint32_t* pixels, int width, int height, int strideBytes);
    void toArgb(uint32_t* pixels, int strideBytes) const;

    // ── Blending ─────────────────────────────────────────────────────────
    static const wchar_t* blendName(BlendMode m);   // as shown to people: "Color Dodge"
    static const char* blendKey(BlendMode m);       // as SVG and .ora write it: "color-dodge"

    // Draws top over this picture with the mode, at an extra opacity of 0 to 1.
    // Both must be the same size.
    void blend(const Img& top, BlendMode mode, float opacity);

    // The mode's formula for one channel, backdrop cb and source cs, all 0 to 1.
    // Only for the modes that treat red, green and blue separately (Normal to Exclusion).
    static float blendChannel(BlendMode m, float cb, float cs);

    // Hue, Saturation, Color and Luminosity work on the whole colour at once
    static void blendColor(BlendMode m, const float cb[3], const float cs[3], float out[3]);

private:
    // Helpers for the whole-colour modes, as the W3C compositing spec names them
    static float lum(const float c[3]);
    static void  clipColor(float c[3]);
    static void  setLum(const float c[3], float l, float out[3]);
    static float sat(const float c[3]);
    static void  setSat(const float c[3], float s, float out[3]);
};
