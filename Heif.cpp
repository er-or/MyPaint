// Heif.cpp
//
// See Heif.h.

#include "Heif.h"
#include "Zip.h"
#include <map>

namespace Heif {

namespace {

//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Reading big-endian numbers without running off the end
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---

struct Cursor {
    const uint8_t* data;
    size_t pos, end;
    bool overrun = false;

    Cursor(const uint8_t* d, size_t from, size_t to) : data(d), pos(from), end(to) {}

    bool has(size_t n) const { return !overrun && pos + n <= end; }

    uint64_t read(int bytes)   // 0 to 8 bytes, big-endian
    {
        if (!has(bytes)) { overrun = true; pos = end; return 0; }
        uint64_t v = 0;
        for (int i = 0; i < bytes; i++) v = (v << 8) | data[pos++];
        return v;
    }
    uint8_t  u8()  { return (uint8_t)read(1); }
    uint16_t u16() { return (uint16_t)read(2); }
    uint32_t u32() { return (uint32_t)read(4); }
    std::string fourcc() { std::string s; for (int i = 0; i < 4; i++) s += (char)u8(); return s; }
    void skip(size_t n) { if (has(n)) pos += n; else { overrun = true; pos = end; } }
};

// A box: its type, and where its contents start and end
struct Box {
    std::string type;
    size_t start = 0, end = 0;
};

// The boxes between from and to, in order
std::vector<Box> listBoxes(const uint8_t* data, size_t from, size_t to)
{
    std::vector<Box> boxes;
    Cursor c(data, from, to);
    while (c.has(8)) {
        size_t at = c.pos;
        uint64_t size = c.u32();
        Box b;
        b.type = c.fourcc();
        if (size == 1) size = c.read(8);   // a 64-bit size follows
        else if (size == 0) size = to - at; // to the end
        if (size < c.pos - at || at + size > to) break;   // damaged
        b.start = c.pos;
        b.end = at + (size_t)size;
        boxes.push_back(b);
        c.pos = b.end;
    }
    return boxes;
}

const Box* findBox(const std::vector<Box>& boxes, const char* type)
{
    for (const Box& b : boxes) if (b.type == type) return &b;
    return nullptr;
}

// What the 'meta' box says about each item
struct Item {
    std::string type;                              // 'unci', 'iovl', 'Exif', ...
    int construction = 0;                          // 0: bytes are in the file; 1: in 'idat'
    std::vector<std::pair<uint64_t, uint64_t>> extents;   // offset and length of each piece
    std::vector<int> properties;                   // indexes into the property list, from 1
    std::vector<uint32_t> dimg;                    // for an overlay: the items it stacks
};

bool fail(std::string* error, const char* why)
{
    if (error) *error = why;
    return false;
}

}   // namespace


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// The item tables in 'meta'
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---

bool readLayers(const std::vector<uint8_t>& file, Picture& out, std::string* error)
{
    const uint8_t* d = file.data();
    std::vector<Box> top = listBoxes(d, 0, file.size());
    const Box* ftyp = findBox(top, "ftyp");
    const Box* meta = findBox(top, "meta");
    if (!ftyp || !meta) return fail(error, "not a HEIF file");

    // 'meta' is a full box: 4 bytes of version and flags before its children
    std::vector<Box> mb = listBoxes(d, meta->start + 4, meta->end);
    std::map<uint32_t, Item> items;
    uint32_t primary = 0;
    size_t idatStart = 0, idatEnd = 0;

    // pitm: which item is the main picture
    if (const Box* b = findBox(mb, "pitm")) {
        Cursor c(d, b->start, b->end);
        int version = c.u8(); c.skip(3);
        primary = version == 0 ? c.u16() : c.u32();
    }

    // iinf: every item's type, one 'infe' box each
    if (const Box* b = findBox(mb, "iinf")) {
        Cursor c(d, b->start, b->end);
        int version = c.u8(); c.skip(3);
        c.skip(version == 0 ? 2 : 4);   // the count; the boxes say the same
        for (const Box& infe : listBoxes(d, c.pos, b->end)) {
            if (infe.type != "infe") continue;
            Cursor e(d, infe.start, infe.end);
            int v = e.u8(); e.skip(3);
            if (v < 2) continue;   // versions 0 and 1 predate item types
            uint32_t id = v == 2 ? e.u16() : e.u32();
            e.skip(2);             // protection index
            items[id].type = e.fourcc();
        }
    }

    // iloc: where each item's bytes are
    if (const Box* b = findBox(mb, "iloc")) {
        Cursor c(d, b->start, b->end);
        int version = c.u8(); c.skip(3);
        int sizes = c.u8(), sizes2 = c.u8();
        int offsetSize = sizes >> 4, lengthSize = sizes & 15;
        int baseSize = sizes2 >> 4, indexSize = (version == 1 || version == 2) ? sizes2 & 15 : 0;
        uint32_t count = version < 2 ? c.u16() : c.u32();
        for (uint32_t i = 0; i < count && !c.overrun; i++) {
            uint32_t id = version < 2 ? c.u16() : c.u32();
            Item& it = items[id];
            if (version == 1 || version == 2) it.construction = c.u16() & 15;
            c.skip(2);   // data reference index
            uint64_t base = c.read(baseSize);
            int extents = c.u16();
            for (int k = 0; k < extents; k++) {
                c.read(indexSize);
                uint64_t off = c.read(offsetSize);
                uint64_t len = c.read(lengthSize);
                it.extents.push_back({ base + off, len });
            }
        }
        if (c.overrun) return fail(error, "damaged item locations");
    }

    // idat: item bytes kept inside 'meta' (the overlay's, in Paint's files)
    if (const Box* b = findBox(mb, "idat")) { idatStart = b->start; idatEnd = b->end; }

    // iprp: the properties (ipco), and which items have which (ipma)
    std::vector<Box> props;
    if (const Box* b = findBox(mb, "iprp")) {
        std::vector<Box> pb = listBoxes(d, b->start, b->end);
        if (const Box* ipco = findBox(pb, "ipco")) props = listBoxes(d, ipco->start, ipco->end);
        for (const Box& ipma : pb) {
            if (ipma.type != "ipma") continue;
            Cursor c(d, ipma.start, ipma.end);
            int version = c.u8();
            int flags = (int)c.read(3);
            uint32_t count = c.u32();
            for (uint32_t i = 0; i < count && !c.overrun; i++) {
                uint32_t id = version < 1 ? c.u16() : c.u32();
                int n = c.u8();
                for (int k = 0; k < n; k++) {
                    // The top bit says "essential"; the rest is the property's place in the list
                    int index = (flags & 1) ? (c.u16() & 0x7FFF) : (c.u8() & 0x7F);
                    items[id].properties.push_back(index);
                }
            }
        }
    }

    // iref: references between items; 'dimg' lists an overlay's layers
    if (const Box* b = findBox(mb, "iref")) {
        Cursor c(d, b->start, b->end);
        int version = c.u8(); c.skip(3);
        for (const Box& r : listBoxes(d, c.pos, b->end)) {
            Cursor e(d, r.start, r.end);
            uint32_t from = version == 0 ? e.u16() : e.u32();
            int n = e.u16();
            for (int k = 0; k < n; k++) {
                uint32_t to = version == 0 ? e.u16() : e.u32();
                if (r.type == "dimg") items[from].dimg.push_back(to);
            }
        }
    }

    //----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
    //
    // An item's bytes, and its properties
    //
    //----+----+----+----+----+----+----+----+----+----+----+----+----+----+---

    auto itemBytes = [&](const Item& it, std::vector<uint8_t>& bytes) -> bool {
        bytes.clear();
        for (auto& e : it.extents) {
            size_t from, to;
            if (it.construction == 0) { from = 0; to = file.size(); }
            else if (it.construction == 1) { from = idatStart; to = idatEnd; }
            else return false;   // 2: built from other items, not used here
            uint64_t len = e.second ? e.second : to - from - e.first;   // 0 means "to the end"
            if (e.first > to - from || len > to - from - e.first) return false;
            bytes.insert(bytes.end(), d + from + e.first, d + from + e.first + len);
        }
        return true;
    };

    auto property = [&](const Item& it, const char* type) -> const Box* {
        for (int index : it.properties)
            if (index >= 1 && index <= (int)props.size() && props[index - 1].type == type) return &props[index - 1];
        return nullptr;
    };

    //----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
    //
    // An uncompressed ('unci') picture
    //
    // uncC says which components there are and how they are laid out: one after
    // another for each pixel ("pixel interleaved") or a whole plane of each
    // ("component interleaved"). cmpd says what each component is: 4 red,
    // 5 green, 6 blue, 7 alpha, 0 grey. cmpC, if there, says the bytes are
    // compressed, and how.
    //
    //----+----+----+----+----+----+----+----+----+----+----+----+----+----+---

    auto readUnci = [&](const Item& it, Layer& layer, std::string& why) -> bool {
        const Box* ispe = property(it, "ispe");
        const Box* uncC = property(it, "uncC");
        const Box* cmpd = property(it, "cmpd");
        if (!ispe || !uncC) { why = "a layer without its size or layout"; return false; }
        Cursor s(d, ispe->start, ispe->end);
        s.skip(4);
        int w = (int)s.u32(), h = (int)s.u32();
        if (s.overrun || w <= 0 || h <= 0 || w > 32767 || h > 32767) { why = "a layer of an impossible size"; return false; }

        // What each component is (cmpd), by its place in the list
        std::vector<int> kinds;
        if (cmpd) {
            Cursor c(d, cmpd->start, cmpd->end);
            uint32_t n = c.u32();
            for (uint32_t i = 0; i < n && !c.overrun; i++) {
                int kind = c.u16();
                if (kind >= 0x8000) while (c.has(1) && c.u8() != 0) {}   // a URI follows: skip it
                kinds.push_back(kind);
            }
        }

        // The layout (uncC). Version 1 just names a common one.
        Cursor c(d, uncC->start, uncC->end);
        int version = c.u8(); c.skip(3);
        std::string profile = c.fourcc();
        std::vector<int> order;   // the component kinds, in the order the bytes come
        int interleave = 1;       // 0: one plane per component, 1: per pixel
        if (version == 1) {
            if      (profile == "rgb3") order = { 4, 5, 6 };
            else if (profile == "rgba") order = { 4, 5, 6, 7 };
            else if (profile == "abgr") order = { 7, 6, 5, 4 };
            else { why = "an uncompressed layout this can't read"; return false; }
        } else {
            uint32_t n = c.u32();
            for (uint32_t i = 0; i < n && !c.overrun; i++) {
                int index = c.u16();
                int bits = c.u8() + 1;
                int format = c.u8();
                c.skip(1);   // alignment
                if (bits != 8 || format != 0) { why = "a layer that isn't 8-bit"; return false; }
                if (index >= (int)kinds.size()) { why = "a damaged component list"; return false; }
                order.push_back(kinds[index]);
            }
            int sampling = c.u8();
            interleave = c.u8();
            int blockSize = c.u8();
            c.skip(1);   // flags
            uint32_t pixelSize = c.u32(), rowAlign = c.u32();
            c.skip(4);   // tile alignment
            uint32_t tileCols = c.u32() + 1, tileRows = c.u32() + 1;
            if (c.overrun || sampling != 0 || blockSize != 0 || (interleave != 0 && interleave != 1) ||
                tileCols != 1 || tileRows != 1 || (pixelSize != 0 && pixelSize != order.size()) || rowAlign > 1) {
                why = "an uncompressed layout this can't read";
                return false;
            }
        }
        if (order.empty()) { why = "a layer with no components"; return false; }

        // The bytes, decompressed if cmpC says so
        std::vector<uint8_t> packed, raw;
        if (!itemBytes(it, packed)) { why = "a layer whose data is missing"; return false; }
        size_t want = (size_t)w * h * order.size();
        if (const Box* cmpC = property(it, "cmpC")) {
            Cursor cc(d, cmpC->start, cmpC->end);
            cc.skip(4);
            std::string method = cc.fourcc();
            int unit = cc.u8();
            if (unit != 0) { why = "a layer compressed in pieces"; return false; }
            bool ok = method == "defl" ? Zip::inflate(packed.data(), packed.size(), raw, want)
                    : method == "zlib" ? Zip::inflateZlib(packed.data(), packed.size(), raw, want)
                    : false;
            if (!ok) { why = "a layer that won't decompress"; return false; }
        } else {
            raw.swap(packed);
        }
        if (raw.size() < want) { why = "a layer with too little data"; return false; }

        // Into red, green, blue, alpha
        layer.w = w; layer.h = h;
        layer.rgba.assign((size_t)w * h * 4, 0);
        size_t pixels = (size_t)w * h, n = order.size();
        for (size_t comp = 0; comp < n; comp++) {
            int kind = order[comp];
            for (size_t p = 0; p < pixels; p++) {
                uint8_t v = interleave == 1 ? raw[p * n + comp] : raw[comp * pixels + p];
                uint8_t* px = &layer.rgba[p * 4];
                switch (kind) {
                case 0: px[0] = px[1] = px[2] = v; break;   // grey
                case 4: px[0] = v; break;
                case 5: px[1] = v; break;
                case 6: px[2] = v; break;
                case 7: px[3] = v; break;
                default: break;                              // something else: leave it out
                }
            }
        }
        // No alpha component: every pixel shows
        bool hasAlpha = false;
        for (int k : order) hasAlpha |= k == 7;
        if (!hasAlpha) for (size_t p = 0; p < pixels; p++) layer.rgba[p * 4 + 3] = 255;
        return true;
    };

    //----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
    //
    // The main picture: an overlay of layers, or a single one
    //
    // An overlay's own bytes are: version, flags, the canvas fill colour
    // (red, green, blue, alpha as 16-bit numbers), the canvas size, then an
    // x and y offset for each layer. Flags bit 0 makes the sizes and offsets
    // 32-bit instead of 16. The first layer listed is the bottom one.
    //
    //----+----+----+----+----+----+----+----+----+----+----+----+----+----+---

    auto pit = items.find(primary);
    if (pit == items.end()) return fail(error, "no main picture");
    const Item& main = pit->second;
    out = Picture();
    std::string why;

    if (main.type == "unci") {
        Layer layer;
        if (!readUnci(main, layer, why)) return fail(error, why.c_str());
        out.w = layer.w; out.h = layer.h;
        out.layers.push_back(std::move(layer));
        return true;
    }
    if (main.type != "iovl") return fail(error, "pictures coded as photos (HEVC or AV1), which this doesn't read");

    std::vector<uint8_t> ov;
    if (!itemBytes(main, ov)) return fail(error, "the overlay's data is missing");
    Cursor c(ov.data(), 0, ov.size());
    c.skip(1);   // version
    int flags = c.u8();
    int field = (flags & 1) ? 4 : 2;
    for (int i = 0; i < 4; i++) out.fill[i] = (uint8_t)(c.u16() >> 8);
    out.w = (int)c.read(field);
    out.h = (int)c.read(field);
    if (c.overrun || out.w <= 0 || out.h <= 0 || out.w > 32767 || out.h > 32767) return fail(error, "a canvas of an impossible size");
    for (uint32_t id : main.dimg) {
        Layer layer;
        // Offsets are signed: a layer may start off the canvas
        int64_t x = (int64_t)c.read(field), y = (int64_t)c.read(field);
        if (field == 2) { x = (int16_t)x; y = (int16_t)y; } else { x = (int32_t)x; y = (int32_t)y; }
        auto lit = items.find(id);
        if (lit == items.end() || lit->second.type != "unci") return fail(error, "a layer that isn't an uncompressed picture");
        if (!readUnci(lit->second, layer, why)) return fail(error, why.c_str());
        layer.x = (int)x; layer.y = (int)y;
        out.layers.push_back(std::move(layer));
    }
    if (c.overrun) return fail(error, "damaged overlay data");
    if (out.layers.empty()) return fail(error, "no layers");
    return true;
}

}   // namespace Heif
