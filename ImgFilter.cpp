// ImgFilter.cpp
//
// Filters: each new pixel is worked out from the pixels around it, not just
// itself. Most are convolutions ("direct convolution" in Img.js): a small
// grid of weights, the kernel, laid over each pixel in turn.

#include "Img.h"
#include <algorithm>
#include <cmath>


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Kernels
//
// Every preset works at any odd size. The weights are kept to small whole
// numbers where that is possible, with the divisor bringing the total back
// to 1, so they are easy to read and to change by hand.
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


float Kernel::sum() const
{
    float s = 0;
    for (float w : v) s += w;
    return s;
}


/**
 * The divisor that keeps the picture's overall brightness: the sum of the
 * weights. Kernels whose weights cancel out (edges, emboss) use 1.
 */
float Kernel::autoDivisor() const
{
    float s = sum();
    return std::fabs(s) < 1e-6f ? 1.0f : s;
}


Kernel Kernel::identity(int n)
{
    Kernel k(n);
    k.at(n / 2, n / 2) = 1;
    return k;
}


/**
 *   1 1 1
 *   1 1 1   / 9
 *   1 1 1
 */
Kernel Kernel::box(int n)
{
    Kernel k(n);
    for (float& w : k.v) w = 1;
    k.divisor = k.autoDivisor();
    return k;
}


/**
 * A bell curve: 100 in the middle, falling away with distance d as
 * 100 * e^(-d^2 / 2 sigma^2). sigma grows with the size so that the edge of
 * the kernel is still worth something (about a fifth on the sides).
 *
 *   37  61  37
 *   61 100  61   / 553   at 3 x 3
 *   37  61  37
 */
Kernel Kernel::gaussian(int n)
{
    Kernel k(n);
    int half = n / 2;
    double sigma = (half + 1) / 2.0;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            double dx = x - half, dy = y - half;
            k.at(x, y) = (float)std::round(100 * std::exp(-(dx * dx + dy * dy) / (2 * sigma * sigma)));
        }
    k.divisor = k.autoDivisor();
    return k;
}


/**
 * Twice the pixel minus the average of the square around it (the pixel
 * included): the difference from its surroundings is doubled, so edges
 * stand out more. Bigger sizes sharpen coarser detail.
 *
 *   -1 -1 -1
 *   -1 17 -1   / 9
 *   -1 -1 -1
 */
Kernel Kernel::sharpen(int n)
{
    Kernel k(n);
    for (float& w : k.v) w = -1;
    k.at(n / 2, n / 2) = (float)(2 * n * n - 1);
    k.divisor = k.autoDivisor();
    return k;
}


/**
 * The pixel against everything around it. The weights add up to 0, so flat
 * areas go black and only changes (edges) show. This is a Laplacian.
 *
 *   -1 -1 -1
 *   -1  8 -1
 *   -1 -1 -1
 */
Kernel Kernel::edges(int n)
{
    Kernel k(n);
    for (float& w : k.v) w = -1;
    k.at(n / 2, n / 2) = (float)(n * n - 1);
    k.divisor = 1;
    k.filterAlpha = false;
    return k;
}


/**
 * Weights that run from negative at the top left to positive at the bottom
 * right: edges facing one way turn light, the other way dark, flat areas
 * middle gray (the offset of 128), as if lit from one side.
 *
 *   -2 -1  0
 *   -1  0  1   + 128
 *    0  1  2
 */
Kernel Kernel::emboss(int n)
{
    Kernel k(n);
    int half = n / 2;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            k.at(x, y) = (float)((x - half) + (y - half));
    k.divisor = (float)std::max(1, half);   // keeps bigger sizes from going all black and white
    k.offset  = 128;
    k.filterAlpha = false;
    return k;
}


/**
 * An average along the middle row only, like a photo of something moving
 * sideways.
 *
 *   0 0 0
 *   1 1 1   / 3
 *   0 0 0
 */
Kernel Kernel::motion(int n)
{
    Kernel k(n);
    for (int x = 0; x < n; x++) k.at(x, n / 2) = 1;
    k.divisor = k.autoDivisor();
    return k;
}


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Convolution
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


/**
 * Lays the kernel over every pixel: the new value is the weighted sum of the
 * pixels under it, divided by the divisor, plus the offset.
 *
 * Alpha, when k.filterAlpha is set: alpha is filtered like the colours, and
 * each colour counts by how much it shows (premultiplied), so a blur doesn't
 * pull black in from see-through pixels. A colour can't end up stronger than
 * its alpha. This is what browsers do for SVG's feConvolveMatrix. Kernels
 * whose weights add up to 0 (edges, emboss) leave flat areas see-through.
 * When it isn't set, alpha stays as it is and only the colours are filtered.
 */
