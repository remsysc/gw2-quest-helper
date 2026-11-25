#include "tracker_service.h"
#include "gw2_logic.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>

namespace gw2qh {

namespace fs = std::filesystem;

namespace {

std::string ReadFileUtf8(const fs::path& p) {
    std::ifstream ifs(p, std::ios::in | std::ios::binary);
    if (!ifs) return "";
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

bool WriteFileUtf8(const fs::path& p, const std::string& content) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return ofs.good();
}

bool HasPermission(const std::vector<std::string>& perms, const std::string& target) {
    return std::find(perms.begin(), perms.end(), target) != perms.end();
}

std::string JoinIdsCsv(const std::vector<int>& ids) {
    std::ostringstream oss;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) oss << ',';
        oss << ids[i];
    }
    return oss.str();
}

std::string ResolveAchievementIconUrl(int achId,
                                      const std::unordered_map<int, AchievementMeta>& achMeta,
                                      const std::unordered_map<int, ItemMeta>& itemMeta) {
    auto it = achMeta.find(achId);
    if (it == achMeta.end()) return "";
    if (!it->second.iconUrl.empty()) return it->second.iconUrl;
    for (int rItemId : it->second.rewardItemIds) {
        auto rIt = itemMeta.find(rItemId);
        if (rIt != itemMeta.end() && !rIt->second.iconUrl.empty()) {
            return rIt->second.iconUrl;
        }
    }
    return "";
}

} // namespace

TrackerService::TrackerService(std::string addonDir,
                               std::unique_ptr<IHttpClient> httpClient,
                               LogCallbackFn logger,
                               AlertCallbackFn alertCb)
    : addonDir_(std::move(addonDir)),
      http_(std::move(httpClient)),
      logger_(std::move(logger)),
      alertCb_(std::move(alertCb)) {
    std::error_code ec;
    fs::create_directories(addonDir_, ec);
    EnsureEnvTemplateExists();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        LoadSettingsFromDiskLocked();
        if (settings_.apiKey.empty()) {
            std::string envKey = ParseEnvApiKey(ReadFileUtf8(GetEnvFilePath()));
            if (!envKey.empty()) {
                settings_.apiKey = envKey;
                SaveSettingsToDiskLocked();
            }
        }
        RebuildEvaluatedCacheLocked();
    }
}

TrackerService::~TrackerService() {
    Stop();
}

void TrackerService::Start() {
    if (workerThread_.joinable()) return;
    stopRequested_.store(false);
    workerThread_ = std::thread(&TrackerService::WorkerLoop, this);
}

void TrackerService::Stop() {
    stopRequested_.store(true);
    cv_.notify_all();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    SaveSettingsToDiskLocked();
}

std::string TrackerService::GetEnvFilePath() const {
    return (fs::path(addonDir_) / ".env").string();
}

std::string TrackerService::GetSettingsFilePath() const {
    return (fs::path(addonDir_) / "settings.json").string();
}

void TrackerService::EnsureEnvTemplateExists() const {
    fs::path examplePath = fs::path(addonDir_) / ".env.example";
    if (!fs::exists(examplePath)) {
        std::string tmpl =
            "# Copy this file to .env in the same folder and set your GW2 API key.\n"
            "# Recommended scopes: account, progression, unlocks, characters\n"
            "GW2_API_KEY=\n";
        WriteFileUtf8(examplePath, tmpl);
    }
}

void TrackerService::LoadSettingsFromDiskLocked() {
    std::string raw = ReadFileUtf8(GetSettingsFilePath());
    if (!raw.empty()) {
        settings_ = DeserializeSettings(raw);
        if (DeduplicateTrackedLocked()) {
            SaveSettingsToDiskLocked();
        }
    }
}

void TrackerService::SaveSettingsToDiskLocked() {
    WriteFileUtf8(GetSettingsFilePath(), SerializeSettings(settings_));
}

void TrackerService::LoadStaticCachesFromDisk() {
    std::string qRaw = ReadFileUtf8(fs::path(addonDir_) / "quests_cache.json");
    std::string sRaw = ReadFileUtf8(fs::path(addonDir_) / "stories_cache.json");
    std::string mRaw = ReadFileUtf8(fs::path(addonDir_) / "map_names_cache.json");
    std::string cRaw = ReadFileUtf8(fs::path(addonDir_) / "ach_categories_cache.json");

    std::unordered_map<int, QuestMeta> qMap;
    std::unordered_map<int, std::string> sMap;
    std::unordered_map<uint32_t, std::string> mMap;
    std::unordered_map<int, AchievementCategoryMeta> cMap;

    if (!qRaw.empty()) {
        ParseQuestsMetaJson(qRaw, qMap);
        if (!sRaw.empty()) ParseStoriesMetaJson(sRaw, sMap);
        for (auto& kv : qMap) {
            auto sIt = sMap.find(kv.second.storyId);
            if (sIt != sMap.end()) kv.second.storyName = sIt->second;
        }
    }

    if (!mRaw.empty()) {
        auto mDoc = nlohmann::json::parse(mRaw, nullptr, false);
        if (!mDoc.is_discarded() && mDoc.is_object()) {
            for (auto it = mDoc.begin(); it != mDoc.end(); ++it) {
                if (!it.value().is_string()) continue;
                try {
                    uint32_t mid = static_cast<uint32_t>(std::stoul(it.key()));
                    if (mid > 0) mMap[mid] = it.value().get<std::string>();
                } catch (...) {
                }
            }
        }
    }

    if (!cRaw.empty()) {
        ParseAchievementCategoriesJson(cRaw, cMap);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!qMap.empty()) {
        questMeta_       = std::move(qMap);
        storyNames_      = std::move(sMap);
        allQuestsLoaded_ = true;
    }
    if (!mMap.empty()) {
        for (const auto& kv : mMap) {
            if (!mapNames_.count(kv.first)) mapNames_[kv.first] = kv.second;
        }
    }
    if (!cMap.empty()) {
        achCategories_ = std::move(cMap);
        achIdToCategoryId_.clear();
        for (const auto& kv : achCategories_) {
            for (int aid : kv.second.achievementIds) {
                achIdToCategoryId_[aid] = kv.first;
            }
        }
        allAchCategoriesLoaded_ = true;
    }
    RebuildEvaluatedCacheLocked();
}

void TrackerService::SaveQuestCacheToDisk(const std::string& questsJson, const std::string& storiesJson) {
    WriteFileUtf8(fs::path(addonDir_) / "quests_cache.json", questsJson);
    if (!storiesJson.empty()) WriteFileUtf8(fs::path(addonDir_) / "stories_cache.json", storiesJson);
}

void TrackerService::SaveMapNamesCacheToDiskLocked() {
    nlohmann::json obj = nlohmann::json::object();
    for (const auto& kv : mapNames_) {
        if (kv.first != 0 && !kv.second.empty()) {
            obj[std::to_string(kv.first)] = kv.second;
        }
    }
    WriteFileUtf8(fs::path(addonDir_) / "map_names_cache.json", obj.dump());
}

void TrackerService::SaveAchCategoriesCacheToDiskLocked() {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& kv : achCategories_) {
        const auto& c = kv.second;
        arr.push_back({
            {"id", c.id},
            {"name", c.name},
            {"icon", c.iconUrl},
            {"achievements", c.achievementIds}
        });
    }
    WriteFileUtf8(fs::path(addonDir_) / "ach_categories_cache.json", arr.dump());
}

void TrackerService::RegisterAchievementNamesLocked() {
    for (const auto& kv : achievementMeta_) {
        if (!kv.second.name.empty()) {
            achievementNameToId_[ToLower(kv.second.name)] = kv.first;
        }
    }
}

void TrackerService::UpdateLiveContext(const std::string& characterName, uint32_t mapId) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool changed = false;
    if (!characterName.empty() && characterName != activeCharacter_) {
        activeCharacter_  = characterName;
        dirtyCharContext_ = true;
        changed           = true;
    }
    if (mapId != 0 && mapId != activeMapId_) {
        bool isMapTransition = (activeMapId_ != 0);
        activeMapId_ = mapId;
        dirtyMeta_   = true;
        changed      = true;
        if (isMapTransition && !settings_.apiKey.empty()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - lastSyncTime_).count();
            if (!hasSyncedOnce_ || elapsed >= 15) {
                dirtySync_ = true;
            } else {
                nextCacheExpirySec_ = 15;
            }
        }
    }
    if (changed) {
        RebuildEvaluatedCacheLocked();
        cv_.notify_one();
    }
}

