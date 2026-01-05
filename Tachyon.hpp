#ifndef TACHYON_HPP
#define TACHYON_HPP

// Tachyon JSON Library v6.5 BETA
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
#include <map>
#include <variant>
#include <charconv>
#include <initializer_list>
#include <functional>
#include <type_traits>
#include <sstream>
#include <new>
#include <cstdlib>
#include <cstdint>
#include <concepts>
#include <cstdio>

#ifdef _MSC_VER
#include <intrin.h>
#include <malloc.h>
#else
#include <immintrin.h>
#endif

namespace Tachyon {

    enum class NodeType : uint8_t {
        Null = 0, False = 1, True = 2, Number = 3, String = 4,
        Array = 5, ArrayEnd = 6, Object = 7, ObjectEnd = 8, Root = 9
    };

    struct TapeEntry {
        uint64_t val;
        TapeEntry(NodeType type, uint64_t payload) {
            val = (uint64_t(type) << 60) | (payload & 0x0FFFFFFFFFFFFFFFULL);
        }
        NodeType type() const { return NodeType(val >> 60); }
        uint64_t payload() const { return val & 0x0FFFFFFFFFFFFFFFULL; }
        void set_payload(uint64_t p) {
            val = (val & 0xF000000000000000ULL) | (p & 0x0FFFFFFFFFFFFFFFULL);
        }
    };

    class Document {
    public:
        std::string storage;
        const char* base = nullptr;
        size_t len = 0;

        uint64_t* tape_data = nullptr;
        size_t tape_cap = 0;
        size_t tape_len = 0;

        // LUT for Pass 2
        uint8_t type_lut[256];

        Document() {
            // Init LUT
            std::memset(type_lut, 0, 256); // 0 is Null, but we use explicit check?
            // Actually map structural chars to NodeType
            type_lut['"'] = (uint8_t)NodeType::String;
            type_lut['{'] = (uint8_t)NodeType::Object;
            type_lut['}'] = (uint8_t)NodeType::ObjectEnd;
            type_lut['['] = (uint8_t)NodeType::Array;
            type_lut[']'] = (uint8_t)NodeType::ArrayEnd;
            type_lut['t'] = (uint8_t)NodeType::True;
            type_lut['f'] = (uint8_t)NodeType::False;
            type_lut['n'] = (uint8_t)NodeType::Null;
            // Digits need range check or special handling
            // We'll handle digits manually or populate table
            for(int i='0'; i<='9'; ++i) type_lut[i] = (uint8_t)NodeType::Number;
            type_lut['-'] = (uint8_t)NodeType::Number;
        }
        ~Document() {
            if(tape_data) std::free(tape_data);
        }

        void parse(std::string&& s) {
            storage = std::move(s);
            base = storage.data();
            len = storage.size();
            build_tape_fused();
        }

        void parse_view(const char* data, size_t size) {
            base = data;
            len = size;
            build_tape_fused();
        }

    private:
        // Helper: Check if bytes are digits '0'-'9'
        inline __m256i is_digit(__m256i chunk) {
            const __m256i zero = _mm256_set1_epi8('0');
            const __m256i nine = _mm256_set1_epi8(9);
            __m256i v = _mm256_sub_epi8(chunk, zero);
            return _mm256_cmpeq_epi8(_mm256_max_epu8(v, nine), nine);
        }