Img& Img::convolve(const Kernel& k)
{
    if (w == 0 || h == 0) return *this;
    const int n = k.size, half = n / 2;
    // A divisor of 0 can't be divided by, so it becomes a tiny one instead: the
    // sums are multiplied a millionfold, and each channel lands on 0 or 255
    // depending on whether its sum was below or above 0.
    const float tiny = 1e-6f;
    const float div = std::fabs(k.divisor) < tiny ? (k.divisor < 0 ? -tiny : tiny) : k.divisor;
    const bool alphaToo = k.filterAlpha;

    // The weights that aren't 0, with where they reach to
    struct Tap { int dx, dy; float weight; };
    std::vector<Tap> taps;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            if (k.at(x, y) != 0) taps.push_back({ x - half, y - half, k.at(x, y) / div });

    // Float copies of the planes, colours scaled by alpha when alpha takes part
    size_t count = (size_t)w * h;
    std::vector<float> R(count), G(count), B(count), A(count);
    for (size_t i = 0; i < count; i++) {
        float show = alphaToo ? a[i] / 255.0f : 1.0f;
        R[i] = r[i] * show;
        G[i] = g[i] * show;
        B[i] = b[i] * show;
        A[i] = a[i];
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sr = 0, sg = 0, sb = 0, sa = 0;
            for (const Tap& t : taps) {
                int sx = std::max(0, std::min(x + t.dx, w - 1));   // past the edge: the edge pixel
                int sy = std::max(0, std::min(y + t.dy, h - 1));
                size_t j = (size_t)sy * w + sx;
                sr += t.weight * R[j];
                sg += t.weight * G[j];
                sb += t.weight * B[j];
                sa += t.weight * A[j];
            }

            int i = at(x, y);
            if (alphaToo) {
                float alpha = std::max(0.0f, std::min(sa, 255.0f));
                float unscale = alpha > 0 ? 255.0f / alpha : 0;   // back from "scaled by alpha"
                sr *= unscale; sg *= unscale; sb *= unscale;
                a[i] = (uint8_t)(alpha + 0.5f);
            }
            r[i] = (uint8_t)std::max(0.0f, std::min(sr + k.offset + 0.5f, 255.0f));
            g[i] = (uint8_t)std::max(0.0f, std::min(sg + k.offset + 0.5f, 255.0f));
            b[i] = (uint8_t)std::max(0.0f, std::min(sb + k.offset + 0.5f, 255.0f));
        }
    }
    return *this;
}


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Others
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


/**
 * Grain: every channel of every pixel moves up or down by a random amount,
 * up to about amount * 1.3 (so 100 is up to half of white). The same seed
 * gives the same grain, so a preview and the final result match.
 */
Img& Img::addNoise(int amount, unsigned seed)
{
    unsigned s = seed ? seed : 1;
    auto random = [&]() {   // xorshift: quick, and plenty random for grain; -1 to 1
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (s & 0xFFFF) / 32767.5f - 1.0f;
    };
    float spread = amount * 1.275f;
    auto shift = [&](uint8_t v) {
        float nv = v + random() * spread;
        return (uint8_t)std::max(0.0f, std::min(nv + 0.5f, 255.0f));
    };
    for (size_t i = 0; i < r.size(); i++) {
        r[i] = shift(r[i]);
        g[i] = shift(g[i]);
        b[i] = shift(b[i]);
    }
    return *this;
}


/**
 * Big square pixels: the picture is cut into cellSize x cellSize squares
 * from the top left, and each square is filled with its average colour
 * (colours counted by how much they show) and average alpha.
 */
Img& Img::pixelate(int cellSize)
{
    int c = std::max(1, cellSize);
    for (int cy = 0; cy < h; cy += c) {
        for (int cx = 0; cx < w; cx += c) {
            int x1 = std::min(cx + c, w), y1 = std::min(cy + c, h);

            double sr = 0, sg = 0, sb = 0, sa = 0;
            for (int y = cy; y < y1; y++)
                for (int x = cx; x < x1; x++) {
                    int i = at(x, y);
                    sr += r[i] * a[i]; sg += g[i] * a[i]; sb += b[i] * a[i]; sa += a[i];
                }
            int pixels = (x1 - cx) * (y1 - cy);
            uint8_t ar = 0, ag = 0, ab = 0;
            if (sa > 0) {
                ar = (uint8_t)(sr / sa + 0.5); ag = (uint8_t)(sg / sa + 0.5); ab = (uint8_t)(sb / sa + 0.5);
            }
            uint8_t aa = (uint8_t)(sa / pixels + 0.5);

            for (int y = cy; y < y1; y++)
                for (int x = cx; x < x1; x++) {
                    int i = at(x, y);
                    r[i] = ar; g[i] = ag; b[i] = ab; a[i] = aa;
                }
        }
    }
    return *this;
}