UiSnapshot TrackerService::GetUiSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    UiSnapshot snap{};
    snap.settings        = settings_;
    snap.accountName     = account_.accountName;
    snap.permissions     = account_.permissions;
    snap.tokenValid      = account_.tokenValid;
    snap.tokenError      = account_.tokenError;
    snap.activeCharacter = activeCharacter_;
    snap.activeMapId     = activeMapId_;
    auto mIt = mapNames_.find(activeMapId_);
    if (mIt != mapNames_.end()) snap.activeMapName = mIt->second;

    snap.syncInProgress = syncInProgress_;
    snap.hasSyncedOnce  = hasSyncedOnce_;
    if (hasSyncedOnce_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - lastSyncTime_).count();
        snap.secondsSinceLastSync = static_cast<int>(elapsed);
    } else {
        snap.secondsSinceLastSync = -1;
    }
    snap.lastStatusMessage      = lastStatusMessage_;
    snap.evaluatedEntries       = evaluatedCache_;
    snap.searchInProgress       = searchInProgress_;
    snap.searchStatus           = searchStatus_;
    snap.searchResults          = searchResults_;
    snap.inProgressLoading      = inProgressLoading_;
    snap.inProgressAchievements = inProgressAchievements_;
    return snap;
}

void TrackerService::SetApiKey(const std::string& rawKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.apiKey = SanitizeApiKey(rawKey);
    SaveSettingsToDiskLocked();
    dirtySync_ = true;
    lastStatusMessage_ = settings_.apiKey.empty() ? "API key cleared" : "Verifying API key...";
    cv_.notify_one();
}

bool TrackerService::ReloadApiKeyFromEnv() {
    std::string envKey = ParseEnvApiKey(ReadFileUtf8(GetEnvFilePath()));
    if (envKey.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastStatusMessage_ = "No GW2_API_KEY found in .env";
        return false;
    }
    SetApiKey(envKey);
    return true;
}

void TrackerService::TriggerSyncNow() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    if (lastManualSyncTime_.time_since_epoch().count() != 0) {
        auto sinceManual = std::chrono::duration_cast<std::chrono::seconds>(now - lastManualSyncTime_).count();
        if (sinceManual < 5) {
            lastStatusMessage_ = "Please wait a few seconds before manual sync";
            return;
        }
    }
    lastManualSyncTime_ = now;
    dirtySync_ = true;
    dirtyMeta_ = true;
    cv_.notify_one();
}

bool TrackerService::IsPinned(EntryKind kind, int id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& t : settings_.tracked) {
        if (t.kind == kind && t.id == id) return true;
    }
    return false;
}

void TrackerService::PinEntry(EntryKind kind, int id, const std::string& labelHint) {
    if (id <= 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& existing : settings_.tracked) {
        if (existing.kind == kind && existing.id == id) {
            SaveSettingsToDiskLocked();
            RebuildEvaluatedCacheLocked();
            return;
        }
    }

    TrackedEntry te{};
    te.kind          = kind;
    te.id            = id;
    te.labelOverride = labelHint;
    te.expanded      = true;
    te.showChain     = false;
    settings_.tracked.push_back(std::move(te));
    DeduplicateTrackedLocked();
    SaveSettingsToDiskLocked();
    dirtyMeta_ = true;
    RebuildEvaluatedCacheLocked();
    cv_.notify_one();
}

void TrackerService::ReplaceEntryWithNext(size_t index, EntryKind nextKind, int nextId, const std::string& labelHint) {
    if (nextId <= 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= settings_.tracked.size()) return;

    // If nextId is already pinned elsewhere, simply remove the completed index
    for (size_t i = 0; i < settings_.tracked.size(); ++i) {
        if (i != index && settings_.tracked[i].kind == nextKind && settings_.tracked[i].id == nextId) {
            settings_.tracked.erase(settings_.tracked.begin() + static_cast<std::ptrdiff_t>(index));
            DeduplicateTrackedLocked();
            SaveSettingsToDiskLocked();
            RebuildEvaluatedCacheLocked();
            return;
        }
    }

    TrackedEntry nextEntry{};
    nextEntry.kind          = nextKind;
    nextEntry.id            = nextId;
    nextEntry.labelOverride = labelHint;
    nextEntry.expanded      = true;
    nextEntry.showChain     = false;
    settings_.tracked[index] = std::move(nextEntry);
    DeduplicateTrackedLocked();

    SaveSettingsToDiskLocked();
    dirtyMeta_ = true;
    RebuildEvaluatedCacheLocked();
    cv_.notify_one();
}

void TrackerService::UnpinEntry(size_t index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= settings_.tracked.size()) return;
    settings_.tracked.erase(settings_.tracked.begin() + static_cast<std::ptrdiff_t>(index));
    SaveSettingsToDiskLocked();
    RebuildEvaluatedCacheLocked();
}

void TrackerService::MoveEntry(size_t index, int delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= settings_.tracked.size()) return;
    int targetIdx = static_cast<int>(index) + delta;
    if (targetIdx < 0 || targetIdx >= static_cast<int>(settings_.tracked.size())) return;
    std::swap(settings_.tracked[index], settings_.tracked[static_cast<size_t>(targetIdx)]);
    SaveSettingsToDiskLocked();
    RebuildEvaluatedCacheLocked();
}

void TrackerService::SetEntryExpanded(size_t index, bool expanded) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= settings_.tracked.size()) return;
    settings_.tracked[index].expanded = expanded;
    SaveSettingsToDiskLocked();
    RebuildEvaluatedCacheLocked();
}

void TrackerService::SetEntryShowChain(size_t index, bool showChain) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= settings_.tracked.size()) return;
    settings_.tracked[index].showChain = showChain;
    SaveSettingsToDiskLocked();
    RebuildEvaluatedCacheLocked();
}

void TrackerService::ToggleLocalBit(size_t entryIndex, int bitIndex) {
    if (bitIndex < 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (entryIndex >= settings_.tracked.size()) return;
    auto& bits = settings_.tracked[entryIndex].localCheckedBits;
    auto it = std::find(bits.begin(), bits.end(), bitIndex);
    if (it != bits.end()) {
        bits.erase(it);
    } else {
        bits.push_back(bitIndex);
    }
    SaveSettingsToDiskLocked();
    RebuildEvaluatedCacheLocked();
}

void TrackerService::UndoLastLocalBit(size_t entryIndex) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (entryIndex >= settings_.tracked.size()) return;
    auto& bits = settings_.tracked[entryIndex].localCheckedBits;
    if (bits.empty()) return;
    bits.pop_back();
    SaveSettingsToDiskLocked();
    RebuildEvaluatedCacheLocked();
}

void TrackerService::UpdateEntryNote(size_t index, const std::string& note) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= settings_.tracked.size()) return;
    settings_.tracked[index].note = note;
    SaveSettingsToDiskLocked();
    dirtyMeta_ = true;
    RebuildEvaluatedCacheLocked();
    cv_.notify_one();
}

void TrackerService::UpdateDisplaySettings(const AddonSettings& updated) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.showHud              = updated.showHud;
    settings_.lockHud              = updated.lockHud;
    settings_.clickThroughWhenLock = updated.clickThroughWhenLock;
    settings_.hideInCombat         = updated.hideInCombat;
    settings_.hideOutsideGameplay  = updated.hideOutsideGameplay;
    settings_.hideCompletedEntries = updated.hideCompletedEntries;
    settings_.hideCompletedBits    = updated.hideCompletedBits;
    settings_.prioritizeCurrentMap = updated.prioritizeCurrentMap;
    settings_.filterCurrentMapOnly = updated.filterCurrentMapOnly;
    settings_.showProgressAlerts   = updated.showProgressAlerts;
    settings_.autoAdvanceQuestline = updated.autoAdvanceQuestline;
    settings_.hudBgAlpha           = std::clamp(updated.hudBgAlpha, 0.15f, 1.0f);
    settings_.hudMaxHeight         = std::clamp(updated.hudMaxHeight, 180.0f, 900.0f);
    settings_.refreshIntervalSec   = std::clamp(updated.refreshIntervalSec, 30, 900);
    SaveSettingsToDiskLocked();
    RebuildEvaluatedCacheLocked();
    cv_.notify_one();
}

void TrackerService::SetShowHud(bool show) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.showHud = show;
    SaveSettingsToDiskLocked();
}

