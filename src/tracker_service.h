#pragma once

#include "http_client.h"
#include "models.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gw2qh {

struct UiSnapshot {
    AddonSettings                      settings;
    std::string                        accountName;
    std::vector<std::string>           permissions;
    bool                               tokenValid = false;
    std::string                        tokenError;

    std::string                        activeCharacter;
    uint32_t                           activeMapId = 0;
    std::string                        activeMapName;

    bool                               syncInProgress = false;
    bool                               hasSyncedOnce  = false;
    int                                secondsSinceLastSync = -1;
    std::string                        lastStatusMessage;

    std::vector<EvaluatedEntryView>    evaluatedEntries;

    bool                               searchInProgress = false;
    std::string                        searchStatus;
    std::vector<SearchResultItem>      searchResults;

    bool                               inProgressLoading = false;
    std::vector<SearchResultItem>      inProgressAchievements;
};

using LogCallbackFn   = std::function<void(int level, const std::string& msg)>;
using AlertCallbackFn = std::function<void(const std::string& alertMsg)>;

class TrackerService {
public:
    TrackerService(std::string addonDir,
                   std::unique_ptr<IHttpClient> httpClient,
                   LogCallbackFn logger = nullptr,
                   AlertCallbackFn alertCb = nullptr);
    ~TrackerService();

    TrackerService(const TrackerService&) = delete;
    TrackerService& operator=(const TrackerService&) = delete;

    void Start();
    void Stop();

    void UpdateLiveContext(const std::string& characterName, uint32_t mapId);
    UiSnapshot GetUiSnapshot() const;

    void SetApiKey(const std::string& rawKey);
    bool ReloadApiKeyFromEnv();
    void TriggerSyncNow();

    void PinEntry(EntryKind kind, int id, const std::string& labelHint = "");
    void ReplaceEntryWithNext(size_t index, EntryKind nextKind, int nextId, const std::string& labelHint = "");
    void UnpinEntry(size_t index);
    void MoveEntry(size_t index, int delta);
    void SetEntryExpanded(size_t index, bool expanded);
    void SetEntryShowChain(size_t index, bool showChain);
    void ToggleLocalBit(size_t entryIndex, int bitIndex);
    void UndoLastLocalBit(size_t entryIndex);
    void UpdateEntryNote(size_t index, const std::string& note);
    bool IsPinned(EntryKind kind, int id) const;

    void UpdateDisplaySettings(const AddonSettings& updated);
    void SetShowHud(bool show);
    void ToggleShowHud();
    void SetShowManager(bool show);
    void ToggleShowManager();
    bool* GetShowManagerPtr();

    void RequestSearch(EntryKind kind, const std::string& query);
    void RequestLoadInProgressAchievements();

    std::string GetEnvFilePath() const;
    std::string GetSettingsFilePath() const;

private:
    void WorkerLoop();
    void LoadSettingsFromDiskLocked();
    void SaveSettingsToDiskLocked();
    void EnsureEnvTemplateExists() const;
    void LoadStaticCachesFromDisk();
    void SaveQuestCacheToDisk(const std::string& questsJson, const std::string& storiesJson);
    void SaveMapNamesCacheToDiskLocked();
    void SaveAchCategoriesCacheToDiskLocked();

    void PerformAccountSync(const std::string& apiKey, const std::string& activeChar);
    void EnsureAchievementCategoriesLoaded();
    void EnsureMetadataLoaded(const std::vector<TrackedEntry>& entries, uint32_t mapId);
    void ResolveWikiGuidesAndChainsForAchievements(const std::vector<int>& achIds);
    void ResolveAchievementNamesBatch(const std::vector<std::string>& names);
    void FetchMissingAchievementMetas(const std::unordered_set<int>& ids);

    void PerformSearch(EntryKind kind, const std::string& query);
    void PerformLoadInProgressAchievements();
    bool DeduplicateTrackedLocked();
    void RebuildEvaluatedCacheLocked();
    void RegisterAchievementNamesLocked();

    template <typename Container>
    std::vector<std::vector<int>> BatchIds(const Container& ids, size_t batchSize = 180) const;

    std::string                  addonDir_;
    std::unique_ptr<IHttpClient> http_;
    LogCallbackFn                logger_;
    AlertCallbackFn              alertCb_;

    mutable std::mutex           mutex_;
    std::condition_variable      cv_;
    std::atomic<bool>            stopRequested_{false};
    std::thread                  workerThread_;

    AddonSettings                   settings_;
    AccountSnapshot                 account_;
    std::vector<EvaluatedEntryView> evaluatedCache_;

    std::string                               activeCharacter_;
    uint32_t                                  activeMapId_ = 0;
    std::unordered_map<uint32_t, std::string> mapNames_;
    std::unordered_set<uint32_t>              failedMapIds_;

    std::unordered_map<int, AchievementMeta>         achievementMeta_;
    std::unordered_map<std::string, int>             achievementNameToId_;
    std::unordered_map<int, AchievementCategoryMeta> achCategories_;
    std::unordered_map<int, int>                     achIdToCategoryId_;
    std::unordered_map<int, ItemMeta>                itemMeta_;
    std::unordered_map<int, QuestMeta>               questMeta_;
    std::unordered_map<int, std::string>             storyNames_;
    std::unordered_map<int, std::string>             skinNames_;
    std::unordered_map<int, std::string>             miniNames_;

    std::unordered_map<int, WikiAchievementGuide>  wikiGuides_;
    std::unordered_set<int>                        checkedWikiAchievements_;
    std::unordered_map<std::string, std::string>   waypointLinkCache_;

    std::unordered_map<int, QuestlineChainInfo>    chainInfoByAchId_;
    std::unordered_map<std::string, QuestlineRoadmapMeta> navRoadmaps_;
    std::unordered_set<std::string>                checkedNavTemplates_;

    bool                                           allQuestsLoaded_        = false;
    bool                                           allAchCategoriesLoaded_ = false;

    bool        dirtySync_         = true;
    bool        dirtyMeta_         = true;
    bool        dirtyCharContext_  = false;
    bool        dirtyInProgress_   = false;
    bool        dirtySearch_       = false;
    EntryKind   pendingSearchKind_ = EntryKind::Achievement;
    std::string pendingSearchQuery_;

    bool                                  syncInProgress_ = false;
    bool                                  hasSyncedOnce_  = false;
    std::chrono::steady_clock::time_point lastSyncTime_{};
    std::chrono::steady_clock::time_point lastManualSyncTime_{};
    int                                   nextCacheExpirySec_ = -1;
    std::string                           lastStatusMessage_ = "Ready";

    bool                          searchInProgress_ = false;
    std::string                   searchStatus_;
    std::vector<SearchResultItem> searchResults_;

    bool                          inProgressLoading_ = false;
    std::vector<SearchResultItem> inProgressAchievements_;
};

} // namespace gw2qh
