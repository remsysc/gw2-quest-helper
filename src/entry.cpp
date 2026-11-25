#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>

#include "Nexus.h"
#include "Mumble.h"
#include "imgui.h"

#include "gw2_logic.h"
#include "http_client.h"
#include "tracker_service.h"
#include "ui_renderer.h"
#include "version.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace {

constexpr const char* kAddonName        = "GW2 Quest Helper";
constexpr const char* kAddonFolder      = "GW2QuestHelper";
constexpr const char* kKbToggleManager  = "KB_GW2QH_TOGGLE_MANAGER";
constexpr const char* kKbToggleHud      = "KB_GW2QH_TOGGLE_HUD";
constexpr const char* kManagerWinName   = "Quest Helper - Manager##GW2QHManager";

AddonDefinition_t                        g_AddonDef{};
HMODULE                                  g_hSelf          = nullptr;
AddonAPI_t*                              g_API            = nullptr;
NexusLinkData_t*                         g_NexusLink      = nullptr;
Mumble::Data*                            g_MumbleLink     = nullptr;
Mumble::Identity*                        g_MumbleIdentity = nullptr;

std::unique_ptr<gw2qh::TrackerService>   g_Service;
std::unique_ptr<gw2qh::UiRenderer>       g_Renderer;

std::mutex                               g_IdentityMutex;
std::string                              g_EventCharName;
uint32_t                                 g_EventMapId = 0;

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), needed);
    return out;
}

std::string WideToUtf8(const wchar_t* wstr, size_t maxChars) {
    if (!wstr || wstr[0] == L'\0') return "";
    size_t len = 0;
    while (len < maxChars && wstr[len] != L'\0') ++len;
    if (len == 0) return "";
    int needed = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return "";
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(len), out.data(), needed, nullptr, nullptr);
    return out;
}

void* LookupOrLoadIconTexture(const std::string& iconUrl) {
    if (!g_API || !g_API->Textures_GetOrCreateFromURL || iconUrl.empty()) return nullptr;
    std::string remote;
    std::string endpoint;
    if (!gw2qh::SplitRenderIconUrl(iconUrl, remote, endpoint)) return nullptr;
    std::string texId = "GW2QH_ICON_" + endpoint;
    Texture_t* tex = g_API->Textures_GetOrCreateFromURL(texId.c_str(), remote.c_str(), endpoint.c_str());
    return (tex && tex->Resource) ? tex->Resource : nullptr;
}

