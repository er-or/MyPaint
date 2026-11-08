// Zip.cpp
//
// See Zip.h.

#include "Zip.h"
#include <cstring>

namespace Zip {


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// CRC-32
//
// The standard one (polynomial 0xEDB88320), a byte at a time from a table.
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc)
{
    static uint32_t table[256];
    static bool made = false;
    if (!made) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        made = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < size; i++) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Inflate (deflate decompression), RFC 1951
//
// Deflate data is a series of blocks. Each is either stored as it is, or a
// stream of Huffman codes, each code being a literal byte, the end of the
// block, or "copy n bytes from d bytes back". The codes come from either a
// fixed table or one the block describes at its start.
//
// This follows the approach of Mark Adler's puff.c, which favours being easy
// to follow over speed: codes are decoded a bit at a time.
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


namespace {

// Hands out bits, lowest bit of each byte first, as deflate packs them
struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t pos = 0;
    uint32_t held = 0;   // bits read from data but not used yet
    int heldCount = 0;
    bool overrun = false;

    BitReader(const uint8_t* d, size_t n) : data(d), size(n) {}

    // The next `need` bits (up to 24) as a number
    int bits(int need)
    {
        uint32_t value = held;
        while (heldCount < need) {
            if (pos >= size) { overrun = true; return 0; }
            value |= (uint32_t)data[pos++] << heldCount;
            heldCount += 8;
        }
        held = value >> need;
        heldCount -= need;
        return (int)(value & ((1u << need) - 1));
    }

    // Stored blocks start on a byte boundary: drop what's left of this byte
    void toByteBoundary() { held = 0; heldCount = 0; }
};

// A canonical Huffman code, kept as how many codes there are of each length
// and the symbols in code order. That is all canonical codes need.
struct Huffman {
    int16_t count[16];     // count[n]: how many codes are n bits long
    int16_t symbol[288];   // the symbols, shortest codes first
};

// Builds h from the code length of each symbol (0 = not used). Returns 0 if
// the code is complete, above 0 if it is incomplete, below 0 if impossible.
int buildHuffman(Huffman& h, const int16_t* lengths, int n)
{
    for (int len = 0; len < 16; len++) h.count[len] = 0;
    for (int s = 0; s < n; s++) h.count[lengths[s]]++;
    if (h.count[0] == n) return 0;   // no codes at all: complete, but nothing can be decoded

    // Each length doubles the codes available; more used than available is impossible
    int left = 1;
    for (int len = 1; len < 16; len++) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return left;
    }

    // Where each length's symbols start in h.symbol
    int16_t offset[16];
    offset[1] = 0;
    for (int len = 1; len < 15; len++) offset[len + 1] = offset[len] + h.count[len];
    for (int s = 0; s < n; s++)
        if (lengths[s] != 0) h.symbol[offset[lengths[s]]++] = (int16_t)s;
    return left;
}

// Reads one symbol. Canonical codes of each length are consecutive numbers,
// so a bit at a time, the code is checked against the range for its length.
int decodeSymbol(BitReader& br, const Huffman& h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= br.bits(1);
        int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
        if (br.overrun) return -1;
    }
    return -1;   // ran past the longest code: damaged data
}

// Length and distance codes: a base value, plus this many extra bits read after the code
const int16_t kLengthBase[29]  = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                   35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
const int16_t kLengthExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                   3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
const int16_t kDistBase[30]    = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                   257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                   8193, 12289, 16385, 24577 };
const int16_t kDistExtra[30]   = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                   7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