void TrackerService::ToggleShowHud() {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.showHud = !settings_.showHud;
    SaveSettingsToDiskLocked();
}

void TrackerService::SetShowManager(bool show) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.showManager = show;
}

void TrackerService::ToggleShowManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_.showManager = !settings_.showManager;
}

bool* TrackerService::GetShowManagerPtr() {
    return &settings_.showManager;
}

void TrackerService::RequestSearch(EntryKind kind, const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingSearchKind_  = kind;
    pendingSearchQuery_ = Trim(query);
    dirtySearch_        = true;
    searchInProgress_   = true;
    searchStatus_       = "Searching...";
    cv_.notify_one();
}

void TrackerService::RequestLoadInProgressAchievements() {
    std::lock_guard<std::mutex> lock(mutex_);
    dirtyInProgress_   = true;
    inProgressLoading_ = true;
    cv_.notify_one();
}

bool TrackerService::DeduplicateTrackedLocked() {
    std::map<std::pair<int, int>, size_t> firstIdxByKey;
    std::vector<TrackedEntry> deduped;
    deduped.reserve(settings_.tracked.size());
    bool removedAny = false;

    for (auto& te : settings_.tracked) {
        if (te.id <= 0) {
            removedAny = true;
            continue;
        }
        std::pair<int, int> key{static_cast<int>(te.kind), te.id};
        auto it = firstIdxByKey.find(key);
        if (it == firstIdxByKey.end()) {
            firstIdxByKey[key] = deduped.size();
            deduped.push_back(std::move(te));
        } else {
            removedAny = true;
            TrackedEntry& keep = deduped[it->second];
            if (keep.note.empty() && !te.note.empty()) {
                keep.note = te.note;
            }
            for (int bidx : te.localCheckedBits) {
                if (bidx >= 0 && std::find(keep.localCheckedBits.begin(), keep.localCheckedBits.end(), bidx) == keep.localCheckedBits.end()) {
                    keep.localCheckedBits.push_back(bidx);
                }
            }
        }
    }

    if (removedAny) {
        settings_.tracked = std::move(deduped);
    }
    return removedAny;
}

void TrackerService::RebuildEvaluatedCacheLocked() {
    std::string activeMapName;
    auto mIt = mapNames_.find(activeMapId_);
    if (mIt != mapNames_.end()) activeMapName = mIt->second;

    evaluatedCache_.clear();
    evaluatedCache_.reserve(settings_.tracked.size());
    for (size_t i = 0; i < settings_.tracked.size(); ++i) {
        evaluatedCache_.push_back(EvaluateTrackedEntry(
            settings_.tracked[i],
            i,
            account_,
            activeCharacter_,
            activeMapName,
            achievementMeta_,
            itemMeta_,
            questMeta_,
            skinNames_,
            miniNames_,
            wikiGuides_,
            achievementNameToId_,
            chainInfoByAchId_,
            navRoadmaps_));
    }

    if (settings_.prioritizeCurrentMap && !activeMapName.empty()) {
        std::stable_sort(evaluatedCache_.begin(), evaluatedCache_.end(),
                         [](const EvaluatedEntryView& a, const EvaluatedEntryView& b) {
                             if (a.matchesCurrentMap != b.matchesCurrentMap) {
                                 return a.matchesCurrentMap > b.matchesCurrentMap;
                             }
                             return false;
                         });
    }
}

template <typename Container>
std::vector<std::vector<int>> TrackerService::BatchIds(const Container& ids, size_t batchSize) const {
    std::vector<std::vector<int>> batches;
    std::vector<int> current;
    current.reserve(batchSize);
    for (int id : ids) {
        if (id <= 0) continue;
        current.push_back(id);
        if (current.size() >= batchSize) {
            batches.push_back(std::move(current));
            current.clear();
            current.reserve(batchSize);
        }
    }
    if (!current.empty()) batches.push_back(std::move(current));
    return batches;
}

void TrackerService::WorkerLoop() {
    LoadStaticCachesFromDisk();

    while (!stopRequested_.load()) {
        bool doSync   = false;
        bool doMeta   = false;
        bool doChar   = false;
        bool doSearch = false;
        bool doInProg = false;

        std::string apiKey;
        std::string activeChar;
        uint32_t mapId = 0;
        std::vector<TrackedEntry> trackedCopy;
        EntryKind searchKind = EntryKind::Achievement;
        std::string searchQuery;
        int intervalSec = 60;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            intervalSec = (nextCacheExpirySec_ > 0)
                ? std::min(settings_.refreshIntervalSec, nextCacheExpirySec_)
                : settings_.refreshIntervalSec;
            auto now = std::chrono::steady_clock::now();
            bool periodicDue = hasSyncedOnce_ && !settings_.apiKey.empty() &&
                (std::chrono::duration_cast<std::chrono::seconds>(now - lastSyncTime_).count() >= intervalSec);

            if (!dirtySync_ && !dirtyMeta_ && !dirtyCharContext_ && !dirtySearch_ && !dirtyInProgress_ && !periodicDue) {
                cv_.wait_for(lock, std::chrono::seconds(5), [this]() {
                    return stopRequested_.load() || dirtySync_ || dirtyMeta_ || dirtyCharContext_ || dirtySearch_ || dirtyInProgress_;
                });
            }
            if (stopRequested_.load()) break;

            intervalSec = (nextCacheExpirySec_ > 0)
                ? std::min(settings_.refreshIntervalSec, nextCacheExpirySec_)
                : settings_.refreshIntervalSec;
            now = std::chrono::steady_clock::now();
            periodicDue = hasSyncedOnce_ && !settings_.apiKey.empty() &&
                (std::chrono::duration_cast<std::chrono::seconds>(now - lastSyncTime_).count() >= intervalSec);

            doSync   = dirtySync_ || periodicDue;
            doMeta   = dirtyMeta_ || !allQuestsLoaded_;
            doChar   = dirtyCharContext_;
            doSearch = dirtySearch_;
            doInProg = dirtyInProgress_;

            dirtySync_        = false;
            dirtyMeta_        = false;
            dirtyCharContext_ = false;
            dirtySearch_      = false;
            dirtyInProgress_  = false;

            apiKey      = settings_.apiKey;
            activeChar  = activeCharacter_;
            mapId       = activeMapId_;
            trackedCopy = settings_.tracked;
            searchKind  = pendingSearchKind_;
            searchQuery = pendingSearchQuery_;
        }

        if (doSearch && !stopRequested_.load()) {
            PerformSearch(searchKind, searchQuery);
        }

        if (doMeta && !stopRequested_.load()) {
            EnsureMetadataLoaded(trackedCopy, mapId);
        }

        if ((doSync || doChar) && !stopRequested_.load()) {
            if (doSync) {
                PerformAccountSync(apiKey, activeChar);
                // If auto-advance swapped in a new achievement, immediately load its metadata & Wiki guide
                bool needsPostSyncMeta = false;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    needsPostSyncMeta = dirtyMeta_;
                    if (needsPostSyncMeta) {
                        dirtyMeta_ = false;
                        trackedCopy = settings_.tracked;
                    }
                }
                if (needsPostSyncMeta && !stopRequested_.load()) {
                    EnsureMetadataLoaded(trackedCopy, mapId);
                }
            } else if (doChar && !apiKey.empty() && !activeChar.empty()) {
                std::string path = "/v2/characters/" + UrlEncode(activeChar) + "/quests";
                auto resp = http_->GetHttps("api.guildwars2.com", path, apiKey, &stopRequested_);
                if (resp.ok()) {
                    std::unordered_set<int> qIds;
                    if (ParseIntArrayJson(resp.body, qIds)) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        account_.characterQuests[activeChar] = std::move(qIds);
                        RebuildEvaluatedCacheLocked();
                    }
                }
            }
        }

        if (doInProg && !stopRequested_.load()) {
            PerformLoadInProgressAchievements();
        }
    }
}

void TrackerService::FetchMissingAchievementMetas(const std::unordered_set<int>& ids) {
    std::vector<int> missing;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (int id : ids) {
            if (id > 0 && !achievementMeta_.count(id)) missing.push_back(id);
        }
    }
    for (const auto& batch : BatchIds(missing)) {
        if (stopRequested_.load()) return;
        auto resp = http_->GetHttps("api.guildwars2.com", "/v2/achievements?ids=" + JoinIdsCsv(batch) + "&lang=en", "", &stopRequested_);
        if (resp.ok()) {
            std::lock_guard<std::mutex> lock(mutex_);
            ParseAchievementsMetaJson(resp.body, achievementMeta_);
            RegisterAchievementNamesLocked();
        }
    }
}