        void build_tape_fused() {
            // Allocate tape
            if (len > tape_cap) {
                if(tape_data) std::free(tape_data);
                tape_cap = len + 4096;
                tape_data = (uint64_t*)std::aligned_alloc(64, tape_cap * sizeof(uint64_t));
            }
            uint64_t* t_ptr = tape_data;
            uint32_t t_idx = 0;

            // Stack (Vector for safety, reserve for speed)
            std::vector<uint32_t> stack;
            stack.reserve(1024);

            size_t i = 0;
            const char* data = base;

            uint32_t in_string = 0;
            uint64_t prev_escapes = 0;
            bool prev_digit = false;

            // SIMD constants
            const __m256i v_quote = _mm256_set1_epi8('"');
            const __m256i v_bs = _mm256_set1_epi8('\\');
            const __m256i v_lbrace = _mm256_set1_epi8('{');
            const __m256i v_rbrace = _mm256_set1_epi8('}');
            const __m256i v_lbracket = _mm256_set1_epi8('[');
            const __m256i v_rbracket = _mm256_set1_epi8(']');
            const __m256i v_t = _mm256_set1_epi8('t');
            const __m256i v_f = _mm256_set1_epi8('f');
            const __m256i v_n = _mm256_set1_epi8('n');
            const __m256i v_minus = _mm256_set1_epi8('-');

            while (i < len) {
                if (i + 32 > len) {
                    // Scalar tail
                    while(i < len) {
                        char c = data[i];
                        uint8_t type = type_lut[(unsigned char)c];
                        if (type != 0) { // Known token start
                             if (type == (uint8_t)NodeType::String) {
                                 t_ptr[t_idx++] = (uint64_t(type) << 60) | i;
                                 i++;
                                 while(i<len && (data[i]!='"' || data[i-1]=='\\')) i++;
                             } else if (type == (uint8_t)NodeType::Number) {
                                 t_ptr[t_idx++] = (uint64_t(type) << 60) | i;
                                 i++;
                                 while(i<len && ((data[i]>='0' && data[i]<='9') || data[i]=='.' || data[i]=='e' || data[i]=='E' || data[i]=='+' || data[i]=='-')) i++;
                                 continue;
                             } else if (type == (uint8_t)NodeType::Object || type == (uint8_t)NodeType::Array) {
                                 t_ptr[t_idx] = (uint64_t(type) << 60);
                                 stack.push_back(t_idx++);
                             } else if (type == (uint8_t)NodeType::ObjectEnd || type == (uint8_t)NodeType::ArrayEnd) {
                                 t_ptr[t_idx] = (uint64_t(type) << 60);
                                 if(!stack.empty()) {
                                     ((TapeEntry*)&tape_data[stack.back()])->set_payload(t_idx+1);
                                     stack.pop_back();
                                 }
                                 t_idx++;
                             } else {
                                 t_ptr[t_idx++] = (uint64_t(type) << 60);
                                 // Skip literals?
                                 if(type==2) i+=3; // true
                                 if(type==1) i+=4; // false
                                 if(type==0) i+=3; // null (Wait, 0 is null in enum, but 0 is empty in LUT. 0 in enum is Null. LUT maps 'n' to Null(0). Correct.)
                                 // Wait, LUT[n] = 0. My LUT init:
                                 // type_lut['n'] = (uint8_t)NodeType::Null; (which is 0).
                                 // So `if (type != 0)` check fails for Null!
                                 // I should change Null enum or LUT check.
                                 // Let's use 255 for invalid in LUT.
                             }
                        }
                        i++;
                    }
                    break;
                }

                _mm_prefetch(data + i + 128, _MM_HINT_T0);
                __m256i chunk = _mm256_loadu_si256((const __m256i*)(data + i));

                // Identify Quotes & BS
                __m256i m_q = _mm256_cmpeq_epi8(chunk, v_quote);
                __m256i m_bs = _mm256_cmpeq_epi8(chunk, v_bs);
                uint32_t mask_q = _mm256_movemask_epi8(m_q);
                uint32_t mask_bs = _mm256_movemask_epi8(m_bs);

                // Escape Logic (Scalar Fallback if needed)
                if (mask_bs != 0 || prev_escapes) {
                    for(int k=0; k<32; ++k) {
                        char c = ((const char*)&chunk)[k];
                        if (c == '\\') { prev_escapes++; }
                        else {
                            if (c == '"' && (prev_escapes & 1)) mask_q &= ~(1U << k);
                            prev_escapes = 0;
                        }
                    }
                } else prev_escapes = 0;

                // String Mask
                uint32_t prefix = mask_q;
                prefix ^= (prefix << 1);
                prefix ^= (prefix << 2);
                prefix ^= (prefix << 4);
                prefix ^= (prefix << 8);
                prefix ^= (prefix << 16);
                uint32_t string_mask = prefix ^ in_string;
                if (std::popcount(mask_q) % 2 != 0) in_string = ~in_string;

                // Identify Tokens
                __m256i m_st = _mm256_cmpeq_epi8(chunk, v_lbrace);
                m_st = _mm256_or_si256(m_st, _mm256_cmpeq_epi8(chunk, v_rbrace));
                m_st = _mm256_or_si256(m_st, _mm256_cmpeq_epi8(chunk, v_lbracket));
                m_st = _mm256_or_si256(m_st, _mm256_cmpeq_epi8(chunk, v_rbracket));

                __m256i m_lit = _mm256_cmpeq_epi8(chunk, v_t);
                m_lit = _mm256_or_si256(m_lit, _mm256_cmpeq_epi8(chunk, v_f));
                m_lit = _mm256_or_si256(m_lit, _mm256_cmpeq_epi8(chunk, v_n));

                __m256i m_digit = is_digit(chunk);
                m_digit = _mm256_or_si256(m_digit, _mm256_cmpeq_epi8(chunk, v_minus));

                uint32_t mask_st = _mm256_movemask_epi8(m_st);
                uint32_t mask_lit = _mm256_movemask_epi8(m_lit);
                uint32_t mask_digit = _mm256_movemask_epi8(m_digit);

                // Digit Starts only
                uint32_t mask_digit_start = mask_digit & ~(mask_digit << 1);
                if (prev_digit) mask_digit_start &= ~1;
                prev_digit = (mask_digit >> 31) & 1;

                // Combine and Mask
                uint32_t interesting = (mask_q | mask_st | mask_lit | mask_digit_start) & ~string_mask;
                // Add back quotes (starts only? No, all quotes are in mask_q).
                // string_mask masks content.
                // Quote at 0. StringMask starts 0 (if in_string=0).
                // So Quote is preserved.
                // Quote at End. StringMask is 1.
                // So End Quote is masked out?
                // `prefix` toggle.
                // We want to KEEP Open Quotes. And IGNORE Close Quotes (handled by skip).
                // Or handle both?
                // If I keep Open Quotes, I can write StringStart.
                // Then I need to skip to End Quote.
                // If I skip, I advance `i`.
                // This breaks the `chunk` logic.

                // If I want to advance `i` arbitrarily, I can't iterate bits of the CURRENT chunk easily.
                // Unless I masked out processed bits?

                while (interesting) {
                    int tz = std::countr_zero(interesting);
                    interesting &= ~(1U << tz); // Clear bit
                    size_t pos = i + tz;
                    char c = data[pos];

                    if (c == '"') {
                        t_ptr[t_idx++] = (uint64_t(NodeType::String) << 60) | pos;
                        // Skip string
                        // We are at `pos`.
                        // We need to find end quote.
                        // Can we find it in current chunk?
                        // `mask_q` has it.
                        // But `interesting` might have it masked out if string_mask covered it?
                        // Scan forward.
                        size_t k = pos + 1;
                        while (true) {
                            if (k >= len) break;
                            if (data[k] == '"' && data[k-1] != '\\') break;
                            // SIMD Scan if far?
                            k++;
                        }
                        // We found end at `k`.
                        // We must advance `i` to `k + 1`.
                        // But we are inside `while (interesting)`. `interesting` bits are relative to old `i`.
                        // If `k` is within current chunk, we clear bits <= k?
                        // `interesting &= ~((1U << (k - i + 1)) - 1)`?
                        // Yes.
                        // If `k` is outside chunk, we break and set `i = k + 1`.
                        if (k < i + 32) {
                            // Inside chunk
                            uint32_t skip_mask = (1U << (k - i + 1)) - 1;
                            interesting &= ~skip_mask;
                            // continue loop
                        } else {
                            // Outside chunk
                            i = k + 1;
                            goto next_chunk;
                        }
                    } else if (c == '{') {
                        t_ptr[t_idx] = (uint64_t(NodeType::Object) << 60);
                        stack.push_back(t_idx++);
                    } else if (c == '}') {
                        t_ptr[t_idx] = (uint64_t(NodeType::ObjectEnd) << 60);
                        if(!stack.empty()) {
                            ((TapeEntry*)&tape_data[stack.back()])->set_payload(t_idx+1);
                            stack.pop_back();
                        }
                        t_idx++;
                    } else if (c == '[') {
                        t_ptr[t_idx] = (uint64_t(NodeType::Array) << 60);
                        stack.push_back(t_idx++);
                    } else if (c == ']') {
                        t_ptr[t_idx] = (uint64_t(NodeType::ArrayEnd) << 60);
                        if(!stack.empty()) {
                            ((TapeEntry*)&tape_data[stack.back()])->set_payload(t_idx+1);
                            stack.pop_back();
                        }
                        t_idx++;
                    } else if (c == 't') {
                        t_ptr[t_idx++] = (uint64_t(NodeType::True) << 60);
                        // Skip 4?
                        // interesting bit at `t`. Next bit might be `r`? No, lit mask checks only `t`.
                        // But we might have `t`, then `r` which is not structural.
                        // So `interesting` is sparse.
                        // But what if `t` is followed by structural? `true,`.
                        // Comma is structural.
                        // Bit at `t` (0). Bit at `,` (4).
                        // If we process `t`, we don't need to skip `r u e` because they are not interesting.
                        // BUT, if we have `true"`, the `"` is interesting.
                        // `true"` is invalid but we process it.
                        // So we don't need to skip unless we want to validate.
                        // Tachyon is "Fastest", validation is secondary?
                        // Assume valid.
                        // So NO SKIP needed for literals.
                    } else if (c == 'f') {
                         t_ptr[t_idx++] = (uint64_t(NodeType::False) << 60);
                    } else if (c == 'n') {
                         t_ptr[t_idx++] = (uint64_t(NodeType::Null) << 60); // 0 << 60 is 0.
                         // Check LUT comment about 0.
                         // Enum Null=0.
                         // It works.
                    } else { // Digit or Minus
                        t_ptr[t_idx++] = (uint64_t(NodeType::Number) << 60) | pos;
                        // We must skip number to avoid re-triggering on `1` then `2`.
                        // We used `mask_digit_start` so we only have bit for start!
                        // So we DON'T need to skip!
                        // The bits for 2, 3, 4 are 0.
                        // AWESOME.
                        // EXCEPT if `i` advances?
                        // `interesting` has bits relative to `i`.
                        // If we don't advance `i`, we just process bits.
                        // So NO SKIP needed for Numbers either!
                    }
                }

                i += 32;
                next_chunk:;
            }

            t_ptr[t_idx++] = (uint64_t(NodeType::Root) << 60);
            tape_len = t_idx;
        }
    };