// Decodes one block's codes until its end-of-block code
bool inflateCodes(BitReader& br, std::vector<uint8_t>& out, size_t start, const Huffman& lit, const Huffman& dist)
{
    for (;;) {
        int sym = decodeSymbol(br, lit);
        if (sym < 0) return false;
        if (sym < 256) {                 // a literal byte
            out.push_back((uint8_t)sym);
        } else if (sym == 256) {         // end of block
            return true;
        } else {                         // copy an earlier run
            sym -= 257;
            if (sym >= 29) return false;
            int length = kLengthBase[sym] + br.bits(kLengthExtra[sym]);
            int dsym = decodeSymbol(br, dist);
            if (dsym < 0 || dsym >= 30) return false;
            size_t back = (size_t)kDistBase[dsym] + br.bits(kDistExtra[dsym]);
            if (br.overrun || back > out.size() - start) return false;   // reaches before the start
            // A byte at a time, as the run may overlap itself (back < length repeats a pattern)
            size_t from = out.size() - back;
            for (int k = 0; k < length; k++) out.push_back(out[from + k]);
        }
    }
}

// The fixed code (block type 1), built once
void fixedTables(Huffman& lit, Huffman& dist)
{
    int16_t lengths[288];
    int s = 0;
    for (; s < 144; s++) lengths[s] = 8;
    for (; s < 256; s++) lengths[s] = 9;
    for (; s < 280; s++) lengths[s] = 7;
    for (; s < 288; s++) lengths[s] = 8;
    buildHuffman(lit, lengths, 288);
    for (s = 0; s < 30; s++) lengths[s] = 5;
    buildHuffman(dist, lengths, 30);
}

// Reads a block's own code (block type 2): the code lengths are themselves
// Huffman coded, with a small code described first
bool dynamicTables(BitReader& br, Huffman& lit, Huffman& dist)
{
    static const int8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    int nlen  = br.bits(5) + 257;
    int ndist = br.bits(5) + 1;
    int ncode = br.bits(4) + 4;
    if (br.overrun || nlen > 286 || ndist > 30) return false;

    // The code for the code lengths
    int16_t lengths[320] = {};
    for (int i = 0; i < ncode; i++) lengths[order[i]] = (int16_t)br.bits(3);
    Huffman lencode;
    if (buildHuffman(lencode, lengths, 19) != 0) return false;   // must be complete

    // The literal/length and distance code lengths, run-length coded
    int i = 0;
    while (i < nlen + ndist) {
        int sym = decodeSymbol(br, lencode);
        if (sym < 0) return false;
        if (sym < 16) {                  // a length
            lengths[i++] = (int16_t)sym;
        } else {
            int16_t value = 0;
            int repeat;
            if (sym == 16) {             // the previous length, 3 to 6 times
                if (i == 0) return false;
                value = lengths[i - 1];
                repeat = 3 + br.bits(2);
            } else if (sym == 17) {      // zero, 3 to 10 times
                repeat = 3 + br.bits(3);
            } else {                     // zero, 11 to 138 times
                repeat = 11 + br.bits(7);
            }
            if (i + repeat > nlen + ndist) return false;
            while (repeat--) lengths[i++] = value;
        }
    }
    if (lengths[256] == 0) return false;   // there must be an end-of-block code

    // The literal/length code must be complete, unless it is a single code
    int err = buildHuffman(lit, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lit.count[0] != 1)) return false;
    err = buildHuffman(dist, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - dist.count[0] != 1)) return false;
    return true;
}

}   // namespace


bool inflate(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t expectedSize)
{
    if (expectedSize) out.reserve(out.size() + expectedSize);
    size_t start = out.size();
    BitReader br(data, size);
    static Huffman fixedLit, fixedDist;
    static bool fixedMade = false;
    if (!fixedMade) { fixedTables(fixedLit, fixedDist); fixedMade = true; }

    bool last;
    do {
        last = br.bits(1) == 1;
        int type = br.bits(2);
        if (br.overrun) return false;
        if (type == 0) {
            // Stored: a length, its ones' complement, then the bytes as they are
            br.toByteBoundary();
            if (br.pos + 4 > size) return false;
            unsigned len  = data[br.pos] | (data[br.pos + 1] << 8);
            unsigned nlen = data[br.pos + 2] | (data[br.pos + 3] << 8);
            br.pos += 4;
            if (len != (~nlen & 0xFFFF) || br.pos + len > size) return false;
            out.insert(out.end(), data + br.pos, data + br.pos + len);
            br.pos += len;
        } else if (type == 1) {
            if (!inflateCodes(br, out, start, fixedLit, fixedDist)) return false;
        } else if (type == 2) {
            Huffman lit, dist;
            if (!dynamicTables(br, lit, dist) || !inflateCodes(br, out, start, lit, dist)) return false;
        } else {
            return false;   // type 3 doesn't exist
        }
    } while (!last);
    return true;
}


