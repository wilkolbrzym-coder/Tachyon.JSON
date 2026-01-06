#ifndef TACHYON_HPP
#define TACHYON_HPP

// Tachyon JSON Library v7.0 FINAL FUSION
// "The Performance Singularity"

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <cstring>
#include <immintrin.h>
#include <bit>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <memory>
#include <charconv>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

// Branch Prediction
#define TACHYON_LIKELY(x) __builtin_expect(!!(x), 1)
#define TACHYON_UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace Tachyon {

    class Document {
    public:
        std::string storage;
        const char* base = nullptr;
        size_t len = 0;

        // Aligned Bitmask Buffer
        uint64_t* bitmask_ptr = nullptr;
        size_t bitmask_cap = 0;
        size_t bitmask_sz = 0;

        Document() {
            allocate_bitmask(1024);
        }

        ~Document() {
            if (bitmask_ptr) std::free(bitmask_ptr);
        }

        void allocate_bitmask(size_t size) {
            if (size <= bitmask_cap) return;
            if (bitmask_ptr) std::free(bitmask_ptr);
            size_t bytes = size * sizeof(uint64_t);
            bytes = (bytes + 31) & ~31;
            bitmask_ptr = (uint64_t*)std::aligned_alloc(32, bytes);
            bitmask_cap = size;
        }

        void parse(std::string&& s) {
            storage = std::move(s);
            // Padding for safe over-read (256 bytes for aggressive unrolling)
            if (storage.capacity() < storage.size() + 256) {
                storage.reserve(storage.size() + 256);
            }
            // Zero-init padding
            std::memset(storage.data() + storage.size(), 0, storage.capacity() - storage.size());

            base = storage.data();
            len = storage.size();
            build_mask_lut();
        }

        void parse_view(const char* data, size_t size) {
            base = data;
            len = size;
            build_mask_lut();
        }

    private:
        void build_mask_lut() {
            size_t u64_count = (len + 63) / 64;
            allocate_bitmask(u64_count + 8);
            bitmask_sz = u64_count;

            uint64_t* m_ptr = bitmask_ptr;
            const char* data = base;
            size_t i = 0;
            uint64_t in_string = 0;

            // Constants
            const __m256i v_quote = _mm256_set1_epi8('"');
            const __m256i v_bs = _mm256_set1_epi8('\\');
            const __m256i v_0f = _mm256_set1_epi8(0x0F);

            // LUTs
            const __m256i v_lut_lo = _mm256_setr_epi8(
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, 0,
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, 0
            );

            const __m256i v_lut_hi = _mm256_setr_epi8(
                0, 0, -1, -1, 0, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, -1, -1, 0, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0
            );

            size_t len_safe = len;

            // Unrolled 4x (256 bytes)
            while (i + 256 <= len_safe) {
                _mm_prefetch(data + i + 1024, _MM_HINT_T0);

                auto process = [&](__m256i chunk) __attribute__((always_inline)) {
                    uint32_t mask_q = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_quote));
                    uint32_t mask_bs = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_bs));

                    if (TACHYON_UNLIKELY(mask_bs)) {
                         uint32_t q = mask_q;
                         while(q) {
                             int idx = std::countr_zero(q);
                             q &= ~(1U << idx);
                             int b_count = 0;
                             for (int k = idx - 1; k >= 0; --k) {
                                 if ((mask_bs >> k) & 1) b_count++; else break;
                             }
                             if (b_count % 2 != 0) mask_q &= ~(1U << idx);
                         }
                    }

                    uint64_t mq64 = mask_q;
                    uint64_t prefix = mq64 ^ (mq64 << 1);
                    prefix ^= (prefix << 2); prefix ^= (prefix << 4); prefix ^= (prefix << 8); prefix ^= (prefix << 16); prefix ^= (prefix << 32);
                    uint64_t s_mask = prefix ^ in_string;
                    in_string = (std::popcount(mq64) & 1) ? ~in_string : in_string;

                    // Nibble Lookup
                    __m256i lo = _mm256_and_si256(chunk, v_0f);
                    __m256i hi = _mm256_and_si256(_mm256_srli_epi16(chunk, 4), v_0f);

                    __m256i m_lo = _mm256_shuffle_epi8(v_lut_lo, lo);
                    __m256i m_hi = _mm256_shuffle_epi8(v_lut_hi, hi);

                    __m256i m_struct = _mm256_and_si256(m_lo, m_hi);

                    uint32_t str_mask = _mm256_movemask_epi8(m_struct);

                    return (str_mask & ~s_mask) | mask_q;
                };

                uint64_t r0 = process(_mm256_loadu_si256((const __m256i*)(data + i)));
                uint64_t r1 = process(_mm256_loadu_si256((const __m256i*)(data + i + 64)));
                uint64_t r2 = process(_mm256_loadu_si256((const __m256i*)(data + i + 128)));
                uint64_t r3 = process(_mm256_loadu_si256((const __m256i*)(data + i + 192)));

                __m256i vec0 = _mm256_set_epi64x(r3, r2, r1, r0);
                __asm__ volatile ("vmovntdq %[val], (%[addr])" : : [val] "x" (vec0), [addr] "r" (&m_ptr[i/64]) : "memory");

                i += 256;
            }

            // Tail
            while (i < len) {
                 if (i + 64 <= len + 256) {
                     uint64_t mask = 0;
                     for (size_t k = 0; k < 64 && i + k < len; ++k) {
                         char c = data[i+k];
                         bool is_s = false;
                         if (c == '"') {
                             size_t bs = 0;
                             size_t b_idx = i + k;
                             while (b_idx > 0 && data[b_idx-1] == '\\') { bs++; b_idx--; }
                             if (bs % 2 == 0) { in_string = ~in_string; is_s = true; }
                         } else if (in_string == 0) {
                             if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',' ||
                                 c == 't' || c == 'f' || c == 'n' || c == '-' || (c >= '0' && c <= '9')) {
                                 is_s = true;
                             }
                         }
                         if (is_s) mask |= (1ULL << k);
                     }
                     m_ptr[i/64] = mask;
                 }
                 i += 64;
            }
            _mm_sfence();
        }
    };

    class json {
        std::shared_ptr<Document> doc;
        size_t bit_index;

    public:
        json() : doc(nullptr), bit_index(0) {}
        json(std::shared_ptr<Document> d, size_t idx) : doc(d), bit_index(idx) {}

        static json parse(std::string s) {
            auto d = std::make_shared<Document>();
            d->parse(std::move(s));
            return json(d, find_next(d, 0));
        }

        static json parse_view(const char* ptr, size_t len) {
             auto d = std::make_shared<Document>();
             d->parse_view(ptr, len);
             return json(d, find_next(d, 0));
        }

        static size_t find_next(const std::shared_ptr<Document>& d, size_t start) {
            size_t block = start / 64;
            if (block >= d->bitmask_sz) return std::string::npos;

            uint64_t mask = d->bitmask_ptr[block];
            mask &= ~((1ULL << (start % 64)) - 1);

            while (mask == 0) {
                block++;
                if (block >= d->bitmask_sz) return std::string::npos;
                mask = d->bitmask_ptr[block];
            }
            return block * 64 + std::countr_zero(mask);
        }

        char get_char() const {
            if (!doc || bit_index >= doc->len) return 0;
            return doc->base[bit_index];
        }

        bool is_null() const {
            char c = get_char();
            return c == 'n' || c == 0;
        }
        bool is_object() const { return get_char() == '{'; }
        bool is_array() const { return get_char() == '['; }
        bool is_string() const { return get_char() == '"'; }
        bool is_number() const { char c = get_char(); return (c >= '0' && c <= '9') || c == '-'; }
        bool is_boolean() const { char c = get_char(); return c == 't' || c == 'f'; }

        template<typename T> T get() const {
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int64_t>) return (T)as_int64();
            else if constexpr (std::is_same_v<T, double>) return as_double();
            else if constexpr (std::is_same_v<T, std::string>) return as_string();
            else if constexpr (std::is_same_v<T, bool>) return as_bool();
            return T();
        }

        int64_t as_int64() const {
            if (!doc) return 0;
            int64_t i;
            std::from_chars(doc->base + bit_index, doc->base + doc->len, i);
            return i;
        }

        double as_double() const {
            if (!doc) return 0.0;
            double d;
            std::from_chars(doc->base + bit_index, doc->base + doc->len, d, std::chars_format::general);
            return d;
        }

        std::string as_string() const {
            if (!doc || !is_string()) return "";
            const char* s = doc->base + bit_index + 1;
            const char* end = s;

            while (true) {
                 if (*end == '"') {
                     size_t bs = 0;
                     const char* b = end - 1;
                     while(b >= s && *b == '\\') { bs++; b--; }
                     if (bs % 2 == 0) break;
                 }
                 end++;
            }

            bool has_escapes = false;
            for(const char* p = s; p < end; ++p) { if(*p == '\\') { has_escapes = true; break; } }
            if(!has_escapes) return std::string(s, end - s);
            return unescape(s, end - s);
        }

        std::string unescape(const char* s, size_t len) const {
            std::string res;
            res.reserve(len);
            for (size_t i = 0; i < len; ++i) {
                if (s[i] == '\\' && i + 1 < len) {
                    char c = s[++i];
                    if (c == '"') res += '"';
                    else if (c == '\\') res += '\\';
                    else if (c == '/') res += '/';
                    else if (c == 'b') res += '\b';
                    else if (c == 'f') res += '\f';
                    else if (c == 'n') res += '\n';
                    else if (c == 'r') res += '\r';
                    else if (c == 't') res += '\t';
                    else if (c == 'u' && i + 4 < len) {
                        unsigned int code;
                        std::from_chars(s + i + 1, s + i + 5, code, 16);
                        res += (char)code;
                        i += 4;
                    } else res += c;
                } else {
                    res += s[i];
                }
            }
            return res;
        }

        bool as_bool() const {
            return get_char() == 't';
        }

        json operator[](const std::string& key) const {
             if (!is_object()) return json();
             size_t cur = bit_index + 1;
             cur = find_next(doc, cur);

             while (cur != std::string::npos) {
                 char c = doc->base[cur];
                 if (c == '}') return json();

                 if (c == '"') {
                     const char* k_ptr = doc->base + cur + 1;
                     bool match = true;
                     for(size_t j=0; j<key.size(); ++j) {
                         if (k_ptr[j] != key[j]) { match = false; break; }
                     }
                     if (match && k_ptr[key.size()] == '"') {
                         cur = find_next(doc, cur + 1);
                         cur = find_next(doc, cur + 1);
                         cur = find_next(doc, cur + 1);
                         return json(doc, cur);
                     }
                 }

                 cur = find_next(doc, cur + 1);
                 cur = find_next(doc, cur + 1);
                 cur = find_next(doc, cur + 1);

                 cur = skip_value(cur);
                 if (doc->base[cur] == ',') cur = find_next(doc, cur + 1);
             }
             return json();
        }

        size_t skip_value(size_t cur) const {
            char c = doc->base[cur];
            if (c == '{') {
                int depth = 1;
                while (depth > 0) {
                    cur = find_next(doc, cur + 1);
                    if (doc->base[cur] == '{') depth++;
                    else if (doc->base[cur] == '}') depth--;
                }
                return find_next(doc, cur + 1);
            } else if (c == '[') {
                int depth = 1;
                while (depth > 0) {
                    cur = find_next(doc, cur + 1);
                    if (doc->base[cur] == '[') depth++;
                    else if (doc->base[cur] == ']') depth--;
                }
                return find_next(doc, cur + 1);
            } else if (c == '"') {
                cur = find_next(doc, cur + 1); // Close Quote
                return find_next(doc, cur + 1); // Next token (comma/end)
            } else {
                return find_next(doc, cur + 1);
            }
        }

        json operator[](size_t idx) const {
            if (!is_array()) return json();
            size_t cur = find_next(doc, bit_index + 1);
            size_t count = 0;

            while (cur != std::string::npos) {
                if (doc->base[cur] == ']') return json();

                if (count == idx) return json(doc, cur);

                cur = skip_value(cur);
                if (doc->base[cur] == ',') cur = find_next(doc, cur + 1);
                count++;
            }
            return json();
        }

        size_t size() const {
            if (!is_array()) return 0;
            size_t cur = find_next(doc, bit_index + 1);
            size_t count = 0;
            while (cur != std::string::npos) {
                if (doc->base[cur] == ']') break;
                count++;
                cur = skip_value(cur);
                if (doc->base[cur] == ',') cur = find_next(doc, cur + 1);
            }
            return count;
        }
    };

    #define TACHYON_DEFINE_TYPE_NON_INTRUSIVE(Type, ...)
}
#endif
