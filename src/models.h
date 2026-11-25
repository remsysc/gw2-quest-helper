#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gw2qh {

enum class EntryKind : int {
    Achievement = 0,
    Quest       = 2
};

enum class BitType : int {
    Text    = 0,
    Item    = 1,
    Skin    = 2,
    MiniPet = 3,
    Unknown = 4
};

struct AchievementBitMeta {
    int         index = 0;
    BitType     type  = BitType::Text;
    int         id    = 0;       // Item/Skin/Mini ID when applicable
    std::string text;            // Explicit text from API bit
};

struct AchievementMeta {
    int                             id = 0;
    std::string                     name;
    std::string                     description;
    std::string                     requirement;
    std::string                     lockedText;
    std::string                     type;
    std::string                     iconUrl;
    int                             maxCount = 1;
    bool                            isCategoryDisplay = false;
    std::vector<int>                categoryAchievementIds;
    std::vector<AchievementBitMeta> bits;
    std::vector<int>                rewardItemIds;
    std::vector<int>                prerequisiteIds;
    bool                            loaded = false;
};

struct AchievementCategoryMeta {
    int              id = 0;
    std::string      name;
    std::string      iconUrl;
    std::vector<int> achievementIds;
};

struct ItemMeta {
    int         id = 0;
    std::string name;
    std::string type;
    std::string rarity;
    std::string description;
    std::string iconUrl;
    bool        loaded = false;
};

struct QuestGoalMeta {
    std::string activeText;
    std::string completeText;
};

struct QuestMeta {
    int                        id = 0;
    std::string                name;
    int                        level   = 0;
    int                        storyId = 0;
    std::string                storyName;
    std::vector<QuestGoalMeta> goals;
    bool                       loaded = false;
};

struct WikiStepGuide {
    std::string rowTitle;
    std::string area;
    std::string cleanNotes;
    std::string waypointName;
    std::string waypointChatLink; // e.g. [&BNILAAA=]
};

struct WikiAchievementGuide {
    std::string                pageTitle;
    std::string                pageUrl;
    std::string                introText;
    std::string                navTemplateName; // e.g. "Skyscale unlock nav"
    std::vector<WikiStepGuide> steps;
    bool                       loaded = false;
};

struct QuestlineStageMeta {
    std::string              stageTitle;     // e.g. "Newborn Skyscales"
    int                      stageAchId = 0; // Resolved achievement ID if applicable
    std::vector<std::string> childTitles;    // e.g. {"Skyscale Scales", ..., "Skyscale Eggs"}
    std::vector<int>         childAchIds;
};

struct QuestlineRoadmapMeta {
    std::string                     questlineTitle; // e.g. "Skyscale unlock achievements"
    std::vector<QuestlineStageMeta> stages;
    bool                            loaded = false;
};

struct QuestlineChainInfo {
    int                  parentAchievementId = 0;
    std::string          parentTitle;
    int                  nextAchievementId   = 0; // Direct successor via Has prerequisite or navbox
    std::string          nextTitle;
    std::string          transitionHint;          // e.g. "Return to Gorrik in Dragonfall..."
    std::string          navTemplateName;
    bool                 resolved = false;
};

// User-pinned entry persisted to settings.json
struct TrackedEntry {
    EntryKind        kind         = EntryKind::Achievement;
    int              id           = 0;
    std::string      labelOverride;
    std::string      note;
    bool             expanded     = true;
    bool             showChain    = false; // Inline questline roadmap drawer toggle
    std::vector<int> localCheckedBits;     // Optimistic local step check-offs pending API sync
};

// Account-wide progress for a single achievement from /v2/account/achievements
struct AccountAchievementProgress {
    int                     id       = 0;
    int                     current  = 0;
    int                     max      = 0;
    bool                    done     = false;
    int                     repeated = 0;
    std::unordered_set<int> completedBits;
};

struct AccountSnapshot {
    std::string                                         accountName;
    std::vector<std::string>                            permissions;
    bool                                                tokenValid = false;
    std::string                                         tokenError;

    std::unordered_map<int, AccountAchievementProgress> achievements;
    std::unordered_set<int>                             unlockedSkins;
    std::unordered_set<int>                             unlockedMinis;

    std::unordered_map<std::string, std::unordered_set<int>> characterQuests;
};

struct EvaluatedBitView {
    int         index          = 0;
    std::string label;
    bool        apiCompleted   = false;
    bool        localCompleted = false;
    bool        completed      = false;
    BitType     type           = BitType::Text;
    int         referenceId    = 0;

    std::string area;
    std::string guideNotes;
    std::string waypointName;
    std::string waypointChatLink;
    int         childAchievementId = 0;
};

struct EvaluatedChainNode {
    EntryKind   kind          = EntryKind::Achievement;
    int         id            = 0;
    std::string title;
    int         current       = 0;
    int         max           = 1;
    bool        completed     = false;
    bool        isCurrent     = false;
    bool        isStageHeader = false;
    bool        canJump       = true; // False for linear Personal Story steps
};

struct EvaluatedChainContext {
    bool                            hasParent       = false;
    EntryKind                       parentKind      = EntryKind::Achievement;
    int                             parentId        = 0;
    std::string                     parentTitle;
    int                             parentCurrent   = 0;
    int                             parentMax       = 0;
    bool                            parentCompleted = false;

    std::string                     questlineTitle;  // e.g. "Skyscale Unlock"
    int                             stageIndex      = 0; // 1-based stage index
    int                             stageCount      = 0;

    bool                            hasNext         = false;
    EntryKind                       nextKind        = EntryKind::Achievement;
    int                             nextId          = 0;
    std::string                     nextTitle;
    std::string                     transitionHint;

    std::vector<EvaluatedChainNode> roadmapNodes;
};

struct EvaluatedEntryView {
    TrackedEntry                    entry;
    size_t                          originalIndex     = 0;
    std::string                     title;
    std::string                     subtitle;
    std::string                     iconUrl;
    std::string                     wikiUrl;
    int                             current           = 0;
    int                             target            = 1;
    bool                            apiCompleted      = false;
    bool                            completed         = false;
    int                             localCheckedCount = 0;
    bool                            metadataLoaded    = false;
    bool                            matchesCurrentMap = false;

    std::vector<EvaluatedBitView>   bits;
    EvaluatedChainContext           chain;
};

struct SearchResultItem {
    EntryKind   kind = EntryKind::Achievement;
    int         id   = 0;
    std::string name;
    std::string subtitle;
    std::string iconUrl;
    int         current = 0;
    int         max     = 0;
    bool        hasProgress = false;
};

struct AddonSettings {
    std::string               apiKey;
    bool                      showHud              = true;
    bool                      showManager          = false;
    bool                      lockHud              = false;
    bool                      clickThroughWhenLock = false;
    bool                      hideInCombat         = false;
    bool                      hideOutsideGameplay  = true;
    bool                      hideCompletedEntries = false;
    bool                      hideCompletedBits    = true;
    bool                      prioritizeCurrentMap = true;
    bool                      filterCurrentMapOnly = false;
    bool                      showProgressAlerts   = true;
    bool                      autoAdvanceQuestline = false; // Auto-replace completed goal with next in chain
    float                     hudBgAlpha           = 0.78f;
    float                     hudMaxHeight         = 420.0f;
    int                       refreshIntervalSec   = 60;
    std::vector<TrackedEntry> tracked;
};

struct DecodedChatLink {
    bool    valid = false;
    uint8_t type  = 0;
    int     id    = 0;
    int     count = 1;
};

} // namespace gw2qh
