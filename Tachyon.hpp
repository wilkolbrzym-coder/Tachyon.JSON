#ifndef TACHYON_HPP
#define TACHYON_HPP

// Tachyon JSON Library v7.5 ALIGNED-TAPE
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
            // 32-byte alignment for AVX2
            size_t bytes = size * sizeof(uint64_t);
            // Round up to multiple of 32
            bytes = (bytes + 31) & ~31;
            bitmask_ptr = (uint64_t*)std::aligned_alloc(32, bytes);
            bitmask_cap = size;
        }

        void parse(std::string&& s) {
            storage = std::move(s);
            // Ensure padding for AVX2 (Safe over-read)
            if (storage.capacity() < storage.size() + 256) {
                storage.reserve(storage.size() + 256);
            }
            std::memset(storage.data() + storage.size(), 0, storage.capacity() - storage.size());

            base = storage.data();
            len = storage.size();
            build_mask_asm();
        }

        void parse_view(const char* data, size_t size) {
            base = data;
            len = size;
            build_mask_asm();
        }

    private:
        // Force inline for speed
        __attribute__((always_inline))
        inline uint64_t process_64_bytes(const char* ptr, uint64_t& string_state,
                                       const __m256i& v_quote, const __m256i& v_bs,
                                       const __m256i& v_lbrace, const __m256i& v_rbrace,
                                       const __m256i& v_lbracket, const __m256i& v_rbracket,
                                       const __m256i& v_colon, const __m256i& v_comma,
                                       const __m256i& v_t, const __m256i& v_f, const __m256i& v_n,
                                       const __m256i& v_minus, const __m256i& v_0) {

            auto process_32 = [&](__m256i chunk) -> uint32_t {
                uint32_t mask_quote = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_quote));
                uint32_t mask_bs = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_bs));

                if (TACHYON_UNLIKELY(mask_bs)) {
                    uint32_t q = mask_quote;
                    while(q) {
                        int idx = std::countr_zero(q);
                        q &= ~(1U << idx);
                        int b_count = 0;
                        for (int k = idx - 1; k >= 0; --k) {
                            if ((mask_bs >> k) & 1) b_count++;
                            else break;
                        }
                        if (b_count % 2 != 0) mask_quote &= ~(1U << idx);
                    }
                }

                uint64_t mask_q_64 = mask_quote;
                uint64_t prefix = mask_q_64;
                prefix ^= (prefix << 1);
                prefix ^= (prefix << 2);
                prefix ^= (prefix << 4);
                prefix ^= (prefix << 8);
                prefix ^= (prefix << 16);
                prefix ^= (prefix << 32);

                uint64_t string_mask = prefix ^ string_state;
                string_state = (std::popcount(mask_q_64) & 1) ? ~string_state : string_state;

                uint32_t m_brace = _mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, v_lbrace), _mm256_cmpeq_epi8(chunk, v_rbrace)));
                uint32_t m_bracket = _mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, v_lbracket), _mm256_cmpeq_epi8(chunk, v_rbracket)));
                uint32_t m_sep = _mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpeq_epi8(chunk, v_colon), _mm256_cmpeq_epi8(chunk, v_comma)));
                uint32_t m_lit = _mm256_movemask_epi8(_mm256_or_si256(
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk, v_t), _mm256_cmpeq_epi8(chunk, v_f)),
                    _mm256_cmpeq_epi8(chunk, v_n)
                ));
                __m256i sub = _mm256_sub_epi8(chunk, v_0);
                __m256i is_dig = _mm256_cmpeq_epi8(_mm256_max_epu8(sub, _mm256_set1_epi8(9)), _mm256_set1_epi8(9));
                uint32_t m_num = _mm256_movemask_epi8(_mm256_or_si256(is_dig, _mm256_cmpeq_epi8(chunk, v_minus)));

                uint32_t structural = m_brace | m_bracket | m_sep | m_lit | m_num | mask_quote;
                structural &= ~string_mask;
                structural |= mask_quote;
                return structural;
            };

            __m256i chunk1 = _mm256_loadu_si256((const __m256i*)ptr);
            __m256i chunk2 = _mm256_loadu_si256((const __m256i*)(ptr + 32));

            uint32_t m1 = process_32(chunk1);
            uint32_t m2 = process_32(chunk2);
            return ((uint64_t)m2 << 32) | m1;
        }

        void build_mask_asm() {
            bitmask_sz = 0;
            size_t u64_count = (len + 63) / 64;
            allocate_bitmask(u64_count + 8); // Padding for safety

            uint64_t* m_ptr = bitmask_ptr;
            const char* data = base;

            uint64_t in_string = 0;
            size_t i = 0;

            const __m256i v_quote = _mm256_set1_epi8('"');
            const __m256i v_bs = _mm256_set1_epi8('\\');
            const __m256i v_lbrace = _mm256_set1_epi8('{');
            const __m256i v_rbrace = _mm256_set1_epi8('}');
            const __m256i v_lbracket = _mm256_set1_epi8('[');
            const __m256i v_rbracket = _mm256_set1_epi8(']');
            const __m256i v_colon = _mm256_set1_epi8(':');
            const __m256i v_comma = _mm256_set1_epi8(',');
            const __m256i v_t = _mm256_set1_epi8('t');
            const __m256i v_f = _mm256_set1_epi8('f');
            const __m256i v_n = _mm256_set1_epi8('n');
            const __m256i v_minus = _mm256_set1_epi8('-');
            const __m256i v_0 = _mm256_set1_epi8('0');

            size_t len_safe = len; // Padded

            // 4x Loop Unrolling (256 bytes)
            while (i + 256 <= len_safe) {
                _mm_prefetch(data + i + 1024, _MM_HINT_T0);

                uint64_t r0 = process_64_bytes(data + i, in_string, v_quote, v_bs, v_lbrace, v_rbrace, v_lbracket, v_rbracket, v_colon, v_comma, v_t, v_f, v_n, v_minus, v_0);
                uint64_t r1 = process_64_bytes(data + i + 64, in_string, v_quote, v_bs, v_lbrace, v_rbrace, v_lbracket, v_rbracket, v_colon, v_comma, v_t, v_f, v_n, v_minus, v_0);
                uint64_t r2 = process_64_bytes(data + i + 128, in_string, v_quote, v_bs, v_lbrace, v_rbrace, v_lbracket, v_rbracket, v_colon, v_comma, v_t, v_f, v_n, v_minus, v_0);
                uint64_t r3 = process_64_bytes(data + i + 192, in_string, v_quote, v_bs, v_lbrace, v_rbrace, v_lbracket, v_rbracket, v_colon, v_comma, v_t, v_f, v_n, v_minus, v_0);

                __m256i vec0 = _mm256_set_epi64x(r3, r2, r1, r0);

                // Use vmovntdq via ASM (requires 32-byte aligned address)
                __asm__ volatile (
                    "vmovntdq %[val], (%[addr])"
                    :
                    : [val] "x" (vec0), [addr] "r" (&m_ptr[i/64])
                    : "memory"
                );

                i += 256;
            }

            // Tail
            while (i < len) {
                 if (i + 64 <= len + 256) {
                     m_ptr[i/64] = process_64_bytes(data + i, in_string, v_quote, v_bs, v_lbrace, v_rbrace, v_lbracket, v_rbracket, v_colon, v_comma, v_t, v_f, v_n, v_minus, v_0);
                 }
                 i += 64;
            }

            _mm_sfence();
            bitmask_sz = u64_count;
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

        bool is_null() const { return get_char() == 'n'; }
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
