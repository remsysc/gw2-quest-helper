#include "gw2_logic.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <sstream>

namespace gw2qh {

using nlohmann::json;

std::string Trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = ToLower(haystack);
    std::string n = ToLower(needle);
    return h.find(n) != std::string::npos;
}

bool ContainsWholePhraseCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return false;
    std::string h = ToLower(haystack);
    std::string n = ToLower(Trim(needle));
    if (n.empty()) return false;
    size_t pos = 0;
    while ((pos = h.find(n, pos)) != std::string::npos) {
        bool leftOk  = (pos == 0) || !std::isalnum(static_cast<unsigned char>(h[pos - 1]));
        size_t end   = pos + n.size();
        bool rightOk = (end >= h.size()) || !std::isalnum(static_cast<unsigned char>(h[end]));
        if (leftOk && rightOk) return true;
        ++pos;
    }
    return false;
}

std::string StripGw2Markup(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '<') {
            size_t close = text.find('>', i + 1);
            if (close != std::string::npos) {
                std::string tag = ToLower(text.substr(i + 1, close - i - 1));
                if (tag == "br" || tag == "br/") {
                    out.push_back('\n');
                }
                i = close + 1;
                continue;
            }
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

std::string UrlEncode(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped.push_back(static_cast<char>(c));
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            escaped.append(buf);
        }
    }
    return escaped;
}

std::string WikiTitleToUrlSlug(const std::string& title) {
    std::string slug = Trim(title);
    for (char& c : slug) {
        if (c == ' ') c = '_';
    }
    return UrlEncode(slug);
}

std::string SanitizeWikiSearchTerm(const std::string& query) {
    std::string clean;
    clean.reserve(query.size());
    for (char c : query) {
        if (c == '[' || c == ']' || c == '|' || c == '{' || c == '}' || c == '~' || c == '*' || c == '<' || c == '>') {
            continue;
        }
        clean.push_back(c);
    }
    return Trim(clean);
}

static std::string ToTitleCaseVariant(const std::string& input, bool lowerArticles) {
    static const std::unordered_set<std::string> kMinorWords = {
        "a", "an", "and", "as", "at", "by", "for", "from", "in", "into", "of", "on", "or", "the", "to", "with"
    };
    std::istringstream iss(input);
    std::string word;
    std::string out;
    bool firstWord = true;
    while (iss >> word) {
        std::string lw = ToLower(word);
        if (!firstWord) out.push_back(' ');
        if (lowerArticles && !firstWord && kMinorWords.count(lw) > 0) {
            out += lw;
        } else {
            for (size_t i = 0; i < lw.size(); ++i) {
                if (i == 0 || (lw[i - 1] == '-' && std::isalpha(static_cast<unsigned char>(lw[i])))) {
                    lw[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(lw[i])));
                }
            }
            out += lw;
        }
        firstWord = false;
    }
    return out;
}

std::vector<std::string> BuildWikiCaseVariants(const std::string& cleanTerm) {
    std::vector<std::string> variants;
    auto addUnique = [&](const std::string& s) {
        if (s.empty()) return;
        if (std::find(variants.begin(), variants.end(), s) == variants.end()) {
            variants.push_back(s);
        }
    };

    addUnique(ToTitleCaseVariant(cleanTerm, true));
    addUnique(ToTitleCaseVariant(cleanTerm, false));
    addUnique(ToLower(cleanTerm));
    addUnique(cleanTerm);
    return variants;
}

std::string BuildWikiSmwCondition(const std::string& cleanTerm) {
    auto variants = BuildWikiCaseVariants(cleanTerm);
    std::vector<std::string> patterns;
    auto addPattern = [&](const std::string& p) {
        if (p.empty()) return;
        if (std::find(patterns.begin(), patterns.end(), p) == patterns.end()) {
            patterns.push_back(p);
        }
    };

    for (const auto& v : variants) {
        addPattern("~*" + v + "*");
    }

    // Multi-word queries (e.g. "return to domain of istan") also emit wildcard-joined
    // and stopword-filtered SMW patterns so minor articles like "the" never break search.
    static const std::unordered_set<std::string> kStopWords = {
        "a", "an", "and", "as", "at", "by", "for", "from", "in", "into", "of", "on", "or", "the", "to", "with"
    };
    std::istringstream iss(cleanTerm);
    std::string w;
    std::vector<std::string> titleWords;
    std::vector<std::string> sigWords;
    while (iss >> w) {
        std::string tw = ToTitleCaseVariant(w, false);
        titleWords.push_back(tw);
        if (!kStopWords.count(ToLower(w))) {
            sigWords.push_back(tw);
        }
    }
    if (titleWords.size() >= 2) {
        std::string p = "~*";
        for (const auto& tw : titleWords) p += tw + "*";
        addPattern(p);
    }
    if (sigWords.size() >= 2 && sigWords.size() < titleWords.size()) {
        std::string p = "~*";
        for (const auto& sw : sigWords) p += sw + "*";
        addPattern(p);
    }

    std::string cond;
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) cond += "||";
        cond += patterns[i];
    }
    return cond;
}

std::string SanitizeApiKey(const std::string& raw) {
    std::string s = Trim(raw);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        s = Trim(s.substr(1, s.size() - 2));
    }
    std::string filtered;
    filtered.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-') {
            filtered.push_back(static_cast<char>(c));
        }
    }
    return filtered;
}