void TrackerService::ResolveAchievementNamesBatch(const std::vector<std::string>& names) {
    std::vector<std::string> unknownNames;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& n : names) {
            std::string clean = SanitizeWikiSearchTerm(n);
            if (!clean.empty() && !achievementNameToId_.count(ToLower(clean))) {
                if (std::find(unknownNames.begin(), unknownNames.end(), clean) == unknownNames.end()) {
                    unknownNames.push_back(clean);
                }
            }
        }
    }

    for (size_t offset = 0; offset < unknownNames.size() && !stopRequested_.load(); offset += 15) {
        size_t end = std::min(unknownNames.size(), offset + 15);
        std::string orExpr;
        for (size_t i = offset; i < end; ++i) {
            if (i > offset) orExpr += "||";
            orExpr += unknownNames[i];
        }
        std::string smw = "[[Has context::Achievement]][[Has canonical name::" + orExpr +
                          "]]|?Has game id|?Has canonical name|limit=30";
        auto resp = http_->GetHttps(
            "wiki.guildwars2.com",
            "/api.php?action=ask&query=" + UrlEncode(smw) + "&format=json",
            "",
            &stopRequested_);
        if (resp.ok()) {
            auto found = ParseWikiAskResultsJson(resp.body, EntryKind::Achievement);
            std::unordered_set<int> newIds;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& f : found) {
                    if (!f.name.empty() && f.id > 0) {
                        achievementNameToId_[ToLower(f.name)] = f.id;
                        if (!achievementMeta_.count(f.id)) newIds.insert(f.id);
                    }
                }
            }
            FetchMissingAchievementMetas(newIds);
        }
    }
}

void TrackerService::ResolveWikiGuidesAndChainsForAchievements(const std::vector<int>& achIds) {
    for (int achId : achIds) {
        if (stopRequested_.load()) return;
        std::string achName;
        std::vector<int> rewardItems;
        std::vector<std::string> childCandidateNames;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (checkedWikiAchievements_.count(achId)) continue;
            auto it = achievementMeta_.find(achId);
            if (it == achievementMeta_.end() || it->second.name.empty()) continue;
            checkedWikiAchievements_.insert(achId);
            achName     = it->second.name;
            rewardItems = it->second.rewardItemIds;
            for (const auto& b : it->second.bits) {
                std::string extracted = ExtractCollectionTargetFromBitText(b.text);
                if (!extracted.empty()) {
                    childCandidateNames.push_back(extracted);
                } else if (b.type == BitType::Text && !b.text.empty()) {
                    childCandidateNames.push_back(b.text);
                }
            }
        }

        QuestlineChainInfo chainInfo{};

        // 1. Fetch Wiki page wikitext (step guides + navbox template name)
        std::string parsePath = "/api.php?action=parse&page=" + WikiTitleToUrlSlug(achName) +
                                "&prop=wikitext&redirects=1&format=json";
        auto pResp = http_->GetHttps("wiki.guildwars2.com", parsePath, "", &stopRequested_);
        if (pResp.ok()) {
            WikiAchievementGuide guide = ParseWikiAchievementGuideJson(pResp.body, achName);
            chainInfo.navTemplateName  = guide.navTemplateName;

            for (auto& step : guide.steps) {
                if (!step.waypointChatLink.empty() || step.waypointName.empty() || stopRequested_.load()) continue;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto wpIt = waypointLinkCache_.find(step.waypointName);
                    if (wpIt != waypointLinkCache_.end()) {
                        step.waypointChatLink = wpIt->second;
                        continue;
                    }
                }
                std::string cleanWp = SanitizeWikiSearchTerm(step.waypointName);
                if (!cleanWp.empty()) {
                    std::string wpQuery = "[[Has canonical name::" + cleanWp + "]]|?Has game id|limit=1";
                    auto wpResp = http_->GetHttps(
                        "wiki.guildwars2.com",
                        "/api.php?action=ask&query=" + UrlEncode(wpQuery) + "&format=json",
                        "",
                        &stopRequested_);
                    if (wpResp.ok()) {
                        int poiId = ParseWikiFirstGameIdJson(wpResp.body);
                        std::string encoded = EncodeMapChatLink(poiId);
                        step.waypointChatLink = encoded;
                        std::lock_guard<std::mutex> lock(mutex_);
                        waypointLinkCache_[step.waypointName] = encoded;
                    }
                }
            }

            std::lock_guard<std::mutex> lock(mutex_);
            wikiGuides_[achId] = std::move(guide);
        }

        // 2. If page had a Questline Navbox Template (e.g. `Skyscale unlock nav`), parse & resolve all stages!
        if (!chainInfo.navTemplateName.empty() && !stopRequested_.load()) {
            bool needNav = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!checkedNavTemplates_.count(chainInfo.navTemplateName)) {
                    checkedNavTemplates_.insert(chainInfo.navTemplateName);
                    needNav = true;
                }
            }
            if (needNav) {
                std::string navSlug = "Template:" + WikiTitleToUrlSlug(chainInfo.navTemplateName);
                auto navResp = http_->GetHttps(
                    "wiki.guildwars2.com",
                    "/api.php?action=parse&page=" + navSlug + "&prop=wikitext&redirects=1&format=json",
                    "",
                    &stopRequested_);
                if (navResp.ok()) {
                    QuestlineRoadmapMeta rm = ParseWikiNavboxRoadmapJson(navResp.body, chainInfo.navTemplateName);
                    if (rm.loaded) {
                        // Resolve the main Stage achievement IDs FIRST so they are never truncated by child limits!
                        std::vector<std::string> stageNames;
                        std::vector<std::string> childNames;
                        for (const auto& st : rm.stages) {
                            stageNames.push_back(st.stageTitle);
                            for (const auto& ch : st.childTitles) childNames.push_back(ch);
                        }
                        ResolveAchievementNamesBatch(stageNames);
                        ResolveAchievementNamesBatch(childNames);

                        std::lock_guard<std::mutex> lock(mutex_);
                        for (auto& st : rm.stages) {
                            auto sIt = achievementNameToId_.find(ToLower(st.stageTitle));
                            if (sIt != achievementNameToId_.end()) st.stageAchId = sIt->second;
                            st.childAchIds.resize(st.childTitles.size(), 0);
                            for (size_t c = 0; c < st.childTitles.size(); ++c) {
                                auto cIt = achievementNameToId_.find(ToLower(st.childTitles[c]));
                                if (cIt != achievementNameToId_.end()) st.childAchIds[c] = cIt->second;
                            }
                        }
                        navRoadmaps_[chainInfo.navTemplateName] = std::move(rm);
                    }
                }
            }
        }

        // 3. Universal Parent Discovery via Reward Trophy Item (`[[Has context::Item]][[Has game id::X]]|?Is part of collection`)
        for (int rItemId : rewardItems) {
            if (stopRequested_.load() || chainInfo.parentAchievementId > 0) break;
            std::string parentQuery = "[[Has context::Item]][[Has game id::" + std::to_string(rItemId) +
                                      "]]|?Is part of collection|limit=1";
            auto pColResp = http_->GetHttps(
                "wiki.guildwars2.com",
                "/api.php?action=ask&query=" + UrlEncode(parentQuery) + "&format=json",
                "",
                &stopRequested_);
            if (pColResp.ok()) {
                std::string parentName = ParseWikiParentCollectionNameJson(pColResp.body);
                if (!parentName.empty()) {
                    ResolveAchievementNamesBatch({parentName});
                    int parentId = 0;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto pIt = achievementNameToId_.find(ToLower(parentName));
                        if (pIt != achievementNameToId_.end()) parentId = pIt->second;
                    }
                    if (parentId > 0 && parentId != achId) {
                        FetchMissingAchievementMetas({parentId});
                        chainInfo.parentAchievementId = parentId;
                        chainInfo.parentTitle         = parentName;

                        // Inspect parent's bits for sibling sub-collections and NPC turn-in transition hint!
                        std::vector<std::string> parentSiblingNames;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            auto pmIt = achievementMeta_.find(parentId);
                            if (pmIt != achievementMeta_.end()) {
                                const auto& pbits = pmIt->second.bits;
                                for (size_t bi = 0; bi < pbits.size(); ++bi) {
                                    std::string sub = ExtractCollectionTargetFromBitText(pbits[bi].text);
                                    if (!sub.empty()) parentSiblingNames.push_back(sub);
                                    // If this bit matches our reward item, check the very next bit for NPC turn-in instructions!
                                    if (pbits[bi].type == BitType::Item && pbits[bi].id == rItemId &&
                                        bi + 1 < pbits.size() && ExtractCollectionTargetFromBitText(pbits[bi + 1].text).empty()) {
                                        chainInfo.transitionHint = pbits[bi + 1].text;
                                    }
                                }
                            }
                        }
                        if (!parentSiblingNames.empty()) {
                            ResolveAchievementNamesBatch(parentSiblingNames);
                        }
                    }
                }
            }
        }

        // 4. Universal Direct Successor Discovery (strictly this achievement's own prerequisite links)
        if (!stopRequested_.load()) {
            std::string prereqCond = "~*" + SanitizeWikiSearchTerm(achName) + "*";
            std::string succQuery = "[[Has context::Achievement]][[Has prerequisite::" + prereqCond +
                                    "]]|?Has game id|?Has canonical name|limit=10";
            auto succResp = http_->GetHttps(
                "wiki.guildwars2.com",
                "/api.php?action=ask&query=" + UrlEncode(succQuery) + "&format=json",
                "",
                &stopRequested_);
            if (succResp.ok()) {
                auto succs = ParseWikiAskResultsJson(succResp.body, EntryKind::Achievement);
                for (const auto& s : succs) {
                    if (s.id > 0 && s.id != achId && s.id != chainInfo.parentAchievementId) {
                        chainInfo.nextAchievementId = s.id;
                        chainInfo.nextTitle         = s.name;
                        FetchMissingAchievementMetas({s.id});
                        break;
                    }
                }
            }
        }

        // 5. Resolve any child sub-achievement names inside this achievement itself (if it's a Meta-Achievement)
        if (!childCandidateNames.empty() && childCandidateNames.size() <= 25 && !stopRequested_.load()) {
            ResolveAchievementNamesBatch(childCandidateNames);
        }

        chainInfo.resolved = true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            chainInfoByAchId_[achId] = std::move(chainInfo);
        }
    }
}