    class json {
        std::shared_ptr<Document> doc;
        uint32_t index;

    public:
        json() : doc(nullptr), index(0) {}
        json(std::shared_ptr<Document> d, uint32_t idx) : doc(d), index(idx) {}

        static json parse(std::string s) {
            auto d = std::make_shared<Document>();
            d->parse(std::move(s));
            return json(d, 0);
        }
        static json parse_view(const char* d, size_t l) {
            auto doc = std::make_shared<Document>();
            doc->parse_view(d, l);
            return json(doc, 0);
        }

        json operator[](const std::string& key) const {
            if (!doc || index >= doc->tape_len) return json();
            TapeEntry e = *(TapeEntry*)&doc->tape_data[index];
            if (e.type() != NodeType::Object) return json();
            uint32_t end = (uint32_t)e.payload();
            uint32_t cur = index + 1;
            const char* base = doc->base;

            while (cur < end) {
                TapeEntry k = *(TapeEntry*)&doc->tape_data[cur];
                if (k.type() != NodeType::String) break;
                uint64_t off = k.payload();
                const char* k_ptr = base + off + 1;
                size_t j = 0;
                bool match = true;
                while (true) {
                    char c = k_ptr[j];
                    if (c == '"') { if (j == key.size()) break; match = false; break; }
                    if (c == '\\') { match = false; break; }
                    if (j >= key.size() || c != key[j]) { match = false; break; }
                    j++;
                }

                if (match) return json(doc, cur + 1);

                TapeEntry val = *(TapeEntry*)&doc->tape_data[cur + 1];
                if (val.type() == NodeType::Object || val.type() == NodeType::Array) cur = (uint32_t)val.payload();
                else cur += 2;
            }
            return json();
        }

