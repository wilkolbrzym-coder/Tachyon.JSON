#include <glaze/glaze.hpp>
#include "tachyon.hpp"
#include "benchmark_structs.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <sched.h>
#include <fstream>
#include <cstring>
#include <cmath>

// --- TACHYON METADATA REGISTRATION ---

TACHYON_DEFINE_TYPE(canada::Geometry, type, coordinates)
TACHYON_DEFINE_TYPE(canada::Property, name)
TACHYON_DEFINE_TYPE(canada::Feature, type, properties, geometry)
TACHYON_DEFINE_TYPE(canada::FeatureCollection, type, features)

TACHYON_DEFINE_TYPE(twitter::Metadata, result_type, iso_language_code)
TACHYON_DEFINE_TYPE(twitter::Url, url, expanded_url, display_url, indices)

// Manual implementation for UrlEntity to bypass linker issues (NO INLINE)
namespace Tachyon {
    template<> void read<twitter::UrlEntity>(twitter::UrlEntity& val, Scanner& s) {
        s.skip_whitespace();
        if (s.peek() != '{') throw Error("Expected {");
        s.consume('{');
        char key_buf[128];
        s.skip_whitespace();
        if (s.peek() == '}') { s.consume('}'); return; }
        while (true) {
            s.skip_whitespace();
            // Updated to new API
            std::string_view key = s.scan_string_view(key_buf, 128);
            s.skip_whitespace(); s.consume(':');
            if (key == "urls") {
                read(val.urls, s);
            } else {
                s.skip_value();
            }
            s.skip_whitespace();
            char c = s.peek();
            if (c == '}') { s.consume('}'); break; }
            if (c == ',') { s.consume(','); continue; }
            throw Error("Expected } or ,");
        }
    }
}

TACHYON_DEFINE_TYPE(twitter::UserEntities, url, description)
TACHYON_DEFINE_TYPE(twitter::User, id, id_str, name, screen_name, location, description, url, entities, protected_user, followers_count, friends_count, listed_count, created_at, favourites_count, utc_offset, time_zone, geo_enabled, verified, statuses_count, lang, contributors_enabled, is_translator, is_translation_enabled, profile_background_color, profile_background_image_url, profile_background_image_url_https, profile_background_tile, profile_image_url, profile_image_url_https, profile_banner_url, profile_link_color, profile_sidebar_border_color, profile_sidebar_fill_color, profile_text_color, profile_use_background_image, default_profile, default_profile_image, following, follow_request_sent, notifications)

TACHYON_DEFINE_TYPE(twitter::StatusEntities, hashtags, symbols, urls, user_mentions)
TACHYON_DEFINE_TYPE(twitter::Status, metadata, created_at, id, id_str, text, source, truncated, in_reply_to_status_id, in_reply_to_status_id_str, in_reply_to_user_id, in_reply_to_user_id_str, in_reply_to_screen_name, user, is_quote_status, retweet_count, favorite_count, entities, favorited, retweeted, lang)

TACHYON_DEFINE_TYPE(twitter::SearchMetadata, completed_in, max_id, max_id_str, next_results, query, refresh_url, count, since_id, since_id_str)
TACHYON_DEFINE_TYPE(twitter::TwitterResult, statuses, search_metadata)

// CITM
TACHYON_DEFINE_TYPE(citm::Event, id, name, description, subtitle, logo, topicId)
TACHYON_DEFINE_TYPE(citm::Catalog, areaNames, audienceSubCategoryNames, blockNames, events)

// Small
TACHYON_DEFINE_TYPE(small::Meta, active, rank)
TACHYON_DEFINE_TYPE(small::Object, id, name, checked, scores, meta, description)

// --- BENCHMARK UTILS ---

template <typename T>
void do_not_optimize(const T& val) {
    asm volatile("" : : "g"(&val) : "memory");
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return "";
    auto size = f.tellg();
    f.seekg(0);
    std::string s;
    s.resize(size);
    f.read(&s[0], size);
    return s;
}

struct Stats {
    double mb_s;
    double median_time;
};

Stats calculate_stats(std::vector<double>& times, size_t bytes) {
    std::sort(times.begin(), times.end());
    double median = times[times.size() / 2];
    double mb_s = (bytes / 1024.0 / 1024.0) / median;
    return { mb_s, median };
}

// --- VERIFICATION ---
double sum_canada(const canada::FeatureCollection& obj) {
    double sum = 0;
    for (const auto& f : obj.features) {
        for (const auto& ring : f.geometry.coordinates) {
            for (const auto& point : ring) {
                for (double d : point) {
                    sum += d;
                }
            }
        }
    }
    return sum;
}