std::string ParseEnvApiKey(const std::string& envContent) {
    std::istringstream iss(envContent);
    std::string line;
    while (std::getline(iss, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        size_t eq = trimmed.find('=');
        if (eq != std::string::npos) {
            std::string key = Trim(trimmed.substr(0, eq));
            std::string val = trimmed.substr(eq + 1);
            size_t hashPos = val.find('#');
            if (hashPos != std::string::npos) val = val.substr(0, hashPos);
            if (key == "GW2_API_KEY" || key == "API_KEY" || key == "GW2_TOKEN") {
                return SanitizeApiKey(val);
            }
        } else if (trimmed.size() >= 36 && trimmed.find('-') != std::string::npos) {
            return SanitizeApiKey(trimmed);
        }
    }
    return "";
}

bool SplitRenderIconUrl(const std::string& fullUrl, std::string& outRemote, std::string& outEndpoint) {
    if (fullUrl.empty()) return false;
    const std::string prefix = "https://render.guildwars2.com";
    if (fullUrl.rfind(prefix, 0) == 0) {
        outRemote   = prefix;
        outEndpoint = fullUrl.substr(prefix.size());
        return !outEndpoint.empty();
    }
    return false;
}

std::string ExtractCollectionTargetFromBitText(const std::string& bitText) {
    // Matches patterns like:
    // "Complete the Skyscale Eggs collection." -> "Skyscale Eggs"
    // "Complete Skyscale of Water." -> "Skyscale of Water"
    std::string t = Trim(bitText);
    if (!t.empty() && t.back() == '.') t.pop_back();
    std::string lt = ToLower(t);
    if (lt.rfind("complete the ", 0) == 0) {
        t = t.substr(13);
        lt = ToLower(t);
    } else if (lt.rfind("complete ", 0) == 0) {
        t = t.substr(9);
        lt = ToLower(t);
    } else {
        return "";
    }
    const std::string suffix = " collection";
    if (lt.size() > suffix.size() && lt.substr(lt.size() - suffix.size()) == suffix) {
        t = t.substr(0, t.size() - suffix.size());
    }
    return Trim(t);
}

static std::string Base64EncodeBytes(const uint8_t* data, size_t len) {
    static const char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

        out.push_back(kAlphabet[(n >> 18) & 0x3F]);
        out.push_back(kAlphabet[(n >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kAlphabet[(n >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? kAlphabet[n & 0x3F] : '=');
    }
    return out;
}

std::string EncodeMapChatLink(int poiId) {
    if (poiId <= 0) return "";
    uint32_t u = static_cast<uint32_t>(poiId);
    uint8_t bytes[5] = {
        0x04,
        static_cast<uint8_t>(u & 0xFF),
        static_cast<uint8_t>((u >> 8) & 0xFF),
        static_cast<uint8_t>((u >> 16) & 0xFF),
        static_cast<uint8_t>((u >> 24) & 0xFF)
    };
    return "[&" + Base64EncodeBytes(bytes, 5) + "]";
}

int ParseHttpCacheRemainingSec(const std::string& cacheControl, const std::string& ageHeader) {
    if (cacheControl.empty()) return -1;
    std::string lower = ToLower(cacheControl);
    size_t pos = lower.find("max-age=");
    if (pos == std::string::npos) return -1;
    pos += 8;
    while (pos < lower.size() && std::isspace(static_cast<unsigned char>(lower[pos]))) ++pos;
    if (pos >= lower.size() || !std::isdigit(static_cast<unsigned char>(lower[pos]))) return -1;

    int maxAge = 0;
    while (pos < lower.size() && std::isdigit(static_cast<unsigned char>(lower[pos]))) {
        maxAge = maxAge * 10 + (lower[pos] - '0');
        if (maxAge > 86400) { maxAge = 86400; break; }
        ++pos;
    }

    int age = 0;
    std::string cleanAge = Trim(ageHeader);
    if (!cleanAge.empty() && std::isdigit(static_cast<unsigned char>(cleanAge[0]))) {
        for (char c : cleanAge) {
            if (!std::isdigit(static_cast<unsigned char>(c))) break;
            age = age * 10 + (c - '0');
            if (age > 86400) { age = 86400; break; }
        }
    }
    return std::max(0, maxAge - age);
}

DecodedChatLink DecodeChatLink(const std::string& text) {
    DecodedChatLink res{};
    std::string trimmed = Trim(text);
    size_t openPos = trimmed.find("[&");
    if (openPos == std::string::npos) return res;
    size_t closePos = trimmed.find(']', openPos + 2);
    if (closePos == std::string::npos || closePos <= openPos + 2) return res;
    std::string b64 = trimmed.substr(openPos + 2, closePos - (openPos + 2));

    static const std::string kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; ++i) T[static_cast<unsigned char>(kAlphabet[i])] = i;

    std::vector<uint8_t> bytes;
    int val = 0;
    int valb = -8;
    for (unsigned char c : b64) {
        if (c == '=') break;
        if (T[c] == -1) return res;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            bytes.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    if (bytes.empty()) return res;
    res.type = bytes[0];
    if (res.type == 0x02 && bytes.size() >= 5) {
        res.valid = true;
        res.count = std::max(1, static_cast<int>(bytes[1]));
        res.id = static_cast<int>(bytes[2]) |
                 (static_cast<int>(bytes[3]) << 8) |
                 (static_cast<int>(bytes[4]) << 16);
    } else if ((res.type == 0x04 || res.type == 0x0A || res.type == 0x0B || res.type == 0x06) && bytes.size() >= 4) {
        res.valid = true;
        res.count = 1;
        res.id = static_cast<int>(bytes[1]) |
                 (static_cast<int>(bytes[2]) << 8) |
                 (static_cast<int>(bytes[3]) << 16);
    }
    return res;
}

bool ParseApiErrorMessage(const std::string& jsonStr, std::string& outMessage) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) return false;
    if (doc.contains("text") && doc["text"].is_string()) {
        outMessage = doc["text"].get<std::string>();
        return true;
    }
    return false;
}

bool ParseTokenInfoJson(const std::string& jsonStr, std::vector<std::string>& outPermissions, std::string& outError) {
    outPermissions.clear();
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        outError = "Invalid response from /v2/tokeninfo";
        return false;
    }
    if (doc.contains("text") && doc["text"].is_string() && !doc.contains("permissions")) {
        outError = doc["text"].get<std::string>();
        return false;
    }
    if (doc.contains("permissions") && doc["permissions"].is_array()) {
        for (const auto& p : doc["permissions"]) {
            if (p.is_string()) outPermissions.push_back(p.get<std::string>());
        }
        return true;
    }
    outError = "Missing permissions field in /v2/tokeninfo";
    return false;
}

bool ParseAccountJson(const std::string& jsonStr, std::string& outAccountName) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) return false;
    if (doc.contains("name") && doc["name"].is_string()) {
        outAccountName = doc["name"].get<std::string>();
        return true;
    }
    return false;
}

bool ParseAccountAchievementsJson(const std::string& jsonStr,
                                  std::unordered_map<int, AccountAchievementProgress>& outAchievements) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_array()) return false;
    outAchievements.clear();
    for (const auto& item : doc) {
        if (!item.is_object() || !item.contains("id") || !item["id"].is_number_integer()) continue;
        AccountAchievementProgress prog{};
        prog.id = item["id"].get<int>();
        if (item.contains("current") && item["current"].is_number_integer()) prog.current = item["current"].get<int>();
        if (item.contains("max") && item["max"].is_number_integer())         prog.max     = item["max"].get<int>();
        if (item.contains("done") && item["done"].is_boolean())              prog.done    = item["done"].get<bool>();
        if (item.contains("repeated") && item["repeated"].is_number_integer()) prog.repeated = item["repeated"].get<int>();
        if (item.contains("bits") && item["bits"].is_array()) {
            for (const auto& b : item["bits"]) {
                if (b.is_number_integer()) prog.completedBits.insert(b.get<int>());
            }
        }
        outAchievements[prog.id] = std::move(prog);
    }
    return true;
}

bool ParseIntArrayJson(const std::string& jsonStr, std::unordered_set<int>& outIds) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_array()) return false;
    outIds.clear();
    for (const auto& v : doc) {
        if (v.is_number_integer()) outIds.insert(v.get<int>());
    }
    return true;
}

bool ParseStringArrayJson(const std::string& jsonStr, std::vector<std::string>& outStrings) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_array()) return false;
    outStrings.clear();
    for (const auto& v : doc) {
        if (v.is_string()) outStrings.push_back(v.get<std::string>());
    }
    return true;
}

static void ParseSingleAchievementMeta(const json& obj, std::unordered_map<int, AchievementMeta>& metaMap) {
    if (!obj.is_object() || !obj.contains("id") || !obj["id"].is_number_integer()) return;
    AchievementMeta m{};
    m.id          = obj["id"].get<int>();
    m.name        = obj.value("name", "");
    m.description = StripGw2Markup(obj.value("description", ""));
    m.requirement = StripGw2Markup(obj.value("requirement", ""));
    m.lockedText  = StripGw2Markup(obj.value("locked_text", ""));
    m.type        = obj.value("type", "Default");
    m.iconUrl     = obj.value("icon", "");
    m.maxCount    = 1;

    if (obj.contains("tiers") && obj["tiers"].is_array()) {
        for (const auto& tier : obj["tiers"]) {
            if (tier.is_object() && tier.contains("count") && tier["count"].is_number_integer()) {
                m.maxCount = std::max(m.maxCount, tier["count"].get<int>());
            }
        }
    }

    if (obj.contains("bits") && obj["bits"].is_array()) {
        int idx = 0;
        for (const auto& b : obj["bits"]) {
            AchievementBitMeta bm{};
            bm.index = idx++;
            if (b.is_object()) {
                std::string t = b.value("type", "Text");
                if (t == "Text") bm.type = BitType::Text;
                else if (t == "Item") bm.type = BitType::Item;
                else if (t == "Skin") bm.type = BitType::Skin;
                else if (t == "MiniPet") bm.type = BitType::MiniPet;
                else bm.type = BitType::Unknown;

                bm.id   = b.value("id", 0);
                bm.text = StripGw2Markup(b.value("text", ""));
            }
            m.bits.push_back(std::move(bm));
        }
        if (!m.bits.empty() && m.maxCount < static_cast<int>(m.bits.size())) {
            m.maxCount = static_cast<int>(m.bits.size());
        }
    }

    if (obj.contains("rewards") && obj["rewards"].is_array()) {
        for (const auto& r : obj["rewards"]) {
            if (r.is_object() && r.value("type", "") == "Item" &&
                r.contains("id") && r["id"].is_number_integer()) {
                m.rewardItemIds.push_back(r["id"].get<int>());
            }
        }
    }

    if (obj.contains("flags") && obj["flags"].is_array()) {
        for (const auto& fl : obj["flags"]) {
            if (fl.is_string() && fl.get<std::string>() == "CategoryDisplay") {
                m.isCategoryDisplay = true;
            }
        }
    }

    if (obj.contains("prerequisites") && obj["prerequisites"].is_array()) {
        for (const auto& pr : obj["prerequisites"]) {
            if (pr.is_number_integer()) m.prerequisiteIds.push_back(pr.get<int>());
        }
    }

    m.loaded = true;
    metaMap[m.id] = std::move(m);
}

