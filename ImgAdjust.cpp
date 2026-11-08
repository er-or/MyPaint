// ImgAdjust.cpp
//
// Adjustments: each pixel is changed from its own colour alone, with no
// regard for its neighbours ("point processing" in Img.js). Most of them
// build a lookup table first: what each of the 256 values becomes. Applying
// the table is then one look-up per channel, however involved the formula.
//
// Alpha is never touched here, so see-through parts stay see-through.

#include "Img.h"
#include <algorithm>
#include <cmath>


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Small helpers
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


// Rounds and keeps a value inside 0 to 255
static uint8_t clampByte(double v)
{
    if (v <= 0)   return 0;
    if (v >= 255) return 255;
    return (uint8_t)(v + 0.5);
}


/**
 * Swaps every red, green and blue value for lut[value].
 */
Img& Img::applyLut(const uint8_t lut[256])
{
    for (size_t i = 0; i < r.size(); i++) {
        r[i] = lut[r[i]];
        g[i] = lut[g[i]];
        b[i] = lut[b[i]];
    }
    return *this;
}


/**
 * Counts how many times each value 0 to 255 turns up in red, green and blue
 * together. Fully transparent pixels are left out: their colour can't be
 * seen, and is often plain black that would throw the counts off.
 */
void Img::histogram(const Img& img, double counts[256])
{
    for (int v = 0; v < 256; v++) counts[v] = 0;
    for (size_t i = 0; i < img.r.size(); i++) {
        if (img.a[i] == 0) continue;
        counts[img.r[i]]++;
        counts[img.g[i]]++;
        counts[img.b[i]]++;
    }
}


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Colour
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


/**
 * The negative: black becomes white, red becomes cyan.
 */
Img& Img::invert()
{
    uint8_t lut[256];
    for (int v = 0; v < 256; v++) lut[v] = (uint8_t)(255 - v);
    return applyLut(lut);
}


/**
 * Shades of gray. Img.js averages red, green and blue; this weighs them the
 * way the eye does (Rec. 601: green looks brightest, blue darkest), so a pure
 * blue sky doesn't come out as light as a pure green field.
 */
Img& Img::gray()
{
    for (size_t i = 0; i < r.size(); i++) {
        uint8_t y = clampByte(0.299 * r[i] + 0.587 * g[i] + 0.114 * b[i]);
        r[i] = g[i] = b[i] = y;
    }
    return *this;
}


/**
 * The brown tint of an old photograph: the commonly used sepia mix, each new
 * channel a weighted sum of the old three.
 */
Img& Img::sepia()
{
    for (size_t i = 0; i < r.size(); i++) {
        double R = r[i], G = g[i], B = b[i];
        r[i] = clampByte(0.393 * R + 0.769 * G + 0.189 * B);
        g[i] = clampByte(0.349 * R + 0.686 * G + 0.168 * B);
        b[i] = clampByte(0.272 * R + 0.534 * G + 0.131 * B);
    }
    return *this;
}


/**
 * Brightness moves every value up or down: +100 adds about half of white.
 * Contrast pushes values away from middle gray (positive) or pulls them
 * towards it (negative); +100 is nearly black and white, -100 flat gray.
 * The contrast slope is tan(45 deg * (1 + contrast / 100)), as GIMP did:
 * 1 at 0, flat at -100, upright at +100.
 */
Img& Img::brightnessContrast(int brightness, int contrast)
{
    const double pi = 3.14159265358979323846;
    double c = std::max(-100, std::min(contrast, 99)) / 100.0;   // +100 would be an infinite slope
    double slope = std::tan((c + 1) * pi / 4);
    double shift = brightness * 1.275;                            // -100..100 to about -128..128

    uint8_t lut[256];
    for (int v = 0; v < 256; v++)
        lut[v] = clampByte((v - 127.5) * slope + 127.5 + shift);
    return applyLut(lut);
}


// RGB (0 to 1) to hue (0 to 360), saturation and lightness (0 to 1), the HSL model
static void rgbToHsl(double R, double G, double B, double& H, double& S, double& L)
{
    double mx = std::max(R, std::max(G, B)), mn = std::min(R, std::min(G, B));
    L = (mx + mn) / 2;
    double d = mx - mn;
    if (d == 0) { H = 0; S = 0; return; }   // a gray: no hue
    S = L > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
    if      (mx == R) H = (G - B) / d + (G < B ? 6 : 0);
    else if (mx == G) H = (B - R) / d + 2;
    else              H = (R - G) / d + 4;
    H *= 60;
}

// One channel back from HSL: t is the hue shifted by a third of the circle for red and blue
static double hueToChannel(double p, double q, double t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0 / 6) return p + (q - p) * 6 * t;
    if (t < 1.0 / 2) return q;
    if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}

