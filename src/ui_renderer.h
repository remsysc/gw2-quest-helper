#pragma once

#include "tracker_service.h"
#include <functional>
#include <string>

struct ImFont;

namespace gw2qh {

struct LiveGameState {
    bool        isGameplay = true;
    bool        isInCombat = false;
    std::string characterName;
    uint32_t    mapId = 0;
    ImFont*     uiFont = nullptr;
};

using IconTextureLookupFn = std::function<void*(const std::string& iconUrl)>;
using OpenExternalUrlFn   = std::function<void(const std::string& url)>;

class UiRenderer {
public:
    UiRenderer(TrackerService& service,
               IconTextureLookupFn iconLookup = nullptr,
               OpenExternalUrlFn openUrl = nullptr);

    void RenderWindows(const LiveGameState& gameState);
    void RenderNexusOptions();

private:
    void RenderHud(const UiSnapshot& snap, const LiveGameState& gameState);
    void RenderManager(const UiSnapshot& snap);

    void RenderTabSearch(const UiSnapshot& snap);
    void RenderTabInProgress(const UiSnapshot& snap);
    void RenderTabPinned(const UiSnapshot& snap);
    void RenderTabSettings(const UiSnapshot& snap);

    void DrawIconIfReady(const std::string& iconUrl, float sizePx = 18.0f);

    TrackerService&     service_;
    IconTextureLookupFn iconLookup_;
    OpenExternalUrlFn   openUrl_;

    // Local UI input state
    int  searchKindIdx_          = 0; // 0=Achievement/Collection, 1=Storyline
    char searchBuf_[256]         = {};
    char inProgressFilter_[128]  = {};
    bool requestedInitialInProg_ = false;

    char apiKeyBuf_[128]         = {};
    bool apiKeyBufInitialized_   = false;
    bool showApiKeyPlain_        = false;

    int  editingPinnedIndex_     = -1;
    char editNoteBuf_[256]       = {};

    std::string lastCopiedWp_;
    float       hudBodyContentHeight_ = 0.0f;
};

} // namespace gw2qh