void ParseAchievementsMetaJson(const std::string& jsonStr, std::unordered_map<int, AchievementMeta>& metaMap) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded()) return;
    if (doc.is_array()) {
        for (const auto& obj : doc) ParseSingleAchievementMeta(obj, metaMap);
    } else if (doc.is_object()) {
        ParseSingleAchievementMeta(doc, metaMap);
    }
}

void ParseAchievementCategoriesJson(const std::string& jsonStr,
                                    std::unordered_map<int, AchievementCategoryMeta>& outCategories) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded()) return;
    auto parseOne = [&](const json& obj) {
        if (!obj.is_object() || !obj.contains("id") || !obj["id"].is_number_integer()) return;
        AchievementCategoryMeta cat{};
        cat.id      = obj["id"].get<int>();
        cat.name    = obj.value("name", "");
        cat.iconUrl = obj.value("icon", "");
        if (obj.contains("achievements") && obj["achievements"].is_array()) {
            for (const auto& aid : obj["achievements"]) {
                if (aid.is_number_integer()) cat.achievementIds.push_back(aid.get<int>());
            }
        }
        outCategories[cat.id] = std::move(cat);
    };
    if (doc.is_array()) {
        for (const auto& obj : doc) parseOne(obj);
    } else if (doc.is_object()) {
        parseOne(doc);
    }
}

static void ParseSingleItemMeta(const json& obj, std::unordered_map<int, ItemMeta>& metaMap) {
    if (!obj.is_object() || !obj.contains("id") || !obj["id"].is_number_integer()) return;
    ItemMeta m{};
    m.id          = obj["id"].get<int>();
    m.name        = obj.value("name", "");
    m.type        = obj.value("type", "");
    m.rarity      = obj.value("rarity", "");
    m.description = StripGw2Markup(obj.value("description", ""));
    m.iconUrl     = obj.value("icon", "");
    m.loaded      = true;
    metaMap[m.id] = std::move(m);
}

void ParseItemsMetaJson(const std::string& jsonStr, std::unordered_map<int, ItemMeta>& metaMap) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded()) return;
    if (doc.is_array()) {
        for (const auto& obj : doc) ParseSingleItemMeta(obj, metaMap);
    } else if (doc.is_object()) {
        ParseSingleItemMeta(doc, metaMap);
    }
}

void ParseQuestsMetaJson(const std::string& jsonStr, std::unordered_map<int, QuestMeta>& metaMap) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded()) return;
    auto parseOne = [&](const json& obj) {
        if (!obj.is_object() || !obj.contains("id") || !obj["id"].is_number_integer()) return;
        QuestMeta q{};
        q.id      = obj["id"].get<int>();
        q.name    = obj.value("name", "");
        q.level   = obj.value("level", 0);
        q.storyId = obj.value("story", 0);
        if (obj.contains("goals") && obj["goals"].is_array()) {
            for (const auto& g : obj["goals"]) {
                if (!g.is_object()) continue;
                QuestGoalMeta gm{};
                gm.activeText   = StripGw2Markup(g.value("active", ""));
                gm.completeText = StripGw2Markup(g.value("complete", ""));
                q.goals.push_back(std::move(gm));
            }
        }
        q.loaded = true;
        metaMap[q.id] = std::move(q);
    };
    if (doc.is_array()) {
        for (const auto& obj : doc) parseOne(obj);
    } else if (doc.is_object()) {
        parseOne(doc);
    }
}

void ParseStoriesMetaJson(const std::string& jsonStr, std::unordered_map<int, std::string>& storyNames) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_array()) return;
    for (const auto& obj : doc) {
        if (obj.is_object() && obj.contains("id") && obj["id"].is_number_integer()) {
            storyNames[obj["id"].get<int>()] = obj.value("name", "");
        }
    }
}

void ParseIdNameArrayJson(const std::string& jsonStr, std::unordered_map<int, std::string>& idToName) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded()) return;
    auto parseOne = [&](const json& obj) {
        if (obj.is_object() && obj.contains("id") && obj["id"].is_number_integer()) {
            idToName[obj["id"].get<int>()] = obj.value("name", "");
        }
    };
    if (doc.is_array()) {
        for (const auto& obj : doc) parseOne(obj);
    } else if (doc.is_object()) {
        parseOne(doc);
    }
}

bool ParseMapNameJson(const std::string& jsonStr, std::string& outMapName) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) return false;
    if (doc.contains("name") && doc["name"].is_string()) {
        outMapName = doc["name"].get<std::string>();
        return true;
    }
    return false;
}

std::vector<SearchResultItem> ParseWikiAskResultsJson(const std::string& jsonStr, EntryKind kind) {
    std::vector<SearchResultItem> out;
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) return out;
    if (!doc.contains("query") || !doc["query"].is_object()) return out;
    const auto& q = doc["query"];
    if (!q.contains("results") || !q["results"].is_object()) return out;

    std::unordered_set<int> seenIds;
    for (auto it = q["results"].begin(); it != q["results"].end(); ++it) {
        const auto& val = it.value();
        if (!val.is_object() || !val.contains("printouts") || !val["printouts"].is_object()) continue;
        const auto& po = val["printouts"];
        if (!po.contains("Has game id") || !po["Has game id"].is_array() || po["Has game id"].empty()) continue;

        int gameId = 0;
        if (po["Has game id"][0].is_number_integer()) gameId = po["Has game id"][0].get<int>();
        if (gameId <= 0 || seenIds.count(gameId)) continue;
        seenIds.insert(gameId);

        std::string canonicalName;
        if (po.contains("Has canonical name") && po["Has canonical name"].is_array() &&
            !po["Has canonical name"].empty() && po["Has canonical name"][0].is_string()) {
            canonicalName = po["Has canonical name"][0].get<std::string>();
        }
        std::string fullText = val.value("fulltext", it.key());
        if (canonicalName.empty()) canonicalName = fullText;

        SearchResultItem item{};
        item.kind = kind;
        item.id   = gameId;
        item.name = canonicalName;
        item.subtitle = (fullText != canonicalName)
            ? (fullText + " (#" + std::to_string(gameId) + ")")
            : ("#" + std::to_string(gameId));
        out.push_back(std::move(item));
    }
    return out;
}

int ParseWikiFirstGameIdJson(const std::string& jsonStr) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) return 0;
    if (!doc.contains("query") || !doc["query"].is_object()) return 0;
    const auto& q = doc["query"];
    if (!q.contains("results") || !q["results"].is_object()) return 0;

    for (auto it = q["results"].begin(); it != q["results"].end(); ++it) {
        const auto& val = it.value();
        if (!val.is_object() || !val.contains("printouts") || !val["printouts"].is_object()) continue;
        const auto& po = val["printouts"];
        if (po.contains("Has game id") && po["Has game id"].is_array() &&
            !po["Has game id"].empty() && po["Has game id"][0].is_number_integer()) {
            int id = po["Has game id"][0].get<int>();
            if (id > 0) return id;
        }
    }
    return 0;
}