bool inflateZlib(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t expectedSize)
{
    // Header: compression method 8 (deflate), and the two bytes as a number divisible by 31
    if (size < 6 || (data[0] & 0x0F) != 8 || ((data[0] << 8) | data[1]) % 31 != 0) return false;
    if (data[1] & 0x20) return false;   // a preset dictionary, which nothing here uses
    size_t start = out.size();
    if (!inflate(data + 2, size - 6, out, expectedSize)) return false;

    // Adler-32 of the result, big-endian, at the very end
    uint32_t a = 1, b = 0;
    for (size_t i = start; i < out.size(); i++) { a = (a + out[i]) % 65521; b = (b + a) % 65521; }
    const uint8_t* t = data + size - 4;
    uint32_t stored = ((uint32_t)t[0] << 24) | (t[1] << 16) | (t[2] << 8) | t[3];
    return stored == ((b << 16) | a);
}


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Reading archives
//
// A zip ends with an "end of central directory" record pointing to the
// central directory, a list of every file with its size, CRC and where its
// local header is. Each file's data follows its local header. The central
// directory is what this trusts; local headers are only used to find the data.
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


namespace {

uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t u32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

}   // namespace


bool Reader::open(std::vector<uint8_t> bytes)
{
    data = std::move(bytes);
    list.clear();
    size_t n = data.size();
    if (n < 22) return false;

    // The end record is the last 22 bytes, unless the archive has a comment
    // (up to 65535 bytes) after it, so search back for its signature
    size_t end = SIZE_MAX;
    size_t lowest = n > 22 + 65535 ? n - 22 - 65535 : 0;
    for (size_t p = n - 22 + 1; p-- > lowest;) {
        if (u32(&data[p]) == 0x06054b50) { end = p; break; }
    }
    if (end == SIZE_MAX) return false;

    uint16_t count  = u16(&data[end + 10]);
    uint32_t dirLen = u32(&data[end + 12]);
    uint32_t dirAt  = u32(&data[end + 16]);
    if (dirAt == 0xFFFFFFFF || (size_t)dirAt + dirLen > end) return false;   // zip64, or nonsense

    size_t p = dirAt;
    for (int i = 0; i < count; i++) {
        if (p + 46 > end || u32(&data[p]) != 0x02014b50) return false;
        uint16_t flags = u16(&data[p + 8]);
        Entry e;
        e.method      = u16(&data[p + 10]);
        e.crc         = u32(&data[p + 16]);
        e.packedSize  = u32(&data[p + 20]);
        e.size        = u32(&data[p + 24]);
        uint16_t nameLen = u16(&data[p + 28]), extraLen = u16(&data[p + 30]), commentLen = u16(&data[p + 32]);
        e.localHeader = u32(&data[p + 42]);
        if (p + 46 + nameLen > end) return false;
        e.name.assign((const char*)&data[p + 46], nameLen);
        if (!(flags & 1) && e.packedSize != 0xFFFFFFFF && e.size != 0xFFFFFFFF)   // skip encrypted and zip64 files
            list.push_back(std::move(e));
        p += 46 + nameLen + extraLen + commentLen;
    }
    return true;
}


const Entry* Reader::find(const std::string& name) const
{
    for (const Entry& e : list) if (e.name == name) return &e;
    return nullptr;
}


bool Reader::extract(const std::string& name, std::vector<uint8_t>& out) const
{
    const Entry* e = find(name);
    return e && extract(*e, out);
}