static void hslToRgb(double H, double S, double L, double& R, double& G, double& B)
{
    if (S == 0) { R = G = B = L; return; }
    double q = L < 0.5 ? L * (1 + S) : L + S - L * S;
    double p = 2 * L - q;
    double h = H / 360;
    R = hueToChannel(p, q, h + 1.0 / 3);
    G = hueToChannel(p, q, h);
    B = hueToChannel(p, q, h - 1.0 / 3);
}


/**
 * Turns the colour wheel by hueDegrees, scales saturation (-100 gray, +100
 * twice as vivid), and moves lightness towards white (+) or black (-).
 */
Img& Img::hueSaturation(int hueDegrees, int saturation, int lightness)
{
    double satScale = 1 + saturation / 100.0;
    double light = lightness / 100.0;
    for (size_t i = 0; i < r.size(); i++) {
        double H, S, L;
        rgbToHsl(r[i] / 255.0, g[i] / 255.0, b[i] / 255.0, H, S, L);

        H = std::fmod(H + hueDegrees + 360, 360);
        S = std::min(1.0, S * satScale);
        if (light > 0) L = L + (1 - L) * light;   // part of the way to white
        else           L = L * (1 + light);       // part of the way to black

        double R, G, B;
        hslToRgb(H, S, L, R, G, B);
        r[i] = clampByte(R * 255);
        g[i] = clampByte(G * 255);
        b[i] = clampByte(B * 255);
    }
    return *this;
}


/**
 * Levels, as in photo editors:
 *   values at or below inBlack become outBlack,
 *   values at or above inWhite become outWhite,
 *   the ones between are spread out evenly, then bent by gamma
 *   (above 1 lightens the middle tones, below 1 darkens them).
 */
Img& Img::levels(int inBlack, int inWhite, float gamma, int outBlack, int outWhite)
{
    if (inWhite <= inBlack) inWhite = inBlack + 1;
    if (gamma < 0.01f) gamma = 0.01f;

    uint8_t lut[256];
    for (int v = 0; v < 256; v++) {
        double t = (double)(v - inBlack) / (inWhite - inBlack);   // 0 at inBlack, 1 at inWhite
        t = std::max(0.0, std::min(1.0, t));
        t = std::pow(t, 1.0 / gamma);
        lut[v] = clampByte(outBlack + t * (outWhite - outBlack));
    }
    return applyLut(lut);
}


/**
 * Black and white only: a pixel whose average of red, green and blue is
 * level or more turns white, the rest black (Img.js thresh2). This is the
 * first step before tracing shapes into polygons.
 */
Img& Img::threshold(int level)
{
    for (size_t i = 0; i < r.size(); i++) {
        int sum = r[i] + g[i] + b[i];
        uint8_t v = sum >= level * 3 ? 255 : 0;
        r[i] = g[i] = b[i] = v;
    }
    return *this;
}


/**
 * Fewer colours: each channel keeps only levelsPerChannel evenly spaced
 * values, each value moving to the nearest of them. 2 gives the 8 colours of
 * black, white, red, green, blue, cyan, magenta and yellow.
 */
Img& Img::posterize(int levelsPerChannel)
{
    int n = std::max(2, std::min(levelsPerChannel, 255));
    double step = 255.0 / (n - 1);
    uint8_t lut[256];
    for (int v = 0; v < 256; v++)
        lut[v] = clampByte(std::round(v / step) * step);
    return applyLut(lut);
}


/**
 * Stretches the values so the darkest becomes 0 and the lightest 255
 * (Img.js dynamicLinearTransform). A tiny share at each end (0.1%) is
 * ignored, so one stray black or white speck doesn't stop it working.
 */
Img& Img::autoLevel()
{
    double counts[256];
    histogram(*this, counts);
    double total = 0;
    for (double c : counts) total += c;
    if (total == 0) return *this;

    double skip = total * 0.001, seen = 0;
    int lo = 0, hi = 255;
    for (lo = 0; lo < 255; lo++)   { seen += counts[lo]; if (seen > skip) break; }
    seen = 0;
    for (hi = 255; hi > 0; hi--)   { seen += counts[hi]; if (seen > skip) break; }
    if (hi <= lo) return *this;   // one flat colour: nothing to stretch

    return levels(lo, hi, 1.0f, 0, 255);
}


/**
 * Histogram equalisation (Img.js unahe): each value moves to its rank among
 * all the values, so every brightness ends up about equally common. Brings
 * out detail in murky pictures, though it can look harsh.
 */
Img& Img::equalize()
{
    double counts[256];
    histogram(*this, counts);
    double total = 0;
    for (double c : counts) total += c;
    if (total == 0) return *this;

    uint8_t lut[256];
    double below = 0;   // how many values are at or below v: the cumulative count
    for (int v = 0; v < 256; v++) {
        below += counts[v];
        lut[v] = clampByte(255 * below / total);
    }
    return applyLut(lut);
}