void TrackerService::EnsureAchievementCategoriesLoaded() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (allAchCategoriesLoaded_) return;
    }

    auto listResp = http_->GetHttps("api.guildwars2.com", "/v2/achievements/categories", "", &stopRequested_);
    if (!listResp.ok() || stopRequested_.load()) return;

    std::unordered_set<int> allIds;
    if (!ParseIntArrayJson(listResp.body, allIds) || allIds.empty()) return;

    std::vector<int> missingIds;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        missingIds.reserve(allIds.size());
        for (int cid : allIds) {
            if (cid > 0 && !achCategories_.count(cid)) missingIds.push_back(cid);
        }
    }

    std::unordered_map<int, AchievementCategoryMeta> fetched;
    for (const auto& batch : BatchIds(missingIds, 180)) {
        if (stopRequested_.load()) return;
        auto batchResp = http_->GetHttps(
            "api.guildwars2.com",
            "/v2/achievements/categories?ids=" + JoinIdsCsv(batch) + "&lang=en",
            "",
            &stopRequested_);
        if (batchResp.ok()) {
            ParseAchievementCategoriesJson(batchResp.body, fetched);
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : fetched) {
        achCategories_[kv.first] = std::move(kv.second);
    }
    achIdToCategoryId_.clear();
    for (const auto& kv : achCategories_) {
        for (int aid : kv.second.achievementIds) {
            achIdToCategoryId_[aid] = kv.first;
        }
    }
    if (!achCategories_.empty()) {
        allAchCategoriesLoaded_ = true;
        SaveAchCategoriesCacheToDiskLocked();
    }
}

void TrackerService::EnsureMetadataLoaded(const std::vector<TrackedEntry>& entries, uint32_t mapId) {
    bool needQuests = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        needQuests = !allQuestsLoaded_;
    }

    if (needQuests && !stopRequested_.load()) {
        auto qResp = http_->GetHttps("api.guildwars2.com", "/v2/quests?ids=all&lang=en", "", &stopRequested_);
        auto sResp = http_->GetHttps("api.guildwars2.com", "/v2/stories?ids=all&lang=en", "", &stopRequested_);
        if (qResp.ok()) {
            std::unordered_map<int, QuestMeta> qMap;
            std::unordered_map<int, std::string> sMap;
            ParseQuestsMetaJson(qResp.body, qMap);
            if (sResp.ok()) ParseStoriesMetaJson(sResp.body, sMap);
            for (auto& kv : qMap) {
                auto sIt = sMap.find(kv.second.storyId);
                if (sIt != sMap.end()) kv.second.storyName = sIt->second;
            }
            SaveQuestCacheToDisk(qResp.body, sResp.ok() ? sResp.body : "");
            std::lock_guard<std::mutex> lock(mutex_);
            questMeta_       = std::move(qMap);
            storyNames_      = std::move(sMap);
            allQuestsLoaded_ = true;
            RebuildEvaluatedCacheLocked();
        }
    }

    if (mapId != 0 && !stopRequested_.load()) {
        bool hasMap = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            hasMap = mapNames_.count(mapId) > 0 || failedMapIds_.count(mapId) > 0;
        }
        if (!hasMap) {
            auto mResp = http_->GetHttps("api.guildwars2.com", "/v2/maps/" + std::to_string(mapId) + "?lang=en", "", &stopRequested_);
            std::string mapName;
            if (mResp.ok() && ParseMapNameJson(mResp.body, mapName)) {
                std::lock_guard<std::mutex> lock(mutex_);
                mapNames_[mapId] = mapName;
                SaveMapNamesCacheToDiskLocked();
                RebuildEvaluatedCacheLocked();
            } else if (mResp.statusCode == 404) {
                std::lock_guard<std::mutex> lock(mutex_);
                failedMapIds_.insert(mapId);
                mapNames_[mapId] = "Instance #" + std::to_string(mapId);
                SaveMapNamesCacheToDiskLocked();
                RebuildEvaluatedCacheLocked();
            }
        }
    }

    std::unordered_set<int> missingAch;
    std::unordered_set<int> missingItems;
    std::vector<int> pinnedAchIds;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& e : entries) {
            if (e.kind == EntryKind::Achievement) {
                pinnedAchIds.push_back(e.id);
                if (!achievementMeta_.count(e.id)) missingAch.insert(e.id);
            }
        }
    }

    FetchMissingAchievementMetas(missingAch);

    // Populate sub-objectives for CategoryDisplay meta-achievements (e.g. "Return to the Domain of Istan")
    bool needCategoryLookup = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (int achId : pinnedAchIds) {
            auto it = achievementMeta_.find(achId);
            if (it != achievementMeta_.end() &&
                it->second.isCategoryDisplay &&
                it->second.bits.empty() &&
                it->second.categoryAchievementIds.empty()) {
                needCategoryLookup = true;
                break;
            }
        }
    }
    if (needCategoryLookup && !stopRequested_.load()) {
        EnsureAchievementCategoriesLoaded();
        std::unordered_set<int> missingCatSubAch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (int achId : pinnedAchIds) {
                auto it = achievementMeta_.find(achId);
                if (it == achievementMeta_.end() ||
                    !it->second.isCategoryDisplay ||
                    !it->second.bits.empty() ||
                    !it->second.categoryAchievementIds.empty()) {
                    continue;
                }
                auto catIdIt = achIdToCategoryId_.find(achId);
                if (catIdIt == achIdToCategoryId_.end()) continue;
                auto catIt = achCategories_.find(catIdIt->second);
                if (catIt == achCategories_.end()) continue;

                const auto& cat = catIt->second;
                if (it->second.iconUrl.empty() && !cat.iconUrl.empty()) {
                    it->second.iconUrl = cat.iconUrl;
                }
                for (int subId : cat.achievementIds) {
                    if (subId <= 0 || subId == achId) continue;
                    it->second.categoryAchievementIds.push_back(subId);
                    if (!achievementMeta_.count(subId)) missingCatSubAch.insert(subId);
                }
            }
        }
        if (!missingCatSubAch.empty()) {
            FetchMissingAchievementMetas(missingCatSubAch);
        }
    }

    std::unordered_set<int> missingSkins;
    std::unordered_set<int> missingMinis;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& e : entries) {
            if (e.kind != EntryKind::Achievement) continue;
            auto it = achievementMeta_.find(e.id);
            if (it == achievementMeta_.end()) continue;
            for (const auto& b : it->second.bits) {
                if (b.type == BitType::Item && b.id > 0 && !itemMeta_.count(b.id)) missingItems.insert(b.id);
                else if (b.type == BitType::Skin && b.id > 0 && !skinNames_.count(b.id)) missingSkins.insert(b.id);
                else if (b.type == BitType::MiniPet && b.id > 0 && !miniNames_.count(b.id)) missingMinis.insert(b.id);
            }
            for (int rItemId : it->second.rewardItemIds) {
                if (rItemId > 0 && !itemMeta_.count(rItemId)) missingItems.insert(rItemId);
            }
        }
    }

    for (const auto& batch : BatchIds(missingItems)) {
        if (stopRequested_.load()) return;
        auto resp = http_->GetHttps("api.guildwars2.com", "/v2/items?ids=" + JoinIdsCsv(batch) + "&lang=en", "", &stopRequested_);
        if (resp.ok()) {
            std::lock_guard<std::mutex> lock(mutex_);
            ParseItemsMetaJson(resp.body, itemMeta_);
        }
    }

    for (const auto& batch : BatchIds(missingSkins)) {
        if (stopRequested_.load()) return;
        auto resp = http_->GetHttps("api.guildwars2.com", "/v2/skins?ids=" + JoinIdsCsv(batch) + "&lang=en", "", &stopRequested_);
        if (resp.ok()) {
            std::lock_guard<std::mutex> lock(mutex_);
            ParseIdNameArrayJson(resp.body, skinNames_);
        }
    }

    for (const auto& batch : BatchIds(missingMinis)) {
        if (stopRequested_.load()) return;
        auto resp = http_->GetHttps("api.guildwars2.com", "/v2/minis?ids=" + JoinIdsCsv(batch) + "&lang=en", "", &stopRequested_);
        if (resp.ok()) {
            std::lock_guard<std::mutex> lock(mutex_);
            ParseIdNameArrayJson(resp.body, miniNames_);
        }
    }

    if (!pinnedAchIds.empty() && !stopRequested_.load()) {
        ResolveWikiGuidesAndChainsForAchievements(pinnedAchIds);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        RebuildEvaluatedCacheLocked();
    }
}

