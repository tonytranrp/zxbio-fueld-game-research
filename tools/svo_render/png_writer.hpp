#pragma once

// Minimal dependency-free PNG writer (8-bit RGB, stored/uncompressed deflate blocks). The CPU
// reference renderer lives outside render/ on purpose (no Diligent, so it runs in the GPU-less CI
// jobs), which is why it cannot borrow frame_verify.cpp's DiligentTools libpng path. ~120 lines
// beats a new dependency for a diagnostic tool whose files are looked at, not shipped.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace svo_render {

class PngWriter {
public:
    // `rgb` is width*height*3 bytes, rows top to bottom. Returns false if the file cannot be written.
    static bool write(const char* path, std::uint32_t width, std::uint32_t height, const std::uint8_t* rgb) {
        std::vector<std::uint8_t> out;
        static constexpr std::uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        out.insert(out.end(), kSignature, kSignature + 8);

        std::vector<std::uint8_t> ihdr;
        put32(ihdr, width);
        put32(ihdr, height);
        ihdr.push_back(8); // bit depth
        ihdr.push_back(2); // color type: RGB
        ihdr.push_back(0); // compression
        ihdr.push_back(0); // filter
        ihdr.push_back(0); // interlace
        chunk(out, "IHDR", ihdr);

        // Raw scanlines, each prefixed with filter type 0.
        const std::size_t rowBytes = static_cast<std::size_t>(width) * 3u;
        std::vector<std::uint8_t> raw;
        raw.reserve((rowBytes + 1) * height);
        for (std::uint32_t y = 0; y < height; ++y) {
            raw.push_back(0);
            raw.insert(raw.end(), rgb + static_cast<std::size_t>(y) * rowBytes,
                       rgb + static_cast<std::size_t>(y + 1) * rowBytes);
        }

        // zlib stream: header + stored deflate blocks (<= 65535 bytes each) + Adler-32.
        std::vector<std::uint8_t> idat;
        idat.push_back(0x78);
        idat.push_back(0x01);
        std::size_t offset = 0;
        do {
            const std::size_t len = raw.size() - offset < 65535u ? raw.size() - offset : 65535u;
            const bool final = offset + len >= raw.size();
            idat.push_back(final ? 1 : 0);
            idat.push_back(static_cast<std::uint8_t>(len & 0xFFu));
            idat.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFFu));
            idat.push_back(static_cast<std::uint8_t>(~len & 0xFFu));
            idat.push_back(static_cast<std::uint8_t>((~len >> 8) & 0xFFu));
            idat.insert(idat.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                        raw.begin() + static_cast<std::ptrdiff_t>(offset + len));
            offset += len;
        } while (offset < raw.size());
        put32(idat, adler32(raw.data(), raw.size()));
        chunk(out, "IDAT", idat);
        chunk(out, "IEND", {});

        std::FILE* f = nullptr;
#if defined(_MSC_VER)
        (void)fopen_s(&f, path, "wb");
#else
        f = std::fopen(path, "wb");
#endif
        if (f == nullptr) {
            return false;
        }
        const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
        std::fclose(f);
        return ok;
    }

private:
    static void put32(std::vector<std::uint8_t>& v, std::uint32_t x) {
        v.push_back(static_cast<std::uint8_t>(x >> 24));
        v.push_back(static_cast<std::uint8_t>(x >> 16));
        v.push_back(static_cast<std::uint8_t>(x >> 8));
        v.push_back(static_cast<std::uint8_t>(x));
    }

    static const std::array<std::uint32_t, 256>& crc_table() {
        static const std::array<std::uint32_t, 256> table = [] {
            std::array<std::uint32_t, 256> t{};
            for (std::uint32_t n = 0; n < 256; ++n) {
                std::uint32_t c = n;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1u) != 0u ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                }
                t[n] = c;
            }
            return t;
        }();
        return table;
    }

    static std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0xFFFFFFFFu) {
        const auto& t = crc_table();
        for (std::size_t i = 0; i < size; ++i) {
            crc = t[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
        }
        return crc;
    }

    static std::uint32_t adler32(const std::uint8_t* data, std::size_t size) {
        std::uint32_t a = 1;
        std::uint32_t b = 0;
        for (std::size_t i = 0; i < size; ++i) {
            a = (a + data[i]) % 65521u;
            b = (b + a) % 65521u;
        }
        return (b << 16) | a;
    }

    static void chunk(std::vector<std::uint8_t>& out, const char* type,
                      const std::vector<std::uint8_t>& data) {
        put32(out, static_cast<std::uint32_t>(data.size()));
        const std::size_t typeStart = out.size();
        out.insert(out.end(), type, type + 4);
        out.insert(out.end(), data.begin(), data.end());
        const std::uint32_t crc = crc32(out.data() + typeStart, out.size() - typeStart) ^ 0xFFFFFFFFu;
        put32(out, crc);
    }
};

} // namespace svo_render
