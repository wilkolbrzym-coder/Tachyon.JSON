#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <optional>
#include <glaze/glaze.hpp>

// --- Canada.json Structs ---
namespace canada {

struct Geometry {
    std::string type;
    std::vector<std::vector<std::vector<double>>> coordinates;
};

struct Property {
    std::string name;
};

struct Feature {
    std::string type;
    Property properties;
    Geometry geometry;
};

struct FeatureCollection {
    std::string type;
    std::vector<Feature> features;
};

} // namespace canada

// --- Twitter.json Structs ---
namespace twitter {

struct Metadata {
    std::string result_type;
    std::string iso_language_code;
};

struct Url {
    std::string url;
    std::string expanded_url;
    std::string display_url;
    std::vector<int> indices;
};

struct UrlEntity {
    std::vector<Url> urls;
};

struct UserEntities {
    UrlEntity url;
    UrlEntity description;
};

struct User {
    uint64_t id;
    std::string id_str;
    std::string name;
    std::string screen_name;
    std::string location;
    std::string description;
    std::string url;
    UserEntities entities;
    bool protected_user;
    int followers_count;
    int friends_count;
    int listed_count;
    std::string created_at;
    int favourites_count;
    std::optional<int> utc_offset;
    std::optional<std::string> time_zone;
    bool geo_enabled;
    bool verified;
    int statuses_count;
    std::string lang;
    bool contributors_enabled;
    bool is_translator;
    bool is_translation_enabled;
    std::string profile_background_color;
    std::string profile_background_image_url;
    std::string profile_background_image_url_https;
    bool profile_background_tile;
    std::string profile_image_url;
    std::string profile_image_url_https;
    std::string profile_banner_url;
    std::string profile_link_color;
    std::string profile_sidebar_border_color;
    std::string profile_sidebar_fill_color;
    std::string profile_text_color;
    bool profile_use_background_image;
    bool default_profile;
    bool default_profile_image;
    bool following;
    bool follow_request_sent;
    bool notifications;
};

struct Hashtag {
    std::string text;
    std::vector<int> indices;
};

struct UserMention {
    std::string screen_name;
    std::string name;
    int64_t id;
    std::string id_str;
    std::vector<int> indices;
};

struct StatusEntities {
    std::vector<Hashtag> hashtags;
    std::vector<Hashtag> symbols;
    std::vector<Url> urls;
    std::vector<UserMention> user_mentions;
};

struct Status {
    Metadata metadata;
    std::string created_at;
    uint64_t id;
    std::string id_str;
    std::string text;
    std::string source;
    bool truncated;
    std::optional<uint64_t> in_reply_to_status_id;
    std::optional<std::string> in_reply_to_status_id_str;
    std::optional<uint64_t> in_reply_to_user_id;
    std::optional<std::string> in_reply_to_user_id_str;
    std::optional<std::string> in_reply_to_screen_name;
    User user;
    bool is_quote_status;
    int retweet_count;
    int favorite_count;
    StatusEntities entities;
    bool favorited;
    bool retweeted;
    std::string lang;
};

struct SearchMetadata {
    double completed_in;
    uint64_t max_id;
    std::string max_id_str;
    std::string next_results;
    std::string query;
    std::string refresh_url;
    int count;
    uint64_t since_id;
    std::string since_id_str;
};

struct TwitterResult {
    std::vector<Status> statuses;
    SearchMetadata search_metadata;
};

} // namespace twitter

// --- CITM Catalog Structs ---
namespace citm {

struct Event {
    uint64_t id;
    std::string name;
    std::string description;
    std::string subtitle;
    std::string logo;
    int topicId;
};

struct Catalog {
    std::map<std::string, std::string> areaNames;
    std::map<std::string, std::string> audienceSubCategoryNames;
    std::map<std::string, std::string> blockNames;
    std::map<std::string, Event> events;
};

} // namespace citm

// --- Small Struct ---
namespace small {
    struct Meta { bool active; double rank; };
    struct Object {
        int id;
        std::string name;
        bool checked;
        std::vector<int> scores;
        Meta meta;
        std::string description;
    };
}

// Glaze registration for small
template<> struct glz::meta<small::Meta> {
    using T = small::Meta;
    static constexpr auto value = object("active", &T::active, "rank", &T::rank);
};
template<> struct glz::meta<small::Object> {
    using T = small::Object;
    static constexpr auto value = object("id", &T::id, "name", &T::name, "checked", &T::checked, "scores", &T::scores, "meta", &T::meta, "description", &T::description);
};
