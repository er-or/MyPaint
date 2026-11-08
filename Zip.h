// Zip.h
//
// Reading and writing zip archives, written for MyPaint's OpenRaster (.ora)
// files, which are zips of PNG layers plus a small XML file. Like Img, it is
// plain code meant to be read and edited by hand, not a full zip library:
//
//  - Reading handles the two ways zips store files: as they are ("stored")
//    and compressed with deflate, which is what other programs write.
//  - Writing only stores files as they are. PNGs are already compressed, so
//    compressing them again would gain next to nothing.
//  - No encryption, no zip64 (archives and files over 4 GB), no spanning.
//
// The deflate decoder (inflate) is also meant for Windows Paint's .paint
// files, whose layers are deflate-compressed pixels.
//
// Formats, for whoever edits this:
//  zip:     https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
//  deflate: https://www.rfc-editor.org/rfc/rfc1951

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Zip {

// The CRC-32 zip files use to check each file. Pass the previous result as
// crc to continue over more data.
uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0);

// Decompresses raw deflate data (no zlib or gzip header) onto the end of out.
// expectedSize, when known, is reserved up front. False if the data is damaged.
bool inflate(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t expectedSize = 0);

// Decompresses zlib data: deflate with a 2-byte header and a checksum at the end
bool inflateZlib(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t expectedSize = 0);

struct Entry {
    std::string name;            // as stored: forward slashes, UTF-8 or the old DOS code page
    uint16_t    method = 0;      // 0 stored, 8 deflate
    uint32_t    crc = 0;
    uint32_t    packedSize = 0;
    uint32_t    size = 0;
    uint32_t    localHeader = 0; // where its local header starts in the archive
};

// Reads an archive held in memory
class Reader {
public:
    // Takes the archive's bytes. False if it isn't a zip MyPaint can read.
    bool open(std::vector<uint8_t> bytes);

    const std::vector<Entry>& entries() const { return list; }
    const Entry* find(const std::string& name) const;

    // The file's contents, checked against its CRC. False if missing or damaged.
    bool extract(const std::string& name, std::vector<uint8_t>& out) const;
    bool extract(const Entry& e, std::vector<uint8_t>& out) const;

private:
    std::vector<uint8_t> data;
    std::vector<Entry> list;
};

// Builds an archive in memory, every file stored as it is
class Writer {
public:
    void add(const std::string& name, const uint8_t* bytes, size_t size);
    void add(const std::string& name, const std::vector<uint8_t>& bytes) { add(name, bytes.data(), bytes.size()); }
    void add(const std::string& name, const std::string& text) { add(name, (const uint8_t*)text.data(), text.size()); }

    // Writes the central directory and returns the whole archive
    std::vector<uint8_t> finish();

private:
    std::vector<uint8_t> out;
    std::vector<Entry> list;
};

}   // namespace Zip