bool Reader::extract(const Entry& e, std::vector<uint8_t>& out) const
{
    out.clear();
    size_t h = e.localHeader;
    if (h + 30 > data.size() || u32(&data[h]) != 0x04034b50) return false;
    // The local header's name and extra field can differ in length from the central directory's
    size_t at = h + 30 + u16(&data[h + 26]) + u16(&data[h + 28]);
    if (at + e.packedSize > data.size()) return false;
    const uint8_t* packed = &data[at];

    if (e.method == 0) {
        if (e.packedSize != e.size) return false;
        out.assign(packed, packed + e.size);
    } else if (e.method == 8) {
        if (!inflate(packed, e.packedSize, out, e.size) || out.size() != e.size) return false;
    } else {
        return false;   // another compression method
    }
    return crc32(out.data(), out.size()) == e.crc;
}


//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---
//
// Writing archives
//
// Each file: a local header, its name, then its bytes. At the end, the
// central directory repeats every header, and the end record points to it.
//
//----+----+----+----+----+----+----+----+----+----+----+----+----+----+---


namespace {

void put16(std::vector<uint8_t>& v, uint32_t x) { v.push_back((uint8_t)x); v.push_back((uint8_t)(x >> 8)); }
void put32(std::vector<uint8_t>& v, uint32_t x) { put16(v, x & 0xFFFF); put16(v, x >> 16); }

// Bit 11 says the name is UTF-8; set it only when the name isn't plain ASCII
uint16_t nameFlags(const std::string& name)
{
    for (unsigned char c : name) if (c >= 0x80) return 0x0800;
    return 0;
}

// Every file gets the same date, 1 January 1980, the earliest zip can hold, as
// MyPaint's files don't need one and that keeps archives identical across saves
const uint16_t kDosTime = 0, kDosDate = (0 << 9) | (1 << 5) | 1;

}   // namespace


void Writer::add(const std::string& name, const uint8_t* bytes, size_t size)
{
    Entry e;
    e.name = name;
    e.method = 0;
    e.crc = crc32(bytes, size);
    e.packedSize = e.size = (uint32_t)size;
    e.localHeader = (uint32_t)out.size();

    put32(out, 0x04034b50);   // local header signature
    put16(out, 20);           // version needed to extract: 2.0
    put16(out, nameFlags(name));
    put16(out, 0);            // method: stored
    put16(out, kDosTime);
    put16(out, kDosDate);
    put32(out, e.crc);
    put32(out, e.packedSize);
    put32(out, e.size);
    put16(out, (uint32_t)name.size());
    put16(out, 0);            // no extra field
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), bytes, bytes + size);
    list.push_back(std::move(e));
}


std::vector<uint8_t> Writer::finish()
{
    uint32_t dirAt = (uint32_t)out.size();
    for (const Entry& e : list) {
        put32(out, 0x02014b50);   // central directory signature
        put16(out, 20);           // made by: version 2.0
        put16(out, 20);           // needed to extract
        put16(out, nameFlags(e.name));
        put16(out, e.method);
        put16(out, kDosTime);
        put16(out, kDosDate);
        put32(out, e.crc);
        put32(out, e.packedSize);
        put32(out, e.size);
        put16(out, (uint32_t)e.name.size());
        put16(out, 0);            // extra field
        put16(out, 0);            // comment
        put16(out, 0);            // disk number
        put16(out, 0);            // internal attributes
        put32(out, 0);            // external attributes
        put32(out, e.localHeader);
        out.insert(out.end(), e.name.begin(), e.name.end());
    }
    uint32_t dirLen = (uint32_t)out.size() - dirAt;
    put32(out, 0x06054b50);       // end of central directory signature
    put16(out, 0);                // this disk
    put16(out, 0);                // disk with the directory
    put16(out, (uint32_t)list.size());
    put16(out, (uint32_t)list.size());
    put32(out, dirLen);
    put32(out, dirAt);
    put16(out, 0);                // no comment
    std::vector<uint8_t> result;
    result.swap(out);
    list.clear();
    return result;
}

}   // namespace Zip