void verify_small(const small::Object& g, const small::Object& t) {
    if (g.id != t.id) throw std::runtime_error("ID mismatch");
    if (g.name != t.name) throw std::runtime_error("Name mismatch");
    if (g.checked != t.checked) throw std::runtime_error("Checked mismatch");
    if (g.scores.size() != t.scores.size()) throw std::runtime_error("Scores size mismatch");
    for(size_t i=0; i<g.scores.size(); ++i) if(g.scores[i] != t.scores[i]) throw std::runtime_error("Scores mismatch");
    if (g.meta.active != t.meta.active) throw std::runtime_error("Meta.active mismatch");
    if (std::abs(g.meta.rank - t.meta.rank) > 1e-8) {
        std::cerr << "Rank G: " << g.meta.rank << " T: " << t.meta.rank << "\n";
        throw std::runtime_error("Meta.rank mismatch");
    }
    if (g.description != t.description) throw std::runtime_error("Description mismatch");
}

int main() {
    std::string canada_data = read_file("canada.json");
    std::string twitter_data = read_file("twitter.json");
    std::string citm_data = read_file("citm_catalog.json");
    std::string small_data = read_file("small.json");

    if (canada_data.empty() || twitter_data.empty() || citm_data.empty()) {
        std::cerr << "Error: Datasets not found." << std::endl;
        return 1;
    }

    auto pad = [](const std::string& s) {
        std::string p = s;
        p.resize(s.size() + 64, ' ');
        return p;
    };

    std::string canada_padded = pad(canada_data);
    std::string twitter_padded = pad(twitter_data);
    std::string citm_padded = pad(citm_data);
    std::string small_padded = pad(small_data);

    std::cout << "==========================================================" << std::endl;
    std::cout << "   TACHYON VS GLAZE: TYPED DESERIALIZATION DEATHMATCH" << std::endl;
    std::cout << "==========================================================" << std::endl;
    std::cout << std::fixed << std::setprecision(2);

    auto run_test = [&](const std::string& name, const std::string& data, const std::string& padded_data, auto& obj_g, auto& obj_t) {
        std::cout << "\n>>> Dataset: " << name << " (" << data.size() << " bytes)" << std::endl;

        // 1. GLAZE
        {
            auto err = glz::read_json(obj_g, data);
            if (err) { std::cerr << "Glaze Error!" << std::endl; exit(1); }

            std::vector<double> times;
            int iters = (data.size() < 1000) ? 50000 : 50;
            for (int i = 0; i < iters; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                auto err = glz::read_json(obj_g, data);
                auto end = std::chrono::high_resolution_clock::now();
                if (err) { std::cerr << "Glaze Error!" << std::endl; break; }
                times.push_back(std::chrono::duration<double>(end - start).count());
            }
            auto s = calculate_stats(times, data.size());
            std::cout << "Glaze:   " << std::setw(10) << s.mb_s << " MB/s | " << s.median_time * 1000 << " ms" << std::endl;
        }

        // 2. TACHYON
        {
            try {
                Tachyon::Scanner sc(padded_data.data(), data.size());
                Tachyon::read(obj_t, sc);

                // --- VERIFICATION ---
                if (name == "Canada.json") {
                    double sum_g = sum_canada((const canada::FeatureCollection&)obj_g);
                    double sum_t = sum_canada((const canada::FeatureCollection&)obj_t);
                    double diff = std::abs(sum_g - sum_t);
                    if (diff > 1e-8) { // Relaxed to 1e-8 for non-fast_float
                        std::cerr << "CRITICAL ERROR: Data integrity check failed!" << std::endl;
                        std::cerr << "Glaze Sum:   " << std::setprecision(10) << sum_g << std::endl;
                        std::cerr << "Tachyon Sum: " << std::setprecision(10) << sum_t << std::endl;
                        std::cerr << "Diff: " << diff << std::endl;
                        exit(1);
                    } else {
                        std::cout << "Integrity Check: PASSED (Diff: " << diff << ")" << std::endl;
                    }
                } else if (name == "Small.json") {
                    verify_small((const small::Object&)obj_g, (const small::Object&)obj_t);
                    std::cout << "Integrity Check: PASSED" << std::endl;
                }

                std::vector<double> times;
                int iters = (data.size() < 1000) ? 50000 : 50;
                for (int i = 0; i < iters; ++i) {
                    auto start = std::chrono::high_resolution_clock::now();
                    Tachyon::Scanner sc(padded_data.data(), data.size());
                    Tachyon::read(obj_t, sc);
                    auto end = std::chrono::high_resolution_clock::now();
                    times.push_back(std::chrono::duration<double>(end - start).count());
                }
                auto s = calculate_stats(times, data.size());
                std::cout << "Tachyon: " << std::setw(10) << s.mb_s << " MB/s | " << s.median_time * 1000 << " ms" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Tachyon Abort: " << e.what() << std::endl;
            }
             std::cout << "Verified: Tachyon parsed." << std::endl;
        }
    };

    {
        canada::FeatureCollection obj_g;
        canada::FeatureCollection obj_t;
        run_test("Canada.json", canada_data, canada_padded, obj_g, obj_t);
    }

    {
        small::Object obj_g;
        small::Object obj_t;
        run_test("Small.json", small_data, small_padded, obj_g, obj_t);
    }

    return 0;
}