void TrackerService::PerformAccountSync(const std::string& apiKey, const std::string& activeChar) {
    if (apiKey.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        account_ = AccountSnapshot{};
        syncInProgress_ = false;
        lastStatusMessage_ = "Set GW2 API key in Settings or .env";
        RebuildEvaluatedCacheLocked();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        syncInProgress_ = true;
        lastStatusMessage_ = "Syncing GW2 account data...";
    }

    AccountSnapshot nextSnap{};

    auto tokResp = http_->GetHttps("api.guildwars2.com", "/v2/tokeninfo", apiKey, &stopRequested_);
    if (stopRequested_.load()) return;
    if (!tokResp.ok()) {
        std::string apiErr;
        if (!ParseApiErrorMessage(tokResp.body, apiErr)) {
            apiErr = tokResp.error.empty() ? ("HTTP " + std::to_string(tokResp.statusCode)) : tokResp.error;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        account_.tokenValid = false;
        account_.tokenError = apiErr;
        syncInProgress_ = false;
        lastStatusMessage_ = "API key error: " + apiErr;
        return;
    }

    if (!ParseTokenInfoJson(tokResp.body, nextSnap.permissions, nextSnap.tokenError)) {
        std::lock_guard<std::mutex> lock(mutex_);
        account_.tokenValid = false;
        account_.tokenError = nextSnap.tokenError;
        syncInProgress_ = false;
        lastStatusMessage_ = "Token error: " + nextSnap.tokenError;
        return;
    }
    nextSnap.tokenValid = true;

    auto accResp = http_->GetHttps("api.guildwars2.com", "/v2/account", apiKey, &stopRequested_);
    if (accResp.ok()) ParseAccountJson(accResp.body, nextSnap.accountName);

    bool hasCharacters  = HasPermission(nextSnap.permissions, "characters");
    bool hasProgression = HasPermission(nextSnap.permissions, "progression");
    bool hasUnlocks     = HasPermission(nextSnap.permissions, "unlocks");

    int achCacheRemainingSec = -1;
    if (hasProgression && !stopRequested_.load()) {
        auto achResp = http_->GetHttps("api.guildwars2.com", "/v2/account/achievements", apiKey, &stopRequested_);
        if (achResp.ok()) {
            ParseAccountAchievementsJson(achResp.body, nextSnap.achievements);
            achCacheRemainingSec = achResp.cacheRemainingSec;
        }
    }

    if (hasUnlocks && !stopRequested_.load()) {
        auto skinsResp = http_->GetHttps("api.guildwars2.com", "/v2/account/skins", apiKey, &stopRequested_);
        if (skinsResp.ok()) ParseIntArrayJson(skinsResp.body, nextSnap.unlockedSkins);

        auto minisResp = http_->GetHttps("api.guildwars2.com", "/v2/account/minis", apiKey, &stopRequested_);
        if (minisResp.ok()) ParseIntArrayJson(minisResp.body, nextSnap.unlockedMinis);
    }

    if (hasCharacters && hasProgression && !stopRequested_.load()) {
        std::string targetChar = activeChar;
        if (targetChar.empty()) {
            auto charsResp = http_->GetHttps("api.guildwars2.com", "/v2/characters", apiKey, &stopRequested_);
            if (charsResp.ok()) {
                std::vector<std::string> charNames;
                if (ParseStringArrayJson(charsResp.body, charNames) && !charNames.empty()) {
                    targetChar = charNames.front();
                }
            }
        }
        if (!targetChar.empty() && !stopRequested_.load()) {
            auto qResp = http_->GetHttps(
                "api.guildwars2.com",
                "/v2/characters/" + UrlEncode(targetChar) + "/quests",
                apiKey,
                &stopRequested_);
            if (qResp.ok()) {
                std::unordered_set<int> qIds;
                if (ParseIntArrayJson(qResp.body, qIds)) {
                    nextSnap.characterQuests[targetChar] = std::move(qIds);
                }
            }
        }
    }

    if (stopRequested_.load()) return;

    std::vector<std::string> alertsToSend;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool wasSyncedBefore = hasSyncedOnce_;
        bool wantAlerts      = settings_.showProgressAlerts && static_cast<bool>(alertCb_);
        bool wantAutoAdvance = settings_.autoAdvanceQuestline;

        struct PrevState { int apiCurrent; bool apiCompleted; };
        std::map<std::pair<int, int>, PrevState> prevMap;
        if (wasSyncedBefore) {
            for (const auto& ev : evaluatedCache_) {
                int apiCur = std::max(0, ev.current - ev.localCheckedCount);
                prevMap[{static_cast<int>(ev.entry.kind), ev.entry.id}] = {apiCur, ev.apiCompleted};
            }
        }

        for (const auto& kv : account_.characterQuests) {
            if (!nextSnap.characterQuests.count(kv.first)) {
                nextSnap.characterQuests[kv.first] = kv.second;
            }
        }
        account_        = std::move(nextSnap);
        syncInProgress_ = false;
        hasSyncedOnce_  = true;
        lastSyncTime_   = std::chrono::steady_clock::now();
        nextCacheExpirySec_ = (achCacheRemainingSec >= 0)
            ? std::clamp(achCacheRemainingSec + 2, 15, settings_.refreshIntervalSec)
            : -1;
        lastStatusMessage_ = "Synced (" + std::to_string(account_.achievements.size()) + " achievements)";

        // Reconcile optimistic local check-offs against freshly synced API/unlock state
        bool anyReconciled = false;
        for (auto& te : settings_.tracked) {
            if (te.kind != EntryKind::Achievement || te.localCheckedBits.empty()) continue;
            auto progIt = account_.achievements.find(te.id);
            if (progIt != account_.achievements.end() &&
                (progIt->second.done || progIt->second.repeated > 0)) {
                te.localCheckedBits.clear();
                anyReconciled = true;
                continue;
            }

            auto metaIt = achievementMeta_.find(te.id);
            auto isBitNowApiCompleted = [&](int bitIdx) -> bool {
                if (progIt != account_.achievements.end() && progIt->second.completedBits.count(bitIdx) > 0) {
                    return true;
                }
                if (metaIt == achievementMeta_.end()) return false;
                const auto& meta = metaIt->second;
                if (meta.bits.empty() && meta.isCategoryDisplay &&
                    bitIdx >= 0 && static_cast<size_t>(bitIdx) < meta.categoryAchievementIds.size()) {
                    int subId = meta.categoryAchievementIds[static_cast<size_t>(bitIdx)];
                    auto subProgIt = account_.achievements.find(subId);
                    if (subProgIt != account_.achievements.end() &&
                        (subProgIt->second.done || subProgIt->second.repeated > 0)) {
                        return true;
                    }
                } else if (bitIdx >= 0 && static_cast<size_t>(bitIdx) < meta.bits.size()) {
                    const auto& bm = meta.bits[static_cast<size_t>(bitIdx)];
                    if (bm.type == BitType::Skin && bm.id > 0 && account_.unlockedSkins.count(bm.id) > 0) return true;
                    if (bm.type == BitType::MiniPet && bm.id > 0 && account_.unlockedMinis.count(bm.id) > 0) return true;
                }
                return false;
            };

            size_t beforeSz = te.localCheckedBits.size();
            te.localCheckedBits.erase(
                std::remove_if(te.localCheckedBits.begin(), te.localCheckedBits.end(), isBitNowApiCompleted),
                te.localCheckedBits.end());
            if (te.localCheckedBits.size() != beforeSz) {
                anyReconciled = true;
            }
        }
        if (anyReconciled) {
            SaveSettingsToDiskLocked();
        }

        RebuildEvaluatedCacheLocked();

        if (wasSyncedBefore) {
            std::set<std::pair<int, int>> initiallyPinned;
            for (const auto& te : settings_.tracked) {
                initiallyPinned.emplace(static_cast<int>(te.kind), te.id);
            }
            std::set<std::pair<int, int>> advancedTargets;
            bool anyAutoAdvanced = false;

            for (const auto& ev : evaluatedCache_) {
                auto it = prevMap.find({static_cast<int>(ev.entry.kind), ev.entry.id});
                if (it == prevMap.end()) continue;
                const auto& prev = it->second;
                int apiCur = std::max(0, ev.current - ev.localCheckedCount);

                if (!prev.apiCompleted && ev.apiCompleted) {
                    if (wantAutoAdvance && ev.chain.hasNext && ev.chain.nextId > 0 &&
                        ev.originalIndex < settings_.tracked.size()) {
                        std::pair<int, int> targetKey{static_cast<int>(ev.chain.nextKind), ev.chain.nextId};
                        bool alreadyPresent = initiallyPinned.count(targetKey) > 0 || advancedTargets.count(targetKey) > 0;

                        TrackedEntry nextEntry{};
                        nextEntry.kind          = ev.chain.nextKind;
                        nextEntry.id            = ev.chain.nextId;
                        nextEntry.labelOverride = ev.chain.nextTitle;
                        nextEntry.expanded      = true;
                        settings_.tracked[ev.originalIndex] = std::move(nextEntry);
                        anyAutoAdvanced = true;
                        dirtyMeta_      = true;
                        advancedTargets.insert(targetKey);
                        if (wantAlerts) {
                            if (alreadyPresent) {
                                alertsToSend.push_back("Quest Helper: " + ev.title + " COMPLETED!");
                            } else {
                                alertsToSend.push_back("Quest Helper: " + ev.title + " DONE! Up next: " + ev.chain.nextTitle);
                            }
                        }
                    } else if (wantAlerts) {
                        alertsToSend.push_back("Quest Helper: " + ev.title + " COMPLETED!");
                    }
                } else if (wantAlerts && apiCur > prev.apiCurrent) {
                    int delta = apiCur - prev.apiCurrent;
                    alertsToSend.push_back("Quest Helper: " + ev.title + " (" +
                                           std::to_string(apiCur) + "/" + std::to_string(ev.target) +
                                           ", +" + std::to_string(delta) + ")");
                }
            }

            if (anyAutoAdvanced) {
                DeduplicateTrackedLocked();
                SaveSettingsToDiskLocked();
                RebuildEvaluatedCacheLocked();
            }
        }
    }

    for (const auto& msg : alertsToSend) {
        alertCb_(msg);
    }
}