void OpenExternalHttpsUrl(const std::string& url) {
    if (url.rfind("https://", 0) != 0) return; // Only allow HTTPS URLs
    std::wstring wUrl = Utf8ToWide(url);
    if (!wUrl.empty()) {
        ShellExecuteW(nullptr, L"open", wUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void OnMumbleIdentityUpdated(void* aEventArgs) {
    if (!aEventArgs) return;
    auto* id = static_cast<Mumble::Identity*>(aEventArgs);
    char safeName[21] = {};
    std::memcpy(safeName, id->Name, 20);
    safeName[20] = '\0';

    std::lock_guard<std::mutex> lock(g_IdentityMutex);
    g_EventCharName = safeName;
    g_EventMapId    = id->MapID;
}

void OnKeybindProcess(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease || !aIdentifier || !g_Service) return;
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput) return;
    if (std::strcmp(aIdentifier, kKbToggleManager) == 0) {
        g_Service->ToggleShowManager();
    } else if (std::strcmp(aIdentifier, kKbToggleHud) == 0) {
        g_Service->ToggleShowHud();
    }
}

gw2qh::LiveGameState ReadLiveGameState() {
    gw2qh::LiveGameState st{};
    if (g_NexusLink) {
        st.isGameplay = g_NexusLink->IsGameplay;
        st.uiFont     = static_cast<ImFont*>(g_NexusLink->Font ? g_NexusLink->Font : g_NexusLink->FontUI);
    }
    if (g_MumbleLink) {
        st.isInCombat = (g_MumbleLink->Context.IsInCombat != 0);
        st.mapId      = g_MumbleLink->Context.MapID;
    }
    if (g_MumbleIdentity && g_MumbleIdentity->Name[0] != '\0') {
        char safeName[21] = {};
        std::memcpy(safeName, g_MumbleIdentity->Name, 20);
        safeName[20]     = '\0';
        st.characterName = safeName;
        if (st.mapId == 0) st.mapId = g_MumbleIdentity->MapID;
    } else {
        std::lock_guard<std::mutex> lock(g_IdentityMutex);
        if (!g_EventCharName.empty()) {
            st.characterName = g_EventCharName;
            if (st.mapId == 0) st.mapId = g_EventMapId;
        }
    }

    if (st.characterName.empty() && g_MumbleLink && g_MumbleLink->Identity[0] != L'\0') {
        std::string idJson = WideToUtf8(g_MumbleLink->Identity, 256);
        auto doc = nlohmann::json::parse(idJson, nullptr, false);
        if (!doc.is_discarded() && doc.is_object()) {
            if (doc.contains("name") && doc["name"].is_string()) {
                st.characterName = doc["name"].get<std::string>();
            }
            if (st.mapId == 0 && doc.contains("map_id") && doc["map_id"].is_number_unsigned()) {
                st.mapId = doc["map_id"].get<uint32_t>();
            }
        }
    }
    return st;
}

void AddonRender() {
    if (!g_Service || !g_Renderer) return;
    gw2qh::LiveGameState live = ReadLiveGameState();
    static std::string sLastChar;
    static uint32_t    sLastMapId = 0;
    if ((!live.characterName.empty() && live.characterName != sLastChar) ||
        (live.mapId != 0 && live.mapId != sLastMapId)) {
        if (!live.characterName.empty()) sLastChar = live.characterName;
        if (live.mapId != 0) sLastMapId = live.mapId;
        g_Service->UpdateLiveContext(live.characterName, live.mapId);
    }
    g_Renderer->RenderWindows(live);
}

void AddonOptions() {
    if (!g_Renderer) return;
    g_Renderer->RenderNexusOptions();
}

void AddonLoad(AddonAPI_t* aApi) {
    g_API = aApi;
    if (!g_API) return;

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(g_API->ImguiContext));
    ImGui::SetAllocatorFunctions(
        reinterpret_cast<void* (*)(size_t, void*)>(g_API->ImguiMalloc),
        reinterpret_cast<void (*)(void*, void*)>(g_API->ImguiFree));

    g_NexusLink      = static_cast<NexusLinkData_t*>(g_API->DataLink_Get(DL_NEXUS_LINK));
    g_MumbleLink     = static_cast<Mumble::Data*>(g_API->DataLink_Get(DL_MUMBLE_LINK));
    g_MumbleIdentity = static_cast<Mumble::Identity*>(g_API->DataLink_Get(DL_MUMBLE_LINK_IDENTITY));

    const char* rawDir = g_API->Paths_GetAddonDirectory(kAddonFolder);
    std::string addonDir = rawDir ? rawDir : "./addons/GW2QuestHelper";

    g_Service = std::make_unique<gw2qh::TrackerService>(
        addonDir,
        gw2qh::CreateWinHttpClient(),
        [](int level, const std::string& msg) {
            if (g_API && g_API->Log) {
                g_API->Log(static_cast<ELogLevel>(level), kAddonName, msg.c_str());
            }
        },
        [](const std::string& alertMsg) {
            if (g_API && g_API->GUI_SendAlert) {
                g_API->GUI_SendAlert(alertMsg.c_str());
            }
        });

    g_Renderer = std::make_unique<gw2qh::UiRenderer>(
        *g_Service,
        LookupOrLoadIconTexture,
        OpenExternalHttpsUrl);

    g_Service->Start();

    g_API->Events_Subscribe(EV_MUMBLE_IDENTITY_UPDATED, OnMumbleIdentityUpdated);
    g_API->InputBinds_RegisterWithString(kKbToggleManager, OnKeybindProcess, "ALT+SHIFT+Q");
    g_API->InputBinds_RegisterWithString(kKbToggleHud, OnKeybindProcess, "ALT+SHIFT+H");
    g_API->GUI_RegisterCloseOnEscape(kManagerWinName, g_Service->GetShowManagerPtr());

    g_API->GUI_Register(RT_Render, AddonRender);
    g_API->GUI_Register(RT_OptionsRender, AddonOptions);

    g_API->Log(LOGL_INFO, kAddonName,
               ("GW2 Quest Helper v" + std::string(GW2QH_VERSION_STR) +
                " loaded. Press ALT+SHIFT+Q to open Quest Manager.").c_str());
}

void AddonUnload() {
    if (g_API) {
        g_API->GUI_Deregister(AddonRender);
        g_API->GUI_Deregister(AddonOptions);
        g_API->GUI_DeregisterCloseOnEscape(kManagerWinName);
        g_API->InputBinds_Deregister(kKbToggleManager);
        g_API->InputBinds_Deregister(kKbToggleHud);
        g_API->Events_Unsubscribe(EV_MUMBLE_IDENTITY_UPDATED, OnMumbleIdentityUpdated);
    }

    if (g_Service) {
        g_Service->Stop();
    }
    g_Renderer.reset();
    g_Service.reset();

    if (g_API && g_API->Log) {
        g_API->Log(LOGL_INFO, kAddonName, "GW2 Quest Helper unloaded cleanly.");
    }

    g_NexusLink      = nullptr;
    g_MumbleLink     = nullptr;
    g_MumbleIdentity = nullptr;
    g_API            = nullptr;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hSelf = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    g_AddonDef.Signature        = static_cast<uint32_t>(-774921);
    g_AddonDef.APIVersion       = NEXUS_API_VERSION;
    g_AddonDef.Name             = kAddonName;
    g_AddonDef.Version.Major    = GW2QH_VERSION_MAJOR;
    g_AddonDef.Version.Minor    = GW2QH_VERSION_MINOR;
    g_AddonDef.Version.Build    = GW2QH_VERSION_PATCH;
    g_AddonDef.Version.Revision = GW2QH_VERSION_REVISION;
    g_AddonDef.Author           = "Shinhra";
    g_AddonDef.Description      = "Real-time questline, collection, achievement, and Wiki waypoint tracker for GW2.";
    g_AddonDef.Load             = AddonLoad;
    g_AddonDef.Unload           = AddonUnload;
    g_AddonDef.Flags            = AF_None;
    g_AddonDef.Provider         = UP_GitHub;
    g_AddonDef.UpdateLink       = "https://github.com/remsysc/gw2-quest-helper";
    return &g_AddonDef;
}
