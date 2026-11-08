// Img.h
//
// A picture held as four planes, red, green, blue and alpha, one byte per pixel
// each, in the spirit of Img.js. Everything here is plain loops over those
// planes, meant to be read and edited by hand rather than to be clever.
//
// MyPaint uses it for layer blend modes (Img.cpp), the Adjust menu
// (ImgAdjust.cpp) and the Filters menu (ImgFilter.cpp). Tracing the shapes
// that come out of threshold() into polygons is meant to join it later.
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

// A convolution kernel: a square grid of weights, an odd number on each side,
// laid over every pixel in turn (the middle weight on the pixel itself). The
// weighted sum is divided by the divisor, then the offset is added.
//
//   new value = (sum of weight * neighbour) / divisor + offset
//
// Presets come in any odd size; a person can then change any weight.
struct Kernel {
    int size = 3;                 // 3, 5, 7 ... weights on each side
    bool filterAlpha = true;      // filter alpha too (SVG's default), or leave it (preserveAlpha="true")
    std::vector<float> v;         // row after row: weight (x, y) is v[y * size + x]
    float divisor = 1;            // 0 is allowed: convolve uses a tiny number instead
    float offset  = 0;           // 128 lifts results that hover around 0 (emboss, edges) to grey

    Kernel() : v(9, 0.0f) { v[4] = 1; }
    explicit Kernel(int n) : size(n), v(n * n, 0.0f) {}
    float& at(int x, int y)       { return v[y * size + x]; }
    float  at(int x, int y) const { return v[y * size + x]; }
    float  sum() const;
    float  autoDivisor() const;   // the sum, or 1 when the weights add up to 0

    // Presets. Each is size x size with its divisor set. Edges and emboss
    // leave alpha alone, since filtering it would make flat areas see-through.
    static Kernel identity(int size);    // changes nothing: 1 in the middle
    static Kernel box(int size);         // plain average: soft blur
    static Kernel gaussian(int size);    // weighted average, heavier in the middle: smooth blur
    static Kernel sharpen(int size);     // twice the pixel minus the average around it
    static Kernel edges(int size);       // the pixel minus the ones around it: outlines on black
    static Kernel emboss(int size);      // top left against bottom right, on grey
    static Kernel motion(int size);      // average along a horizontal line
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

    // ── Adjustments (ImgAdjust.cpp) ──────────────────────────────────────
    // Each pixel changes on its own, from its own colour only. Alpha is left
    // as it is. All return *this so they can be chained.
    Img& applyLut(const uint8_t lut[256]);              // the same lookup table for red, green and blue
    Img& invert();                                      // 255 - value
    Img& gray();                                        // the colour's brightness, as the eye sees it
    Img& sepia();                                       // gray, tinted brown like an old photo
    Img& brightnessContrast(int brightness, int contrast);           // both -100 to 100
    Img& hueSaturation(int hueDegrees, int saturation, int lightness); // -180..180, -100..100, -100..100
    Img& levels(int inBlack, int inWhite, float gamma, int outBlack, int outWhite);
    Img& threshold(int level);                          // black below level, white from it up
    Img& posterize(int levelsPerChannel);               // 2 to 255 values per channel
    Img& autoLevel();                                   // stretch the darkest to black, the lightest to white
    Img& equalize();                                    // spread the values evenly (histogram equalisation)

    // Histogram of red, green and blue together, counting only pixels that show
    static void histogram(const Img& img, double counts[256]);

    // ── Filters (ImgFilter.cpp) ──────────────────────────────────────────
    // Each new pixel depends on its neighbours. Pixels past the edge count as
    // copies of the nearest edge pixel.
    Img& convolve(const Kernel& k);
    Img& addNoise(int amount, unsigned seed);           // amount 0 to 100
    Img& pixelate(int cellSize);                        // squares of cellSize x cellSize, each one colour

private:
    // Helpers for the whole-colour modes, as the W3C compositing spec names them
    static float lum(const float c[3]);
    static void  clipColor(float c[3]);
    static void  setLum(const float c[3], float l, float out[3]);
    static float sat(const float c[3]);
    static void  setSat(const float c[3], float s, float out[3]);
};