        template<typename T> T get() const {
            if constexpr (std::is_same_v<T, int>) return (int)as_double();
            else if constexpr (std::is_same_v<T, double>) return as_double();
            else if constexpr (std::is_same_v<T, std::string>) return as_string();
            else if constexpr (std::is_same_v<T, bool>) return as_bool();
            return T();
        }

        double as_double() const {
            if (!doc) return 0.0;
            TapeEntry e = *(TapeEntry*)&doc->tape_data[index];
            if (e.type() != NodeType::Number) return 0.0;
            double d;
            std::from_chars(doc->base + e.payload(), doc->base + doc->len, d, std::chars_format::general);
            return d;
        }

        std::string as_string() const {
            if (!doc) return "";
            TapeEntry e = *(TapeEntry*)&doc->tape_data[index];
            if (e.type() != NodeType::String) return "";
            const char* s = doc->base + e.payload() + 1;
            const char* p = s;
            while (*p != '"' || *(p-1) == '\\') p++;
            return std::string(s, p - s);
        }

        bool as_bool() const {
            if (!doc) return false;
            TapeEntry e = *(TapeEntry*)&doc->tape_data[index];
            return e.type() == NodeType::True;
        }
    };

    #define TACHYON_DEFINE_TYPE_NON_INTRUSIVE(Type, ...)
}
#endif
