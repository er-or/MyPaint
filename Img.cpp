// Img.cpp
//
// See Img.h. The blend formulas follow the W3C "Compositing and Blending
// Level 1" spec, which is what SVG, CSS, Inkscape and OpenRaster all use:
// https://www.w3.org/TR/compositing-1/

#include "Img.h"
#include <algorithm>
#include <cmath>
#include <cstddef>


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Construction and I/O
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


/**
 * A width x height picture, every plane zero (transparent black).
 */
Img::Img(int width, int height)
    : w(width), h(height),
      r(width * height), g(width * height), b(width * height), a(width * height)
{
}


/**
 * Splits 0xAARRGGBB pixels into the four planes.
 */
Img Img::fromArgb(const uint32_t* pixels, int width, int height, int strideBytes)
{
    Img img(width, height);
    for (int y = 0; y < height; y++) {
        const uint32_t* row = (const uint32_t*)((const uint8_t*)pixels + (std::ptrdiff_t)y * strideBytes);
        for (int x = 0; x < width; x++) {
            uint32_t p = row[x];
            int i = img.at(x, y);
            img.a[i] = (uint8_t)(p >> 24);
            img.r[i] = (uint8_t)(p >> 16);
            img.g[i] = (uint8_t)(p >> 8);
            img.b[i] = (uint8_t)p;
        }
    }
    return img;
}


/**
 * Puts the four planes back together as 0xAARRGGBB pixels.
 */
void Img::toArgb(uint32_t* pixels, int strideBytes) const
{
    for (int y = 0; y < h; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)pixels + (std::ptrdiff_t)y * strideBytes);
        for (int x = 0; x < w; x++) {
            int i = at(x, y);
            row[x] = ((uint32_t)a[i] << 24) | ((uint32_t)r[i] << 16) | ((uint32_t)g[i] << 8) | b[i];
        }
    }
}


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Blending
//
// For every pixel, with the backdrop's colour Cb and alpha ab, and the
// source's colour Cs and alpha as (times the layer's opacity):
//
//   1. B = the mode's mix of Cb and Cs. Normal gives back Cs.
//   2. Where the backdrop is see-through, the source shows as it is:
//        Cs' = (1 - ab) * Cs + ab * B
//   3. Cs' then goes over the backdrop the ordinary way ("source over"):
//        ao = as + ab * (1 - as)
//        Co = (as * Cs' + (1 - as) * ab * Cb) / ao
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


const wchar_t* Img::blendName(BlendMode m)
{
    switch (m) {
    case BlendMode::Normal:     return L"Normal";
    case BlendMode::Darken:     return L"Darken";
    case BlendMode::Multiply:   return L"Multiply";
    case BlendMode::ColorBurn:  return L"Color Burn";
    case BlendMode::Lighten:    return L"Lighten";
    case BlendMode::Screen:     return L"Screen";
    case BlendMode::ColorDodge: return L"Color Dodge";
    case BlendMode::Overlay:    return L"Overlay";
    case BlendMode::SoftLight:  return L"Soft Light";
    case BlendMode::HardLight:  return L"Hard Light";
    case BlendMode::Difference: return L"Difference";
    case BlendMode::Exclusion:  return L"Exclusion";
    case BlendMode::Hue:        return L"Hue";
    case BlendMode::Saturation: return L"Saturation";
    case BlendMode::Color:      return L"Color";
    case BlendMode::Luminosity: return L"Luminosity";
    default:                    return L"Normal";
    }
}


const char* Img::blendKey(BlendMode m)
{
    switch (m) {
    case BlendMode::Normal:     return "normal";
    case BlendMode::Darken:     return "darken";
    case BlendMode::Multiply:   return "multiply";
    case BlendMode::ColorBurn:  return "color-burn";
    case BlendMode::Lighten:    return "lighten";
    case BlendMode::Screen:     return "screen";
    case BlendMode::ColorDodge: return "color-dodge";
    case BlendMode::Overlay:    return "overlay";
    case BlendMode::SoftLight:  return "soft-light";
    case BlendMode::HardLight:  return "hard-light";
    case BlendMode::Difference: return "difference";
    case BlendMode::Exclusion:  return "exclusion";
    case BlendMode::Hue:        return "hue";
    case BlendMode::Saturation: return "saturation";
    case BlendMode::Color:      return "color";
    case BlendMode::Luminosity: return "luminosity";
    default:                    return "normal";
    }
}


/**
 * Draws top over this picture with the blend mode, at an extra opacity.
 */
void Img::blend(const Img& top, BlendMode mode, float opacity)
{
    if (top.w != w || top.h != h) return;
    bool wholeColor = mode >= BlendMode::Hue;
    for (int i = 0; i < w * h; i++) {
        float as = top.a[i] / 255.f * opacity;
        if (as <= 0.f) continue;   // nothing of the source here
        float ab = a[i] / 255.f;
        float cb[3] = { r[i] / 255.f, g[i] / 255.f, b[i] / 255.f };
        float cs[3] = { top.r[i] / 255.f, top.g[i] / 255.f, top.b[i] / 255.f };

        // 1. The mode's mix
        float mix[3];
        if (wholeColor) {
            blendColor(mode, cb, cs, mix);
        } else {
            for (int c = 0; c < 3; c++) mix[c] = blendChannel(mode, cb[c], cs[c]);
        }

        // 2 and 3. Mix only where there is a backdrop, then source over
        float ao = as + ab * (1.f - as);
        float out[3];
        for (int c = 0; c < 3; c++) {
            float shown = (1.f - ab) * cs[c] + ab * mix[c];
            out[c] = (as * shown + (1.f - as) * ab * cb[c]) / ao;
        }
        r[i] = (uint8_t)std::lround(std::clamp(out[0], 0.f, 1.f) * 255.f);
        g[i] = (uint8_t)std::lround(std::clamp(out[1], 0.f, 1.f) * 255.f);
        b[i] = (uint8_t)std::lround(std::clamp(out[2], 0.f, 1.f) * 255.f);
        a[i] = (uint8_t)std::lround(std::clamp(ao, 0.f, 1.f) * 255.f);
    }
}