std::string ParseWikiParentCollectionNameJson(const std::string& jsonStr) {
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) return "";
    if (!doc.contains("query") || !doc["query"].is_object()) return "";
    const auto& q = doc["query"];
    if (!q.contains("results") || !q["results"].is_object()) return "";

    for (auto it = q["results"].begin(); it != q["results"].end(); ++it) {
        const auto& val = it.value();
        if (!val.is_object() || !val.contains("printouts") || !val["printouts"].is_object()) continue;
        const auto& po = val["printouts"];
        if (!po.contains("Is part of collection") || !po["Is part of collection"].is_array() ||
            po["Is part of collection"].empty()) continue;

        const auto& first = po["Is part of collection"][0];
        if (first.is_object() && first.contains("fulltext") && first["fulltext"].is_string()) {
            std::string ft = first["fulltext"].get<std::string>();
            size_t hash = ft.find('#');
            return Trim((hash == std::string::npos) ? ft : ft.substr(0, hash));
        }
        if (first.is_string()) {
            return Trim(first.get<std::string>());
        }
    }
    return "";
}

std::string CleanWikitextMarkup(const std::string& raw, std::string* outFirstWaypointName) {
    std::string out;
    out.reserve(raw.size());
    size_t i = 0;
    while (i < raw.size()) {
        if (i + 1 < raw.size() && raw[i] == '[' && raw[i + 1] == '[') {
            size_t close = raw.find("]]", i + 2);
            if (close != std::string::npos) {
                std::string inner = raw.substr(i + 2, close - (i + 2));
                std::string lowerInner = ToLower(inner);
                if (lowerInner.rfind("file:", 0) == 0 || lowerInner.rfind("image:", 0) == 0) {
                    i = close + 2;
                    continue;
                }
                size_t pipePos = inner.find('|');
                std::string target  = Trim((pipePos == std::string::npos) ? inner : inner.substr(0, pipePos));
                std::string display = Trim((pipePos == std::string::npos) ? inner : inner.substr(pipePos + 1));

                if (outFirstWaypointName && outFirstWaypointName->empty() &&
                    ContainsCaseInsensitive(target, "Waypoint")) {
                    size_t hash = target.find('#');
                    *outFirstWaypointName = (hash == std::string::npos) ? target : Trim(target.substr(hash + 1));
                }

                out += display;
                i = close + 2;
                continue;
            }
        }
        if (i + 1 < raw.size() && raw[i] == '{' && raw[i + 1] == '{') {
            int depth = 1;
            size_t pos = i + 2;
            while (pos + 1 < raw.size() && depth > 0) {
                if (raw[pos] == '{' && raw[pos + 1] == '{') { ++depth; pos += 2; }
                else if (raw[pos] == '}' && raw[pos + 1] == '}') { --depth; if (depth == 0) break; pos += 2; }
                else { ++pos; }
            }
            if (depth == 0) {
                std::string tmpl = raw.substr(i + 2, pos - (i + 2));
                size_t p1 = tmpl.find('|');
                if (p1 != std::string::npos) {
                    std::string tname = ToLower(Trim(tmpl.substr(0, p1)));
                    std::string rest  = tmpl.substr(p1 + 1);
                    size_t p2 = rest.find('|');
                    std::string arg1 = Trim((p2 == std::string::npos) ? rest : rest.substr(0, p2));
                    if (outFirstWaypointName && outFirstWaypointName->empty() &&
                        (tname == "wp" || tname == "waypoint" || ContainsCaseInsensitive(arg1, "Waypoint"))) {
                        *outFirstWaypointName = arg1;
                    }
                    if (!arg1.empty() && arg1.find('=') == std::string::npos) out += arg1;
                }
                i = pos + 2;
                continue;
            }
        }
        if (i + 1 < raw.size() && raw[i] == '\'' && raw[i + 1] == '\'') {
            while (i < raw.size() && raw[i] == '\'') ++i;
            continue;
        }
        out.push_back(raw[i]);
        ++i;
    }
    return Trim(StripGw2Markup(out));
}