void TrackerService::PerformSearch(EntryKind kind, const std::string& query) {
    std::vector<SearchResultItem> results;

    if (query.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        searchInProgress_ = false;
        searchResults_.clear();
        searchStatus_ = "Enter a collection/achievement or storyline name, or a numeric ID.";
        return;
    }

    DecodedChatLink decoded = DecodeChatLink(query);
    if (decoded.valid && decoded.id > 0) {
        if (kind == EntryKind::Achievement) {
            auto resp = http_->GetHttps("api.guildwars2.com", "/v2/achievements/" + std::to_string(decoded.id) + "?lang=en", "", &stopRequested_);
            if (resp.ok()) {
                std::unordered_map<int, AchievementMeta> tmp;
                ParseAchievementsMetaJson(resp.body, tmp);
                if (tmp.count(decoded.id)) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    achievementMeta_[decoded.id] = tmp[decoded.id];
                    RegisterAchievementNamesLocked();
                    SearchResultItem sr{};
                    sr.kind     = EntryKind::Achievement;
                    sr.id       = decoded.id;
                    sr.name     = tmp[decoded.id].name;
                    sr.subtitle = tmp[decoded.id].requirement;
                    sr.iconUrl  = ResolveAchievementIconUrl(decoded.id, achievementMeta_, itemMeta_);
                    auto aIt    = account_.achievements.find(decoded.id);
                    if (aIt != account_.achievements.end()) {
                        sr.current     = aIt->second.current;
                        sr.max         = std::max(1, aIt->second.max > 0 ? aIt->second.max : tmp[decoded.id].maxCount);
                        sr.hasProgress = true;
                    } else {
                        sr.max = std::max(1, tmp[decoded.id].maxCount);
                    }
                    results.push_back(std::move(sr));
                }
            }
        }
        std::lock_guard<std::mutex> lock(mutex_);
        searchInProgress_ = false;
        searchResults_    = std::move(results);
        searchStatus_     = searchResults_.empty()
            ? ("Chat link ID #" + std::to_string(decoded.id) + " did not match an achievement.")
            : "Found chat link match.";
        return;
    }

    bool allDigits = !query.empty() && std::all_of(query.begin(), query.end(), [](unsigned char c) {
        return std::isdigit(c);
    });
    if (allDigits) {
        int id = 0;
        try { id = std::stoi(query); } catch (...) { id = 0; }
        if (id > 0) {
            if (kind == EntryKind::Achievement) {
                auto resp = http_->GetHttps("api.guildwars2.com", "/v2/achievements/" + std::to_string(id) + "?lang=en", "", &stopRequested_);
                if (resp.ok()) {
                    std::unordered_map<int, AchievementMeta> tmp;
                    ParseAchievementsMetaJson(resp.body, tmp);
                    if (tmp.count(id)) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        achievementMeta_[id] = tmp[id];
                        RegisterAchievementNamesLocked();
                        SearchResultItem sr{};
                        sr.kind     = EntryKind::Achievement;
                        sr.id       = id;
                        sr.name     = tmp[id].name;
                        sr.subtitle = tmp[id].requirement;
                        sr.iconUrl  = ResolveAchievementIconUrl(id, achievementMeta_, itemMeta_);
                        auto aIt    = account_.achievements.find(id);
                        if (aIt != account_.achievements.end()) {
                            sr.current     = aIt->second.current;
                            sr.max         = std::max(1, aIt->second.max > 0 ? aIt->second.max : tmp[id].maxCount);
                            sr.hasProgress = true;
                        } else {
                            sr.max = std::max(1, tmp[id].maxCount);
                        }
                        results.push_back(std::move(sr));
                    }
                }
            } else {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = questMeta_.find(id);
                if (it != questMeta_.end()) {
                    SearchResultItem sr{};
                    sr.kind     = EntryKind::Quest;
                    sr.id       = id;
                    sr.name     = it->second.storyName.empty() ? it->second.name : it->second.storyName;
                    sr.subtitle = it->second.name;
                    results.push_back(std::move(sr));
                }
            }
        }
        std::lock_guard<std::mutex> lock(mutex_);
        searchInProgress_ = false;
        searchResults_    = std::move(results);
        searchStatus_     = searchResults_.empty() ? "No match for ID #" + query : "Found exact ID match.";
        return;
    }

    if (kind == EntryKind::Quest) {
        std::lock_guard<std::mutex> lock(mutex_);
        // Group quests by Storyline (storyId) so users pin the Story Chapter and track its active step automatically
        std::unordered_set<int> seenStories;
        std::vector<const QuestMeta*> sortedQuests;
        sortedQuests.reserve(questMeta_.size());
        for (const auto& kv : questMeta_) sortedQuests.push_back(&kv.second);
        std::sort(sortedQuests.begin(), sortedQuests.end(), [](const QuestMeta* a, const QuestMeta* b) {
            if (a->level != b->level) return a->level < b->level;
            return a->id < b->id;
        });

        for (const QuestMeta* qm : sortedQuests) {
            if (ContainsCaseInsensitive(qm->name, query) ||
                ContainsCaseInsensitive(qm->storyName, query)) {
                if (qm->storyId > 0 && !qm->storyName.empty()) {
                    if (seenStories.count(qm->storyId)) continue;
                    seenStories.insert(qm->storyId);
                    int repId     = qm->id;
                    int stepCount = 0;
                    for (const QuestMeta* other : sortedQuests) {
                        if (other->storyId == qm->storyId) {
                            if (stepCount == 0) repId = other->id;
                            ++stepCount;
                        }
                    }
                    SearchResultItem sr{};
                    sr.kind     = EntryKind::Quest;
                    sr.id       = repId;
                    sr.name     = qm->storyName;
                    sr.subtitle = "Storyline (" + std::to_string(stepCount) + " steps, Lv." + std::to_string(qm->level) + ")";
                    results.push_back(std::move(sr));
                } else {
                    SearchResultItem sr{};
                    sr.kind     = EntryKind::Quest;
                    sr.id       = qm->id;
                    sr.name     = qm->name;
                    sr.subtitle = "Story (Lv." + std::to_string(qm->level) + ")";
                    results.push_back(std::move(sr));
                }
                if (results.size() >= 50) break;
            }
        }
        std::sort(results.begin(), results.end(), [](const SearchResultItem& a, const SearchResultItem& b) {
            return a.name < b.name;
        });
        searchInProgress_ = false;
        searchResults_    = std::move(results);
        searchStatus_     = "Found " + std::to_string(searchResults_.size()) + " storyline(s).";
        return;
    }

    std::string cleanTerm = SanitizeWikiSearchTerm(query);
    if (cleanTerm.size() < 2) {
        std::lock_guard<std::mutex> lock(mutex_);
        searchInProgress_ = false;
        searchStatus_     = "Please enter at least 2 characters.";
        return;
    }

    std::string smwCond  = BuildWikiSmwCondition(cleanTerm);
    std::string smwQuery = "[[Has context::Achievement]][[Has canonical name::" + smwCond +
                           "]]|?Has game id|?Has canonical name|limit=150";
    std::string path     = "/api.php?action=ask&query=" + UrlEncode(smwQuery) + "&format=json";

    auto resp = http_->GetHttps("wiki.guildwars2.com", path, "", &stopRequested_);
    if (!resp.ok()) {
        std::lock_guard<std::mutex> lock(mutex_);
        searchInProgress_ = false;
        searchStatus_     = "Wiki search failed (" + (resp.error.empty() ? ("HTTP " + std::to_string(resp.statusCode)) : resp.error) + ")";
        return;
    }

    results = ParseWikiAskResultsJson(resp.body, EntryKind::Achievement);

    // Fallback: if Wiki SMW returned 0 hits, search locally cached achievement metadata by all tokens
    if (results.empty()) {
        std::vector<std::string> tokens;
        std::istringstream tiss(cleanTerm);
        std::string tok;
        while (tiss >> tok) tokens.push_back(tok);

        if (!tokens.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& kv : achievementMeta_) {
                const auto& m = kv.second;
                if (!m.loaded || m.name.empty()) continue;
                bool allMatch = true;
                for (const auto& t : tokens) {
                    if (!ContainsCaseInsensitive(m.name, t)) {
                        allMatch = false;
                        break;
                    }
                }
                if (allMatch) {
                    SearchResultItem sr{};
                    sr.kind     = EntryKind::Achievement;
                    sr.id       = m.id;
                    sr.name     = m.name;
                    sr.subtitle = m.requirement;
                    results.push_back(std::move(sr));
                    if (results.size() >= 50) break;
                }
            }
        }
    }

    // Fetch metadata (icons + requirements + tiers) for top 25 uncached results in a single batch
    std::string lowerQuery = ToLower(cleanTerm);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& r : results) {
            achievementNameToId_[ToLower(r.name)] = r.id;
            auto it = account_.achievements.find(r.id);
            if (it != account_.achievements.end()) {
                r.current     = it->second.current;
                r.max         = it->second.max;
                r.hasProgress = true;
            }
        }
        auto rankScore = [&lowerQuery](const SearchResultItem& item) -> int {
            std::string ln = ToLower(item.name);
            if (ln == lowerQuery) return 0;
            if (ln.find(lowerQuery) == 0) return 1;
            if (item.hasProgress && item.current > 0) return 2;
            return 3;
        };
        std::sort(results.begin(), results.end(), [&](const SearchResultItem& a, const SearchResultItem& b) {
            int sa = rankScore(a);
            int sb = rankScore(b);
            if (sa != sb) return sa < sb;
            if (a.name.size() != b.name.size()) return a.name.size() < b.name.size();
            return a.name < b.name;
        });
    }

    std::unordered_set<int> topMetaIds;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < results.size() && topMetaIds.size() < 25; ++i) {
            if (!achievementMeta_.count(results[i].id)) topMetaIds.insert(results[i].id);
        }
    }
    if (!topMetaIds.empty() && !stopRequested_.load()) {
        FetchMissingAchievementMetas(topMetaIds);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& r : results) {
            r.iconUrl = ResolveAchievementIconUrl(r.id, achievementMeta_, itemMeta_);
            auto mIt  = achievementMeta_.find(r.id);
            if (mIt != achievementMeta_.end() && mIt->second.loaded) {
                if (!mIt->second.requirement.empty()) r.subtitle = mIt->second.requirement;
                if (r.max <= 0) r.max = std::max(1, mIt->second.maxCount);
                if (r.hasProgress && r.max <= 0) r.max = std::max(1, mIt->second.maxCount);
            }
        }
        searchInProgress_ = false;
        searchResults_    = std::move(results);
        searchStatus_     = "Found " + std::to_string(searchResults_.size()) + " result(s).";
    }
}

