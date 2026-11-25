#pragma once

#include "models.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gw2qh {

// String & formatting utilities
std::string Trim(const std::string& s);
std::string ToLower(const std::string& s);
bool        ContainsCaseInsensitive(const std::string& haystack, const std::string& needle);
bool        ContainsWholePhraseCaseInsensitive(const std::string& haystack, const std::string& needle);
std::string StripGw2Markup(const std::string& text);
std::string UrlEncode(const std::string& value);
std::string WikiTitleToUrlSlug(const std::string& title);
std::string SanitizeWikiSearchTerm(const std::string& query);
std::vector<std::string> BuildWikiCaseVariants(const std::string& cleanTerm);
std::string BuildWikiSmwCondition(const std::string& cleanTerm);
std::string SanitizeApiKey(const std::string& raw);
std::string ParseEnvApiKey(const std::string& envContent);
bool        SplitRenderIconUrl(const std::string& fullUrl, std::string& outRemote, std::string& outEndpoint);
std::string ExtractCollectionTargetFromBitText(const std::string& bitText);

// Chat link & HTTP header utilities
DecodedChatLink DecodeChatLink(const std::string& text);
std::string     EncodeMapChatLink(int poiId);
int             ParseHttpCacheRemainingSec(const std::string& cacheControl, const std::string& ageHeader);

// GW2 REST API JSON parsers
bool ParseTokenInfoJson(const std::string& jsonStr, std::vector<std::string>& outPermissions, std::string& outError);
bool ParseAccountJson(const std::string& jsonStr, std::string& outAccountName);
bool ParseApiErrorMessage(const std::string& jsonStr, std::string& outMessage);

bool ParseAccountAchievementsJson(const std::string& jsonStr,
                                  std::unordered_map<int, AccountAchievementProgress>& outAchievements);
bool ParseIntArrayJson(const std::string& jsonStr, std::unordered_set<int>& outIds);
bool ParseStringArrayJson(const std::string& jsonStr, std::vector<std::string>& outStrings);

// Static metadata parsers
void ParseAchievementsMetaJson(const std::string& jsonStr, std::unordered_map<int, AchievementMeta>& metaMap);
void ParseAchievementCategoriesJson(const std::string& jsonStr,
                                    std::unordered_map<int, AchievementCategoryMeta>& outCategories);
void ParseItemsMetaJson(const std::string& jsonStr, std::unordered_map<int, ItemMeta>& metaMap);
void ParseQuestsMetaJson(const std::string& jsonStr, std::unordered_map<int, QuestMeta>& metaMap);
void ParseStoriesMetaJson(const std::string& jsonStr, std::unordered_map<int, std::string>& storyNames);
void ParseIdNameArrayJson(const std::string& jsonStr, std::unordered_map<int, std::string>& idToName);
bool ParseMapNameJson(const std::string& jsonStr, std::string& outMapName);

// GW2 Wiki SMW, Wikitext walkthrough, & Questline Navbox parsers
std::vector<SearchResultItem> ParseWikiAskResultsJson(const std::string& jsonStr, EntryKind kind);
int                           ParseWikiFirstGameIdJson(const std::string& jsonStr);
std::string                   ParseWikiParentCollectionNameJson(const std::string& jsonStr);
std::string                   CleanWikitextMarkup(const std::string& raw, std::string* outFirstWaypointName = nullptr);
WikiAchievementGuide          ParseWikiAchievementGuideJson(const std::string& parseJsonStr, const std::string& fallbackTitle);
QuestlineRoadmapMeta          ParseWikiNavboxRoadmapJson(const std::string& parseJsonStr, const std::string& templateName);

// Evaluates a pinned entry against account snapshot + metadata + wiki guides + questline chains
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
    const std::unordered_map<std::string, QuestlineRoadmapMeta>& navRoadmaps);

// Settings persistence
AddonSettings DeserializeSettings(const std::string& jsonStr);
std::string   SerializeSettings(const AddonSettings& settings);

} // namespace gw2qh