static std::vector<std::string> SplitTopLevelPipes(const std::string& body) {
    std::vector<std::string> parts;
    int braceDepth = 0;
    int bracketDepth = 0;
    size_t start = 0;
    for (size_t i = 0; i < body.size(); ++i) {
        if (i + 1 < body.size() && body[i] == '{' && body[i + 1] == '{') { ++braceDepth; ++i; }
        else if (i + 1 < body.size() && body[i] == '}' && body[i + 1] == '}') { braceDepth = std::max(0, braceDepth - 1); ++i; }
        else if (i + 1 < body.size() && body[i] == '[' && body[i + 1] == '[') { ++bracketDepth; ++i; }
        else if (i + 1 < body.size() && body[i] == ']' && body[i + 1] == ']') { bracketDepth = std::max(0, bracketDepth - 1); ++i; }
        else if (body[i] == '|' && braceDepth == 0 && bracketDepth == 0) {
            parts.push_back(body.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(body.substr(start));
    return parts;
}

WikiAchievementGuide ParseWikiAchievementGuideJson(const std::string& parseJsonStr, const std::string& fallbackTitle) {
    WikiAchievementGuide guide{};
    guide.pageTitle = fallbackTitle;
    guide.pageUrl   = "https://wiki.guildwars2.com/wiki/" + WikiTitleToUrlSlug(fallbackTitle);

    json doc = json::parse(parseJsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("parse") || !doc["parse"].is_object()) {
        return guide;
    }
    const auto& p = doc["parse"];
    if (p.contains("title") && p["title"].is_string()) {
        guide.pageTitle = p["title"].get<std::string>();
        guide.pageUrl   = "https://wiki.guildwars2.com/wiki/" + WikiTitleToUrlSlug(guide.pageTitle);
    }
    if (!p.contains("wikitext") || !p["wikitext"].is_object() || !p["wikitext"].contains("*")) {
        return guide;
    }
    std::string wt = p["wikitext"]["*"].get<std::string>();

    size_t firstHeading = wt.find("\n==");
    std::string introRaw = (firstHeading == std::string::npos) ? wt.substr(0, 600) : wt.substr(0, firstHeading);
    guide.introText = CleanWikitextMarkup(introRaw);

    std::string fallbackNav;
    size_t pos = 0;
    while (pos < wt.size()) {
        size_t open = wt.find("{{", pos);
        if (open == std::string::npos) break;

        int depth = 1;
        size_t cur = open + 2;
        while (cur + 1 < wt.size() && depth > 0) {
            if (wt[cur] == '{' && wt[cur + 1] == '{') { ++depth; cur += 2; }
            else if (wt[cur] == '}' && wt[cur + 1] == '}') { --depth; if (depth == 0) break; cur += 2; }
            else { ++cur; }
        }
        if (depth != 0) break;

        std::string tmplBody = wt.substr(open + 2, cur - (open + 2));
        auto parts = SplitTopLevelPipes(tmplBody);
        if (!parts.empty()) {
            std::string rawName  = Trim(parts[0]);
            std::string tmplName = ToLower(rawName);

            // Detect questline navbox templates like `{{Skyscale unlock nav}}` or `{{Seasons of the Dragons nav}}`
            if (parts.size() == 1 && tmplName.size() > 4 && tmplName.substr(tmplName.size() - 4) == " nav") {
                if (tmplName.find("unlock") != std::string::npos ||
                    tmplName.find("skyscale") != std::string::npos ||
                    tmplName.find("aurora") != std::string::npos ||
                    tmplName.find("vision") != std::string::npos ||
                    tmplName.find("regalia") != std::string::npos ||
                    tmplName.find("legendary") != std::string::npos) {
                    guide.navTemplateName = rawName;
                } else if (fallbackNav.empty()) {
                    fallbackNav = rawName;
                }
            }

            if (tmplName == "collection table row" || tmplName == "achievement table row") {
                WikiStepGuide step{};
                std::string rawNotes;
                std::string rawArea;

                for (size_t k = 1; k < parts.size(); ++k) {
                    size_t eq = parts[k].find('=');
                    if (eq == std::string::npos) {
                        if (step.rowTitle.empty()) step.rowTitle = CleanWikitextMarkup(parts[k]);
                    } else {
                        std::string paramKey = ToLower(Trim(parts[k].substr(0, eq)));
                        std::string paramVal = Trim(parts[k].substr(eq + 1));
                        if (paramKey == "1" && step.rowTitle.empty()) {
                            step.rowTitle = CleanWikitextMarkup(paramVal);
                        } else if (paramKey == "col1" || paramKey == "area" || paramKey == "location" || paramKey == "zone") {
                            std::string lv = ToLower(paramVal);
                            if (lv.find(".jpg") == std::string::npos && lv.find(".png") == std::string::npos && rawArea.empty()) {
                                rawArea = paramVal;
                            }
                        } else if (paramKey == "notes" || paramKey == "hint" || paramKey == "description" || paramKey == "col2") {
                            if (rawNotes.empty()) rawNotes = paramVal;
                        }
                    }
                }

                std::string wpName;
                step.area         = CleanWikitextMarkup(rawArea, &wpName);
                step.cleanNotes   = CleanWikitextMarkup(rawNotes, &wpName);
                step.waypointName = wpName;

                DecodedChatLink existingLink = DecodeChatLink(rawArea + " " + rawNotes);
                if (existingLink.valid && existingLink.type == 0x04) {
                    step.waypointChatLink = EncodeMapChatLink(existingLink.id);
                }

                if (!step.rowTitle.empty() || !step.cleanNotes.empty() || !step.area.empty()) {
                    guide.steps.push_back(std::move(step));
                }
            }
        }
        pos = cur + 2;
    }

    guide.loaded = true;
    return guide;
}

static std::vector<std::string> ExtractWikiLinkTargets(const std::string& line) {
    std::vector<std::string> targets;
    size_t pos = 0;
    while (pos < line.size()) {
        size_t open = line.find("[[", pos);
        if (open == std::string::npos) break;
        size_t close = line.find("]]", open + 2);
        if (close == std::string::npos) break;
        std::string inner = line.substr(open + 2, close - (open + 2));
        std::string low   = ToLower(inner);
        if (low.rfind("file:", 0) != 0 && low.rfind("image:", 0) != 0 && low.rfind("category:", 0) != 0) {
            size_t pipe = inner.find('|');
            std::string target = Trim((pipe == std::string::npos) ? inner : inner.substr(0, pipe));
            size_t hash = target.find('#');
            if (hash != std::string::npos) target = Trim(target.substr(0, hash));
            if (!target.empty()) targets.push_back(target);
        }
        pos = close + 2;
    }
    return targets;
}

QuestlineRoadmapMeta ParseWikiNavboxRoadmapJson(const std::string& parseJsonStr, const std::string& templateName) {
    QuestlineRoadmapMeta roadmap{};
    json doc = json::parse(parseJsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("parse") || !doc["parse"].is_object()) {
        return roadmap;
    }
    const auto& p = doc["parse"];
    if (!p.contains("wikitext") || !p["wikitext"].is_object() || !p["wikitext"].contains("*")) {
        return roadmap;
    }
    std::string wt = p["wikitext"]["*"].get<std::string>();
    // Strip <noinclude>...</noinclude>
    size_t noInc = wt.find("<noinclude>");
    if (noInc != std::string::npos) wt = wt.substr(0, noInc);

    // Extract heading if present (`<div class="heading">...</div>`)
    size_t headPos = wt.find("class=\"heading\">");
    if (headPos != std::string::npos) {
        size_t closeDiv = wt.find("</div>", headPos);
        if (closeDiv != std::string::npos) {
            roadmap.questlineTitle = CleanWikitextMarkup(wt.substr(headPos + 16, closeDiv - (headPos + 16)));
        }
    }
    if (roadmap.questlineTitle.empty()) {
        roadmap.questlineTitle = templateName;
        if (roadmap.questlineTitle.size() > 4 &&
            ToLower(roadmap.questlineTitle.substr(roadmap.questlineTitle.size() - 4)) == " nav") {
            roadmap.questlineTitle = Trim(roadmap.questlineTitle.substr(0, roadmap.questlineTitle.size() - 4));
        }
    }

    // Parse MediaWiki table rows: `! ... [[StageTitle]]` followed by `| [[Child1]] • [[Child2]]`
    std::istringstream iss(wt);
    std::string line;
    QuestlineStageMeta currentStage{};
    bool inTable = false;

    auto flushStage = [&]() {
        if (!currentStage.stageTitle.empty() && !currentStage.childTitles.empty()) {
            roadmap.stages.push_back(std::move(currentStage));
        }
        currentStage = QuestlineStageMeta{};
    };

    while (std::getline(iss, line)) {
        std::string t = Trim(line);
        if (t.rfind("{|", 0) == 0) { inTable = true; continue; }
        if (!inTable) continue;
        if (t.rfind("|}", 0) == 0) { flushStage(); break; }
        if (t.rfind("|-", 0) == 0) { flushStage(); continue; }

        if (!t.empty() && t[0] == '!') {
            auto links = ExtractWikiLinkTargets(t);
            if (!links.empty()) {
                currentStage.stageTitle = links.back();
            }
        } else if (!t.empty() && t[0] == '|') {
            auto links = ExtractWikiLinkTargets(t);
            for (const auto& lk : links) {
                if (std::find(currentStage.childTitles.begin(), currentStage.childTitles.end(), lk) ==
                    currentStage.childTitles.end()) {
                    currentStage.childTitles.push_back(lk);
                }
            }
        }
    }

    roadmap.loaded = !roadmap.stages.empty();
    return roadmap;
}

static std::string NormalizeMatchKey(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

static void FillAchNodeProgress(
    EvaluatedChainNode& node,
    const AccountSnapshot& account,
    const std::unordered_map<int, AchievementMeta>& achievementMeta) {

    if (node.id <= 0) return;
    auto mIt = achievementMeta.find(node.id);
    if (mIt != achievementMeta.end()) {
        node.max = std::max(1, mIt->second.maxCount);
    }
    auto pIt = account.achievements.find(node.id);
    if (pIt != account.achievements.end()) {
        if (pIt->second.max > 0) node.max = pIt->second.max;
        node.completed = pIt->second.done || (pIt->second.repeated > 0);
        node.current   = node.completed ? node.max : pIt->second.current;
    }
}

EvaluatedEntryView EvaluateTrackedEntry(
    const TrackedEntry& entry,
    size_t originalIndex,
    const AccountSnapshot& account,
    const std::string& activeCharacterName,
    const std::string& activeMapName,
    const std::unordered_map<int, AchievementMeta>& achievementMeta,
    const std::unordered_map<int, ItemMeta>& itemMeta,
    const std::unordered_map<int, QuestMeta>& questMeta,
    const std::unordered_map<int, std::string>& skinNames,
    const std::unordered_map<int, std::string>& miniNames,
    const std::unordered_map<int, WikiAchievementGuide>& wikiGuides,
    const std::unordered_map<std::string, int>& achievementNameToId,
    const std::unordered_map<int, QuestlineChainInfo>& chainInfoByAchId,
    const std::unordered_map<std::string, QuestlineRoadmapMeta>& navRoadmaps) {

    EvaluatedEntryView view{};
    view.entry         = entry;
    view.originalIndex = originalIndex;

    if (entry.kind == EntryKind::Achievement) {
        const AccountAchievementProgress* accProg = nullptr;
        auto aIt = account.achievements.find(entry.id);
        if (aIt != account.achievements.end()) accProg = &aIt->second;

        const WikiAchievementGuide* guide = nullptr;
        auto gIt = wikiGuides.find(entry.id);
        if (gIt != wikiGuides.end() && gIt->second.loaded) {
            guide = &gIt->second;
            view.wikiUrl = guide->pageUrl;
        }

        auto mIt = achievementMeta.find(entry.id);
        if (mIt != achievementMeta.end() && mIt->second.loaded) {
            const auto& meta = mIt->second;
            view.metadataLoaded = true;
            view.title          = meta.name;
            view.iconUrl        = meta.iconUrl;
            if (view.iconUrl.empty()) {
                for (int rItemId : meta.rewardItemIds) {
                    auto rIt = itemMeta.find(rItemId);
                    if (rIt != itemMeta.end() && !rIt->second.iconUrl.empty()) {
                        view.iconUrl = rIt->second.iconUrl;
                        break;
                    }
                }
            }
            if (view.wikiUrl.empty()) {
                view.wikiUrl = "https://wiki.guildwars2.com/wiki/" + WikiTitleToUrlSlug(meta.name);
            }
            view.subtitle = !meta.requirement.empty() ? meta.requirement : meta.description;
            view.target   = std::max(1, (accProg && accProg->max > 0) ? accProg->max : meta.maxCount);

            std::unordered_set<int> localBitSet(entry.localCheckedBits.begin(), entry.localCheckedBits.end());
            if (accProg) {
                view.apiCompleted = accProg->done || (accProg->repeated > 0);
                view.completed    = view.apiCompleted;
                view.current      = accProg->done ? view.target : accProg->current;
            }

            if (!activeMapName.empty()) {
                bool allowFlavorMatch = (activeMapName.size() >= 10 || activeMapName.find(' ') != std::string::npos);
                if (ContainsWholePhraseCaseInsensitive(meta.name, activeMapName) ||
                    ContainsWholePhraseCaseInsensitive(meta.requirement, activeMapName) ||
                    ContainsWholePhraseCaseInsensitive(entry.note, activeMapName) ||
                    (allowFlavorMatch && ContainsWholePhraseCaseInsensitive(meta.description, activeMapName)) ||
                    (guide && ContainsWholePhraseCaseInsensitive(guide->introText, activeMapName))) {
                    view.matchesCurrentMap = true;
                }
            }

            if (meta.bits.empty() && !meta.categoryAchievementIds.empty()) {
                int synIdx = 0;
                for (int subId : meta.categoryAchievementIds) {
                    if (subId <= 0 || subId == entry.id) continue;
                    EvaluatedBitView bv{};
                    bv.index              = synIdx++;
                    bv.type               = BitType::Text;
                    bv.referenceId        = subId;
                    bv.childAchievementId = subId;

                    const AccountAchievementProgress* subProg = nullptr;
                    auto spIt = account.achievements.find(subId);
                    if (spIt != account.achievements.end()) subProg = &spIt->second;
                    bv.apiCompleted   = view.apiCompleted || (subProg && (subProg->done || subProg->repeated > 0));
                    bv.localCompleted = !bv.apiCompleted && (localBitSet.count(bv.index) > 0);
                    bv.completed      = bv.apiCompleted || bv.localCompleted;
                    if (bv.localCompleted) ++view.localCheckedCount;

                    std::string subLabel = "Achievement #" + std::to_string(subId);
                    auto smIt = achievementMeta.find(subId);
                    if (smIt != achievementMeta.end() && !smIt->second.name.empty()) {
                        subLabel      = smIt->second.name;
                        bv.guideNotes = smIt->second.requirement;
                        if (!activeMapName.empty() &&
                            (ContainsWholePhraseCaseInsensitive(smIt->second.name, activeMapName) ||
                             ContainsWholePhraseCaseInsensitive(smIt->second.requirement, activeMapName))) {
                            view.matchesCurrentMap = true;
                        }
                    }
                    if (!bv.completed && subProg && subProg->max > 1) {
                        subLabel += " (" + std::to_string(subProg->current) + "/" + std::to_string(subProg->max) + ")";
                    }
                    bv.label = std::move(subLabel);
                    view.bits.push_back(std::move(bv));
                }
            }

            int unlockedBitCounter = 0;
            for (const auto& bitMeta : meta.bits) {
                EvaluatedBitView bv{};
                bv.index       = bitMeta.index;
                bv.type        = bitMeta.type;
                bv.referenceId = bitMeta.id;

                if (accProg && (accProg->done || accProg->completedBits.count(bitMeta.index) > 0)) {
                    bv.apiCompleted = true;
                } else if (bitMeta.type == BitType::Skin && bitMeta.id > 0 && account.unlockedSkins.count(bitMeta.id) > 0) {
                    bv.apiCompleted = true;
                } else if (bitMeta.type == BitType::MiniPet && bitMeta.id > 0 && account.unlockedMinis.count(bitMeta.id) > 0) {
                    bv.apiCompleted = true;
                }
                bv.localCompleted = !bv.apiCompleted && (localBitSet.count(bitMeta.index) > 0);
                bv.completed      = bv.apiCompleted || bv.localCompleted;
                if (bv.apiCompleted) ++unlockedBitCounter;
                if (bv.localCompleted) ++view.localCheckedCount;

                std::string resolvedItemName;
                if (bitMeta.type == BitType::Item && bitMeta.id > 0) {
                    auto itItem = itemMeta.find(bitMeta.id);
                    if (itItem != itemMeta.end() && !itItem->second.name.empty()) {
                        resolvedItemName = itItem->second.name;
                    }
                    // Prefer explicit actionable step text when present (e.g. "Talk to Gorrik in Dragonfall.")
                    if (!bitMeta.text.empty() && bitMeta.text.rfind("Somewhere in ", 0) != 0) {
                        bv.label = bitMeta.text;
                    } else if (!resolvedItemName.empty()) {
                        bv.label = resolvedItemName;
                    } else if (!bitMeta.text.empty()) {
                        bv.label = bitMeta.text;
                    } else {
                        bv.label = "Item #" + std::to_string(bitMeta.id);
                    }
                } else if (bitMeta.type == BitType::Skin && bitMeta.id > 0) {
                    auto itSkin = skinNames.find(bitMeta.id);
                    bv.label = (itSkin != skinNames.end() && !itSkin->second.empty())
                        ? ("Skin: " + itSkin->second)
                        : ("Skin #" + std::to_string(bitMeta.id));
                } else if (bitMeta.type == BitType::MiniPet && bitMeta.id > 0) {
                    auto itMini = miniNames.find(bitMeta.id);
                    bv.label = (itMini != miniNames.end() && !itMini->second.empty())
                        ? ("Mini: " + itMini->second)
                        : ("Mini #" + std::to_string(bitMeta.id));
                } else {
                    bv.label = bitMeta.text.empty()
                        ? ("Objective #" + std::to_string(bitMeta.index + 1))
                        : bitMeta.text;
                }

                if (guide && !guide->steps.empty()) {
                    const WikiStepGuide* matchedStep = nullptr;
                    if (guide->steps.size() == meta.bits.size() &&
                        static_cast<size_t>(bitMeta.index) < guide->steps.size()) {
                        matchedStep = &guide->steps[static_cast<size_t>(bitMeta.index)];
                    } else {
                        std::string bitKey = NormalizeMatchKey(resolvedItemName.empty() ? bv.label : resolvedItemName);
                        for (const auto& st : guide->steps) {
                            if (!bitKey.empty() && NormalizeMatchKey(st.rowTitle) == bitKey) {
                                matchedStep = &st;
                                break;
                            }
                        }
                    }
                    if (matchedStep) {
                        bv.area             = matchedStep->area;
                        bv.guideNotes       = matchedStep->cleanNotes;
                        bv.waypointName     = matchedStep->waypointName;
                        bv.waypointChatLink = matchedStep->waypointChatLink;
                        if (!activeMapName.empty() && ContainsWholePhraseCaseInsensitive(bv.area, activeMapName)) {
                            view.matchesCurrentMap = true;
                        }
                    }
                }

                // Meta-achievement child link lookup (both Text bits and Item bits like "Complete the Skyscale Eggs collection.")
                std::vector<std::string> candidateChildNames;
                if (!bv.label.empty()) candidateChildNames.push_back(bv.label);
                if (!resolvedItemName.empty()) candidateChildNames.push_back(resolvedItemName);
                std::string extractedTarget = ExtractCollectionTargetFromBitText(bitMeta.text);
                if (!extractedTarget.empty()) candidateChildNames.push_back(extractedTarget);

                for (const auto& cand : candidateChildNames) {
                    auto chIt = achievementNameToId.find(ToLower(cand));
                    if (chIt != achievementNameToId.end() && chIt->second != entry.id) {
                        bv.childAchievementId = chIt->second;
                        break;
                    }
                }

                view.bits.push_back(std::move(bv));
            }

            int baseCurrent = view.apiCompleted ? view.target : (accProg ? accProg->current : unlockedBitCounter);
            view.current    = std::min(view.target, baseCurrent + view.localCheckedCount);
            view.completed  = view.apiCompleted || (!view.bits.empty() && view.current >= view.target);

            // Evaluate Questline Chain Context (Parent Breadcrumb + Roadmap + Up Next)
            auto chInfoIt = chainInfoByAchId.find(entry.id);
            if (chInfoIt != chainInfoByAchId.end()) {
                const auto& ci = chInfoIt->second;
                if (ci.parentAchievementId > 0) {
                    view.chain.hasParent   = true;
                    view.chain.parentKind  = EntryKind::Achievement;
                    view.chain.parentId    = ci.parentAchievementId;
                    view.chain.parentTitle = ci.parentTitle;

                    EvaluatedChainNode pNode{};
                    pNode.id = ci.parentAchievementId;
                    FillAchNodeProgress(pNode, account, achievementMeta);
                    view.chain.parentCurrent   = pNode.current;
                    view.chain.parentMax       = pNode.max;
                    view.chain.parentCompleted = pNode.completed;
                }
                view.chain.transitionHint = ci.transitionHint;

                // 1. Check multi-stage Wiki Navbox Roadmap (stage-level only, never 1-by-1 sub-items!)
                std::string navKey = !ci.navTemplateName.empty()
                    ? ci.navTemplateName
                    : (guide ? guide->navTemplateName : "");
                auto navIt = navKey.empty() ? navRoadmaps.end() : navRoadmaps.find(navKey);
                if (navIt != navRoadmaps.end() && navIt->second.loaded) {
                    const auto& rm = navIt->second;
                    view.chain.questlineTitle = rm.questlineTitle;
                    view.chain.stageCount     = static_cast<int>(rm.stages.size());

                    int currentStageIdx = -1;
                    for (size_t s = 0; s < rm.stages.size(); ++s) {
                        const auto& st = rm.stages[s];
                        int resolvedStageId = st.stageAchId;
                        if (resolvedStageId == 0) {
                            auto sIdIt = achievementNameToId.find(ToLower(st.stageTitle));
                            if (sIdIt != achievementNameToId.end()) resolvedStageId = sIdIt->second;
                        }

                        bool isThisStage = (resolvedStageId == entry.id || ToLower(st.stageTitle) == ToLower(meta.name));

                        int doneChildren = 0;
                        for (size_t c = 0; c < st.childTitles.size(); ++c) {
                            int cid = (c < st.childAchIds.size()) ? st.childAchIds[c] : 0;
                            if (cid == 0) {
                                auto idIt = achievementNameToId.find(ToLower(st.childTitles[c]));
                                if (idIt != achievementNameToId.end()) cid = idIt->second;
                            }
                            EvaluatedChainNode chCheck{};
                            chCheck.id = cid;
                            FillAchNodeProgress(chCheck, account, achievementMeta);
                            if (chCheck.completed) ++doneChildren;

                            if (cid == entry.id || ToLower(st.childTitles[c]) == ToLower(meta.name)) {
                                isThisStage = true;
                            }
                        }

                        EvaluatedChainNode stHeader{};
                        stHeader.kind          = EntryKind::Achievement;
                        stHeader.id            = resolvedStageId;
                        stHeader.title         = st.stageTitle;
                        stHeader.isStageHeader = true;
                        stHeader.isCurrent     = isThisStage;
                        stHeader.canJump       = (resolvedStageId > 0 && !isThisStage);
                        FillAchNodeProgress(stHeader, account, achievementMeta);

                        if (stHeader.max <= 1 && !st.childTitles.empty()) {
                            stHeader.max = static_cast<int>(st.childTitles.size());
                            if (!stHeader.completed && stHeader.current == 0) {
                                stHeader.current = doneChildren;
                            }
                        }

                        if (isThisStage) {
                            currentStageIdx = static_cast<int>(s);
                            if (!view.chain.hasParent && resolvedStageId > 0 && resolvedStageId != entry.id) {
                                view.chain.hasParent       = true;
                                view.chain.parentKind      = EntryKind::Achievement;
                                view.chain.parentId        = resolvedStageId;
                                view.chain.parentTitle     = st.stageTitle;
                                view.chain.parentCurrent   = stHeader.current;
                                view.chain.parentMax       = stHeader.max;
                                view.chain.parentCompleted = stHeader.completed;
                            }
                        }

                        view.chain.roadmapNodes.push_back(std::move(stHeader));
                    }

                    if (currentStageIdx >= 0) {
                        view.chain.stageIndex = currentStageIdx + 1;

                        // Suggest ONLY the next main Stage achievement (e.g. Saving Skyscales), not 1-by-1 sub-items!
                        for (size_t s = static_cast<size_t>(currentStageIdx + 1); s < rm.stages.size(); ++s) {
                            const auto& st = rm.stages[s];
                            int nextStageId = st.stageAchId;
                            if (nextStageId == 0) {
                                auto sIdIt = achievementNameToId.find(ToLower(st.stageTitle));
                                if (sIdIt != achievementNameToId.end()) nextStageId = sIdIt->second;
                            }
                            if (nextStageId <= 0 || nextStageId == entry.id) continue;

                            EvaluatedChainNode stCheck{};
                            stCheck.id = nextStageId;
                            FillAchNodeProgress(stCheck, account, achievementMeta);
                            if (!stCheck.completed) {
                                view.chain.hasNext   = true;
                                view.chain.nextKind  = EntryKind::Achievement;
                                view.chain.nextId    = nextStageId;
                                view.chain.nextTitle = st.stageTitle;
                                break;
                            }
                        }
                    }
                }

                // 2. Fallback: Direct Has prerequisite successor (e.g. Beetle Juice -> Beetle Saddle)
                if (!view.chain.hasNext && ci.nextAchievementId > 0) {
                    view.chain.hasNext   = true;
                    view.chain.nextKind  = EntryKind::Achievement;
                    view.chain.nextId    = ci.nextAchievementId;
                    view.chain.nextTitle = ci.nextTitle;
                }
            }
        } else {
            view.title    = entry.labelOverride.empty() ? ("Achievement #" + std::to_string(entry.id)) : entry.labelOverride;
            view.subtitle = "Loading achievement details...";
            if (accProg) {
                view.current   = accProg->current;
                view.target    = std::max(1, accProg->max);
                view.completed = accProg->done;
            }
        }
        return view;
    }

    // EntryKind::Quest (Automatic Personal Story / Living World Chapter tracker — locks onto current active step!)
    view.target = 1;
    const std::unordered_set<int>* completedQuests = nullptr;
    if (!activeCharacterName.empty()) {
        auto cIt = account.characterQuests.find(activeCharacterName);
        if (cIt != account.characterQuests.end()) completedQuests = &cIt->second;
    } else if (!account.characterQuests.empty()) {
        completedQuests = &account.characterQuests.begin()->second;
    }

    auto qIt = questMeta.find(entry.id);
    if (qIt != questMeta.end() && qIt->second.loaded) {
        const auto& qm = qIt->second;
        view.metadataLoaded = true;

        if (qm.storyId > 0 && !qm.storyName.empty()) {
            std::vector<const QuestMeta*> chapterQuests;
            for (const auto& kv : questMeta) {
                if (kv.second.storyId == qm.storyId) chapterQuests.push_back(&kv.second);
            }
            std::sort(chapterQuests.begin(), chapterQuests.end(), [](const QuestMeta* a, const QuestMeta* b) {
                if (a->level != b->level) return a->level < b->level;
                return a->id < b->id;
            });

            int chapterDone = 0;
            const QuestMeta* activeQuest = nullptr;
            for (const QuestMeta* cq : chapterQuests) {
                bool done = (completedQuests && completedQuests->count(cq->id) > 0);
                if (done) {
                    ++chapterDone;
                } else if (!activeQuest) {
                    activeQuest = cq;
                }
            }

            for (const QuestMeta* cq : chapterQuests) {
                bool done = (completedQuests && completedQuests->count(cq->id) > 0);
                EvaluatedChainNode qNode{};
                qNode.kind      = EntryKind::Quest;
                qNode.id        = cq->id;
                qNode.title     = cq->name;
                qNode.current   = done ? 1 : 0;
                qNode.max       = 1;
                qNode.completed = done;
                qNode.isCurrent = (activeQuest && cq->id == activeQuest->id);
                qNode.canJump   = false; // Storyline steps advance automatically; user cannot skip out of order
                view.chain.roadmapNodes.push_back(std::move(qNode));
            }

            view.title     = qm.storyName;
            view.current   = chapterDone;
            view.target    = std::max(1, static_cast<int>(chapterQuests.size()));
            view.completed = (chapterDone >= static_cast<int>(chapterQuests.size()));

            if (activeQuest) {
                view.wikiUrl = "https://wiki.guildwars2.com/wiki/" + WikiTitleToUrlSlug(activeQuest->name);
                if (!activeQuest->goals.empty() && !activeQuest->goals[0].activeText.empty()) {
                    view.subtitle = activeQuest->name + " - " + activeQuest->goals[0].activeText;
                } else {
                    view.subtitle = activeQuest->name;
                }
                view.chain.questlineTitle = "Step " + std::to_string(chapterDone + 1) + "/" +
                                            std::to_string(chapterQuests.size()) + ": " + activeQuest->name;
            } else {
                view.wikiUrl  = "https://wiki.guildwars2.com/wiki/" + WikiTitleToUrlSlug(qm.storyName);
                view.subtitle = "All chapter steps completed";
            }
        } else {
            bool done      = (completedQuests && completedQuests->count(entry.id) > 0);
            view.title     = qm.name;
            view.wikiUrl   = "https://wiki.guildwars2.com/wiki/" + WikiTitleToUrlSlug(qm.name);
            view.current   = done ? 1 : 0;
            view.completed = done;
            view.subtitle  = (!qm.goals.empty() && !qm.goals[0].activeText.empty())
                ? qm.goals[0].activeText
                : ("Lv." + std::to_string(qm.level));
        }
    } else {
        view.title    = entry.labelOverride.empty() ? ("Story #" + std::to_string(entry.id)) : entry.labelOverride;
        view.subtitle = "Loading story details...";
    }
    view.apiCompleted = view.completed;
    return view;
}

AddonSettings DeserializeSettings(const std::string& jsonStr) {
    AddonSettings s{};
    json doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) return s;

    s.apiKey               = SanitizeApiKey(doc.value("api_key", ""));
    s.showHud              = doc.value("show_hud", true);
    s.showManager          = doc.value("show_manager", false);
    s.lockHud              = doc.value("lock_hud", false);
    s.clickThroughWhenLock = doc.value("click_through_when_locked", false);
    s.hideInCombat         = doc.value("hide_in_combat", false);
    s.hideOutsideGameplay  = doc.value("hide_outside_gameplay", true);
    s.hideCompletedEntries = doc.value("hide_completed_entries", false);
    s.hideCompletedBits    = doc.value("hide_completed_bits", true);
    s.prioritizeCurrentMap = doc.value("prioritize_current_map", true);
    s.filterCurrentMapOnly = doc.value("filter_current_map_only", false);
    s.showProgressAlerts   = doc.value("show_progress_alerts", true);
    s.autoAdvanceQuestline = doc.value("auto_advance_questline", false);
    s.hudBgAlpha           = std::clamp(doc.value("hud_bg_alpha", 0.78f), 0.15f, 1.0f);
    s.hudMaxHeight         = std::clamp(doc.value("hud_max_height", 420.0f), 180.0f, 900.0f);
    s.refreshIntervalSec   = std::clamp(doc.value("refresh_interval_sec", 60), 30, 900);

    if (doc.contains("tracked") && doc["tracked"].is_array()) {
        for (const auto& t : doc["tracked"]) {
            if (!t.is_object() || !t.contains("id") || !t["id"].is_number_integer()) continue;
            int kindInt = t.value("kind", 0);
            if (kindInt != 0 && kindInt != 2) {
                // Silently drop legacy Item (1) and Currency (3) entries removed in v1.5.0
                continue;
            }
            TrackedEntry te{};
            te.kind          = (kindInt == 2) ? EntryKind::Quest : EntryKind::Achievement;
            te.id            = t["id"].get<int>();
            te.labelOverride = t.value("label", "");
            te.note          = t.value("note", "");
            te.expanded      = t.value("expanded", true);
            te.showChain     = t.value("show_chain", false);
            if (t.contains("local_bits") && t["local_bits"].is_array()) {
                for (const auto& lb : t["local_bits"]) {
                    if (!lb.is_number_integer()) continue;
                    int bidx = lb.get<int>();
                    if (bidx >= 0 && std::find(te.localCheckedBits.begin(), te.localCheckedBits.end(), bidx) == te.localCheckedBits.end()) {
                        te.localCheckedBits.push_back(bidx);
                    }
                }
            }
            s.tracked.push_back(std::move(te));
        }
    }
    return s;
}

std::string SerializeSettings(const AddonSettings& s) {
    json doc;
    doc["api_key"]                   = s.apiKey;
    doc["show_hud"]                  = s.showHud;
    doc["show_manager"]              = s.showManager;
    doc["lock_hud"]                  = s.lockHud;
    doc["click_through_when_locked"] = s.clickThroughWhenLock;
    doc["hide_in_combat"]            = s.hideInCombat;
    doc["hide_outside_gameplay"]     = s.hideOutsideGameplay;
    doc["hide_completed_entries"]    = s.hideCompletedEntries;
    doc["hide_completed_bits"]       = s.hideCompletedBits;
    doc["prioritize_current_map"]    = s.prioritizeCurrentMap;
    doc["filter_current_map_only"]   = s.filterCurrentMapOnly;
    doc["show_progress_alerts"]      = s.showProgressAlerts;
    doc["auto_advance_questline"]    = s.autoAdvanceQuestline;
    doc["hud_bg_alpha"]              = s.hudBgAlpha;
    doc["hud_max_height"]            = s.hudMaxHeight;
    doc["refresh_interval_sec"]      = s.refreshIntervalSec;

    json arr = json::array();
    for (const auto& te : s.tracked) {
        arr.push_back({
            {"kind", static_cast<int>(te.kind)},
            {"id", te.id},
            {"label", te.labelOverride},
            {"note", te.note},
            {"expanded", te.expanded},
            {"show_chain", te.showChain},
            {"local_bits", te.localCheckedBits}
        });
    }
    doc["tracked"] = std::move(arr);
    return doc.dump(2);
}

} // namespace gw2qh