void TrackerService::PerformLoadInProgressAchievements() {
    std::vector<int> missingMetaIds;
    std::vector<AccountAchievementProgress> candidates;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& kv : account_.achievements) {
            if (!kv.second.done && kv.second.current > 0) {
                candidates.push_back(kv.second);
                if (!achievementMeta_.count(kv.first)) missingMetaIds.push_back(kv.first);
            }
        }
    }

    if (missingMetaIds.size() > 400) missingMetaIds.resize(400);
    std::unordered_set<int> missingSet(missingMetaIds.begin(), missingMetaIds.end());
    FetchMissingAchievementMetas(missingSet);

    std::vector<SearchResultItem> out;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& prog : candidates) {
            auto mIt = achievementMeta_.find(prog.id);
            if (mIt == achievementMeta_.end() || mIt->second.name.empty()) continue;
            SearchResultItem item{};
            item.kind        = EntryKind::Achievement;
            item.id          = prog.id;
            item.name        = mIt->second.name;
            item.subtitle    = mIt->second.requirement;
            item.iconUrl     = ResolveAchievementIconUrl(prog.id, achievementMeta_, itemMeta_);
            item.current     = prog.current;
            item.max         = std::max(1, prog.max > 0 ? prog.max : mIt->second.maxCount);
            item.hasProgress = true;
            out.push_back(std::move(item));
        }
        std::sort(out.begin(), out.end(), [](const SearchResultItem& a, const SearchResultItem& b) {
            float ratioA = static_cast<float>(a.current) / static_cast<float>(std::max(1, a.max));
            float ratioB = static_cast<float>(b.current) / static_cast<float>(std::max(1, b.max));
            if (ratioA != ratioB) return ratioA > ratioB;
            return a.name < b.name;
        });
        inProgressAchievements_ = std::move(out);
        inProgressLoading_      = false;
    }
}

} // namespace gw2qh