/**
 * One channel of the modes that work on red, green and blue separately.
 * cb is the backdrop, cs the source, both 0 to 1.
 */
float Img::blendChannel(BlendMode m, float cb, float cs)
{
    switch (m) {
    case BlendMode::Darken:      // whichever is darker
        return std::min(cb, cs);

    case BlendMode::Multiply:    // like stacking coloured glass: white leaves it, black makes black
        return cb * cs;

    case BlendMode::ColorBurn:   // darkens the backdrop to show the source, with more contrast
        if (cb >= 1.f) return 1.f;
        if (cs <= 0.f) return 0.f;
        return 1.f - std::min(1.f, (1.f - cb) / cs);

    case BlendMode::Lighten:     // whichever is lighter
        return std::max(cb, cs);

    case BlendMode::Screen:      // the opposite of Multiply: black leaves it, white makes white
        return cb + cs - cb * cs;

    case BlendMode::ColorDodge:  // brightens the backdrop to show the source
        if (cb <= 0.f) return 0.f;
        if (cs >= 1.f) return 1.f;
        return std::min(1.f, cb / (1.f - cs));

    case BlendMode::Overlay:     // Hard Light with the two swapped: the backdrop decides
        return blendChannel(BlendMode::HardLight, cs, cb);

    case BlendMode::HardLight:   // Multiply where the source is dark, Screen where it is light
        if (cs <= 0.5f) return cb * (2.f * cs);
        return blendChannel(BlendMode::Screen, cb, 2.f * cs - 1.f);

    case BlendMode::SoftLight: { // a gentle Hard Light, like a diffused spotlight
        if (cs <= 0.5f) return cb - (1.f - 2.f * cs) * cb * (1.f - cb);
        float d = cb <= 0.25f ? ((16.f * cb - 12.f) * cb + 4.f) * cb : std::sqrt(cb);
        return cb + (2.f * cs - 1.f) * (d - cb);
    }

    case BlendMode::Difference:  // how far apart they are: the same colours make black
        return std::fabs(cb - cs);

    case BlendMode::Exclusion:   // like Difference, with less contrast
        return cb + cs - 2.f * cb * cs;

    default:                     // Normal
        return cs;
    }
}


/**
 * The modes that work on the whole colour: each keeps some qualities of the
 * backdrop and takes the others from the source.
 */
void Img::blendColor(BlendMode m, const float cb[3], const float cs[3], float out[3])
{
    float t[3];
    switch (m) {
    case BlendMode::Hue:          // the source's hue, the backdrop's saturation and lightness
        setSat(cs, sat(cb), t);
        setLum(t, lum(cb), out);
        break;
    case BlendMode::Saturation:   // the source's saturation, the backdrop's hue and lightness
        setSat(cb, sat(cs), t);
        setLum(t, lum(cb), out);
        break;
    case BlendMode::Color:        // the source's hue and saturation, the backdrop's lightness
        setLum(cs, lum(cb), out);
        break;
    case BlendMode::Luminosity:   // the source's lightness, the backdrop's hue and saturation
        setLum(cb, lum(cs), out);
        break;
    default:
        out[0] = cs[0]; out[1] = cs[1]; out[2] = cs[2];
        break;
    }
}


// How light a colour looks: green counts most, blue least
float Img::lum(const float c[3])
{
    return 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2];
}


// Brings a colour back between 0 and 1 while keeping its lightness
void Img::clipColor(float c[3])
{
    float l = lum(c);
    float n = std::min({ c[0], c[1], c[2] });
    float x = std::max({ c[0], c[1], c[2] });
    for (int i = 0; i < 3; i++) {
        if (n < 0.f && l - n > 0.f) c[i] = l + (c[i] - l) * l / (l - n);
        if (x > 1.f && x - l > 0.f) c[i] = l + (c[i] - l) * (1.f - l) / (x - l);
    }
}


// The colour c made as light as l
void Img::setLum(const float c[3], float l, float out[3])
{
    float d = l - lum(c);
    for (int i = 0; i < 3; i++) out[i] = c[i] + d;
    clipColor(out);
}


// How colourful a colour is: the gap between its largest and smallest channel
float Img::sat(const float c[3])
{
    return std::max({ c[0], c[1], c[2] }) - std::min({ c[0], c[1], c[2] });
}


// The colour c with its saturation changed to s, keeping its hue
void Img::setSat(const float c[3], float s, float out[3])
{
    // Find which channel is largest, middle and smallest
    int mx = 0, mn = 0;
    for (int i = 1; i < 3; i++) {
        if (c[i] > c[mx]) mx = i;
        if (c[i] < c[mn]) mn = i;
    }
    if (mx == mn) { out[0] = out[1] = out[2] = 0.f; return; }   // grey has no hue to keep
    int md = 3 - mx - mn;
    out[md] = (c[md] - c[mn]) * s / (c[mx] - c[mn]);
    out[mx] = s;
    out[mn] = 0.f;
}
