#ifndef SHA1_HPP
#define SHA1_HPP

#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>

class SHA1 {
private:
    uint32_t digest[5];
    uint8_t buffer[64];
    uint64_t transforms;
    uint64_t total_bytes;

    static constexpr uint32_t K[] = {
        0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
    };

    static inline uint32_t rol(uint32_t value, size_t bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    void process_block(const uint8_t* block) {
        uint32_t w[80];
        uint32_t a, b, c, d, e, temp;

        // Initialize hash value for this block
        a = digest[0];
        b = digest[1];
        c = digest[2];
        d = digest[3];
        e = digest[4];

        // Convert block to 32-bit words (big-endian)
        for (size_t i = 0; i < 16; ++i) {
            w[i] = (block[i*4] << 24) |
                    (block[i*4+1] << 16) |
                    (block[i*4+2] << 8) |
                    block[i*4+3];
        }

        // Extend to 80 words
        for (size_t i = 16; i < 80; ++i) {
            w[i] = rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        }

        // Main computation loop
        for (size_t i = 0; i < 80; ++i) {
            uint32_t f, k;

            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = K[0];
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = K[1];
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = K[2];
            } else {
                f = b ^ c ^ d;
                k = K[3];
            }

            temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }

        // Update digest
        digest[0] += a;
        digest[1] += b;
        digest[2] += c;
        digest[3] += d;
        digest[4] += e;
    }

    void pad() {
        size_t pad_len = (total_bytes % 64 < 56) ? 
                         (56 - total_bytes % 64) : 
                         (120 - total_bytes % 64);

        update(std::string(1, 0x80) + std::string(pad_len - 1, 0x00));

        // Append length in bits (big-endian)
        uint64_t bit_length = total_bytes * 8;
        uint8_t length_bytes[8];
        for (int i = 7; i >= 0; --i) {
            length_bytes[i] = bit_length & 0xFF;
            bit_length >>= 8;
        }
        update(reinterpret_cast<char*>(length_bytes), 8);
    }

public:
    SHA1() {
        reset();
    }

    void reset() {
        digest[0] = 0x67452301;
        digest[1] = 0xEFCDAB89;
        digest[2] = 0x98BADCFE;
        digest[3] = 0x10325476;
        digest[4] = 0xC3D2E1F0;
        transforms = 0;
        total_bytes = 0;
        std::fill(buffer, buffer + 64, 0);
    }

    void update(const std::string &s) {
        update(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    void update(const char* data, size_t len) {
        update(reinterpret_cast<const uint8_t*>(data), len);
    }

    void update(const uint8_t* data, size_t len) {
        total_bytes += len;
        size_t offset = transforms % 64;

        // Process full blocks first
        if (offset + len >= 64) {
            size_t copy_len = 64 - offset;
            std::copy(data, data + copy_len, buffer + offset);
            process_block(buffer);
            transforms++;
            len -= copy_len;
            data += copy_len;
            offset = 0;
        }

        // Process remaining full blocks
        while (len >= 64) {
            std::copy(data, data + 64, buffer);
            process_block(buffer);
            transforms++;
            len -= 64;
            data += 64;
        }

        // Store remaining bytes
        std::copy(data, data + len, buffer + offset);
    }

    std::string final() {
        pad();
        if (total_bytes % 64 != 0) {
            process_block(buffer);
        }

        std::ostringstream result;
        for (int i = 0; i < 5; ++i) {
            result << std::hex << std::setfill('0') << std::setw(8) 
                   << digest[i];
        }
        reset();
        return result.str();
    }

    static std::string from_file(const std::string &filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) return "";

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        SHA1 sha;
        std::vector<uint8_t> buffer(1024 * 1024);   // 1MB buffer
        
        while (size > 0) {
            size_t read_size = static_cast<size_t>(std::min<std::streamsize>(size, buffer.size()));
            file.read(reinterpret_cast<char*>(buffer.data()), read_size);
            sha.update(buffer.data(), read_size);
            size -= read_size;
        }

        return sha.final();
    }
};

#endif // SHA1_HPP