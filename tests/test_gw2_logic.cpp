#include "gw2_logic.h"
#include "tracker_service.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

using namespace gw2qh;

namespace {

class MockHttpClient final : public IHttpClient {
public:
    std::atomic<int> syncRound{0};
    std::atomic<int> map99999Calls{0};

    HttpResponse GetHttps(const std::string& host,
                          const std::string& pathAndQuery,
                          const std::string& bearerToken,
                          const std::atomic<bool>* /*cancelFlag*/) override {
        HttpResponse r{};
        r.statusCode = 200;

        if (host == "api.guildwars2.com") {
            if (pathAndQuery == "/v2/tokeninfo") {
                if (bearerToken != "VALID-TOKEN-1234") {
                    r.statusCode = 401;
                    r.body = R"({"text":"Invalid access token"})";
                    return r;
                }
                r.body = R"({"id":"key","name":"test","permissions":["account","characters","progression","unlocks"]})";
                return r;
            }
            if (pathAndQuery == "/v2/account") {
                r.body = R"({"id":"guid","name":"Shinhra.9632"})";
                return r;
            }
            if (pathAndQuery == "/v2/characters") {
                r.body = R"(["Lunacrina"])";
                return r;
            }
            if (pathAndQuery == "/v2/account/achievements") {
                int round = syncRound.load();
                if (round == 0) {
                    // Scales (4690) done, Eggs (4666) 1/2, Parent Newborn Skyscales (4714) 6/8,
                    // CategoryDisplay Return to Domain of Istan (5948) 8/15 with sub-achievements 5862 (done) & 5865 (2/10)
                    r.body = R"([
                        {"id": 4690, "current": 21, "max": 21, "done": true},
                        {"id": 4666, "bits": [0], "current": 1, "max": 2, "done": false},
                        {"id": 4714, "bits": [0, 1, 2, 3, 4, 5], "current": 6, "max": 8, "done": false},
                        {"id": 5948, "current": 8, "max": 15, "done": false},
                        {"id": 5862, "current": 1, "max": 1, "done": true},
                        {"id": 5865, "current": 2, "max": 10, "done": false}
                    ])";
                } else {
                    // Eggs (4666) completes!
                    r.body = R"([
                        {"id": 4690, "current": 21, "max": 21, "done": true},
                        {"id": 4666, "bits": [0, 1], "current": 2, "max": 2, "done": true},
                        {"id": 4714, "bits": [0, 1, 2, 3, 4, 5, 6], "current": 7, "max": 8, "done": false},
                        {"id": 5948, "current": 8, "max": 15, "done": false},
                        {"id": 5862, "current": 1, "max": 1, "done": true},
                        {"id": 5865, "current": 2, "max": 10, "done": false}
                    ])";
                }
                return r;
            }
            if (pathAndQuery == "/v2/account/skins" || pathAndQuery == "/v2/account/minis") {
                r.body = R"([])";
                return r;
            }
            if (pathAndQuery == "/v2/characters/Lunacrina/quests") {
                r.body = R"([15])";
                return r;
            }
            if (pathAndQuery.find("/v2/quests?ids=all") == 0) {
                r.body = R"([
                    {"id": 15, "name": "Explosive Intellect", "level": 1, "story": 8, "goals": [{"active": "Stop the golems.", "complete": "Done."}]},
                    {"id": 16, "name": "Golem Positioning System", "level": 4, "story": 8, "goals": [{"active": "Find the golem.", "complete": "Done."}]}
                ])";
                return r;
            }
            if (pathAndQuery.find("/v2/stories?ids=all") == 0) {
                r.body = R"([{"id": 8, "name": "My Story"}])";
                return r;
            }
            if (pathAndQuery == "/v2/achievements/categories") {
                r.body = R"([310])";
                return r;
            }
            if (pathAndQuery.find("/v2/achievements/categories?ids=") == 0) {
                r.body = R"([{
                    "id": 310,
                    "name": "Return to Daybreak",
                    "icon": "https://render.guildwars2.com/file/CAT/310.png",
                    "achievements": [5862, 5865, 5948]
                }])";
                return r;
            }
            if (pathAndQuery.find("/v2/achievements?ids=") == 0) {
                struct AchMockItem { const char* idStr; const char* json; };
                static const AchMockItem kMocks[] = {
                    {"4666", R"({"id":4666,"name":"Skyscale Eggs","requirement":"Recover all the skyscale eggs.","type":"ItemSet","tiers":[{"count":2,"points":1}],"bits":[{"type":"Item","id":90760,"text":"Somewhere in Sky's Reach."},{"type":"Item","id":90383,"text":"Somewhere in Melandru's Chalice."}],"rewards":[{"type":"Item","id":90869,"count":1}]})"},
                    {"4690", R"({"id":4690,"name":"Skyscale Scales","requirement":"Collect scales.","type":"ItemSet","tiers":[{"count":21,"points":1}]})"},
                    {"4714", R"({"id":4714,"name":"Newborn Skyscales","icon":"https://render.guildwars2.com/file/ACH/4714.png","requirement":"Something is wrong with the skyscales.","type":"ItemSet","tiers":[{"count":8,"points":5}],"bits":[{"type":"Item","id":90860,"text":"Complete the Skyscale Scales collection."},{"type":"Item","id":90869,"text":"Complete the Skyscale Eggs collection."},{"type":"Item","id":90380,"text":"Return to Gorrik in Dragonfall after collecting all the skyscale eggs."}]})"},
                    {"4712", R"({"id":4712,"name":"Saving Skyscales","requirement":"Hatch a skyscale egg.","type":"ItemSet","tiers":[{"count":14,"points":5}]})"},
                    {"5948", R"({"id":5948,"name":"Return to the Domain of Istan","requirement":"Complete Return to Daybreak achievements.","type":"Default","flags":["Pvp","RepairOnLogin","CategoryDisplay","Permanent"],"tiers":[{"count":15,"points":15}]})"},
                    {"5862", R"({"id":5862,"name":"Treasure Hunter: Domain of Istan","requirement":"Loot 10 chests in Domain of Istan.","type":"Default","tiers":[{"count":1,"points":1}]})"},
                    {"5865", R"({"id":5865,"name":"Ore Miner: Domain of Istan","requirement":"Mine 10 nodes in Domain of Istan.","type":"Default","tiers":[{"count":10,"points":1}]})"}
                };
                std::string arr = "[";
                bool first = true;
                for (const auto& item : kMocks) {
                    if (pathAndQuery.find(item.idStr) != std::string::npos) {
                        if (!first) arr += ",";
                        arr += item.json;
                        first = false;
                    }
                }
                arr += "]";
                r.body = arr;
                return r;
            }
            if (pathAndQuery.find("/v2/items?") == 0) {
                r.body = R"([
                    {"id": 90760, "name": "Skyscale Egg #1"},
                    {"id": 90383, "name": "Skyscale Egg #17"},
                    {"id": 90869, "name": "Skyscale Eggs", "icon": "https://render.guildwars2.com/file/EGG/90869.png"}
                ])";
                return r;
            }
            if (pathAndQuery.find("/v2/maps/1317") == 0) {
                r.body = R"({"id": 1317, "name": "Dragonfall"})";
                return r;
            }
            if (pathAndQuery.find("/v2/maps/99999") == 0) {
                ++map99999Calls;
                r.statusCode = 404;
                r.body = R"({"text":"no such id"})";
                return r;
            }
        } else if (host == "wiki.guildwars2.com") {
            if (pathAndQuery.find("action=parse&page=Template%3ASkyscale_unlock_nav") != std::string::npos) {
                r.body = R"({
                    "parse": {
                        "title": "Template:Skyscale unlock nav",
                        "wikitext": {
                            "*": "<div class=\"heading\">[[Skyscale]] unlock achievements</div>\n{|\n! [[Newborn Skyscales]]\n| [[Skyscale Scales|Scales]] • [[Skyscale Eggs|Eggs]]\n|-\n! [[Saving Skyscales]]\n| [[Skyscale of Water|Water]]\n|}"
                        }
                    }
                })";
                return r;
            }
            if (pathAndQuery.find("action=parse&page=Skyscale_Eggs") != std::string::npos) {
                r.body = R"({
                    "parse": {
                        "title": "Skyscale Eggs",
                        "wikitext": {
                            "*": "[[Skyscale Eggs]] is an achievement in [[Dragonfall]].\n== Achievement ==\n{{collection table row|Skyscale Egg 1 | col1 = [[Sky's Reach]] | notes = Near Gorrik.}}\n{{collection table row|Skyscale Egg 17 | col1 = [[Melandru's Chalice]] | notes = Northeast of [[Melandru's Lost Domain Waypoint]] on a branch.}}\n{{Skyscale unlock nav}}"
                        }
                    }
                })";
                return r;
            }
            if (pathAndQuery.find("90869") != std::string::npos && pathAndQuery.find("Is%20part%20of%20collection") != std::string::npos) {
                r.body = R"json({
                    "query": {
                        "results": {
                            "Skyscale Eggs (item)": {
                                "printouts": {
                                    "Is part of collection": [{"fulltext": "Newborn Skyscales"}]
                                }
                            }
                        }
                    }
                })json";
                return r;
            }
            if (pathAndQuery.find("Melandru") != std::string::npos) {
                r.body = R"({
                    "query": {
                        "results": {
                            "Mist Warden Camp#Melandru's Lost Domain Waypoint": {
                                "printouts": { "Has game id": [3026] }
                            }
                        }
                    }
                })";
                return r;
            }
            if (pathAndQuery.find("Has%20context%3A%3AAchievement") != std::string::npos) {
                r.body = R"({
                    "query": {
                        "results": {
                            "War Eternal#ach4714": {"printouts": {"Has game id": [4714], "Has canonical name": ["Newborn Skyscales"]}},
                            "War Eternal#ach4690": {"printouts": {"Has game id": [4690], "Has canonical name": ["Skyscale Scales"]}},
                            "War Eternal#ach4666": {"printouts": {"Has game id": [4666], "Has canonical name": ["Skyscale Eggs"]}},
                            "War Eternal#ach4712": {"printouts": {"Has game id": [4712], "Has canonical name": ["Saving Skyscales"]}}
                        }
                    }
                })";
                return r;
            }
            r.body = R"({"query":{"results":[]}})";
            return r;
        }

        r.statusCode = 404;
        r.body = R"({"text":"not found"})";
        return r;
    }
};

void TestQuestlineParsers() {
    assert(ExtractCollectionTargetFromBitText("Complete the Skyscale Eggs collection.") == "Skyscale Eggs");
    assert(ExtractCollectionTargetFromBitText("Complete Skyscale of Water.") == "Skyscale of Water");

    std::string navJson = R"({
        "parse": {
            "wikitext": {
                "*": "<div class=\"heading\">[[Skyscale]] unlock achievements</div>\n{|\n! style=\"width:12em\" | [[Newborn Skyscales]]\n| [[Skyscale Scales|Scales]] • [[Skyscale Eggs|Eggs]]\n|-\n! [[Saving Skyscales]]\n| [[Skyscale of Water|Water]]\n|}"
            }
        }
    })";
    auto rm = ParseWikiNavboxRoadmapJson(navJson, "Skyscale unlock nav");
    assert(rm.loaded);
    assert(rm.questlineTitle == "Skyscale unlock achievements");
    assert(rm.stages.size() == 2);
    assert(rm.stages[0].stageTitle == "Newborn Skyscales");
    assert(rm.stages[0].childTitles.size() == 2);
    assert(rm.stages[1].stageTitle == "Saving Skyscales");

    // Verify legacy Item (kind=1) and Currency (kind=3) entries are dropped cleanly on load
    std::string legacyJson = R"({
        "tracked": [
            {"kind": 0, "id": 4666, "label_hint": "Skyscale Eggs"},
            {"kind": 1, "id": 91696, "target_count": 5, "label_hint": "Legacy Item"},
            {"kind": 2, "id": 15, "label_hint": "Explosive Intellect"},
            {"kind": 3, "id": 50, "target_count": 250, "label_hint": "Legacy Currency"}
        ]
    })";
    AddonSettings migrated = DeserializeSettings(legacyJson);
    assert(migrated.tracked.size() == 2);
    assert(migrated.tracked[0].kind == EntryKind::Achievement && migrated.tracked[0].id == 4666);
    assert(migrated.tracked[1].kind == EntryKind::Quest && migrated.tracked[1].id == 15);

    // Verify Wiki SMW multi-word wildcard generation ("return to domain of istan" -> matches "Return to the Domain of Istan")
    std::string smw = BuildWikiSmwCondition("return to domain of istan");
    assert(smw.find("~*Return*Domain*Istan*") != std::string::npos);

    // Verify whole-phrase map matching prevents "Chaos" from matching "chaotic"
    assert(!ContainsWholePhraseCaseInsensitive("Clear a chaotic rift in Tyria", "Chaos"));
    assert(ContainsWholePhraseCaseInsensitive("Return to the Domain of Istan", "Domain of Istan"));

    // Verify HTTP cache header parsing
    assert(ParseHttpCacheRemainingSec("public, max-age=300", "275") == 25);
    assert(ParseHttpCacheRemainingSec("MAX-AGE=120", "150") == 0);
    assert(ParseHttpCacheRemainingSec("no-cache", "") == -1);
    assert(ParseHttpCacheRemainingSec("", "") == -1);
}

void TestEndToEndQuestlineAndAutoAdvance() {
    auto tempDir = std::filesystem::temp_directory_path() / "gw2qh_v12_test";
    std::filesystem::remove_all(tempDir);

    auto mockPtr = std::make_unique<MockHttpClient>();
    MockHttpClient* rawMock = mockPtr.get();

    std::vector<std::string> alerts;
    TrackerService svc(
        tempDir.string(),
        std::move(mockPtr),
        nullptr,
        [&alerts](const std::string& msg) { alerts.push_back(msg); });
    svc.Start();

    svc.PinEntry(EntryKind::Achievement, 4666, "Skyscale Eggs");
    svc.PinEntry(EntryKind::Quest, 15, "Explosive Intellect");
    svc.UpdateLiveContext("Lunacrina", 1317);
    svc.SetApiKey("VALID-TOKEN-1234");

    UiSnapshot snap{};
    for (int i = 0; i < 60; ++i) {
        snap = svc.GetUiSnapshot();
        if (snap.hasSyncedOnce && !snap.syncInProgress &&
            !snap.evaluatedEntries.empty() &&
            snap.evaluatedEntries[0].chain.hasParent &&
            snap.evaluatedEntries[0].chain.hasNext) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 1. Verify Stage-Only Roadmap & Next ("Saving Skyscales", NOT 1-by-1 sub-items!) + Reward Icon Fallback
    const auto& eggs = snap.evaluatedEntries[0];
    assert(eggs.entry.id == 4666);
    assert(eggs.iconUrl == "https://render.guildwars2.com/file/EGG/90869.png");
    assert(eggs.chain.hasParent);
    assert(eggs.chain.parentId == 4714);
    assert(eggs.chain.parentTitle == "Newborn Skyscales");
    assert(eggs.chain.parentCurrent == 6 && eggs.chain.parentMax == 8);
    assert(eggs.chain.stageIndex == 1 && eggs.chain.stageCount == 2);
    assert(eggs.chain.roadmapNodes.size() == 2); // Only the 2 Main Stages ("Newborn Skyscales", "Saving Skyscales")!
    assert(eggs.chain.hasNext);
    assert(eggs.chain.nextId == 4712);
    assert(eggs.chain.nextTitle == "Saving Skyscales");
    assert(eggs.chain.transitionHint == "Return to Gorrik in Dragonfall after collecting all the skyscale eggs.");

    // 2. Verify Personal Storyline auto-locks onto active uncompleted step (#16 "Golem Positioning System") with canJump == false!
    const auto& storyQ = snap.evaluatedEntries[1];
    assert(storyQ.title == "My Story");
    assert(storyQ.current == 1 && storyQ.target == 2 && !storyQ.completed);
    assert(storyQ.subtitle.find("Golem Positioning System") != std::string::npos);
    assert(!storyQ.chain.hasNext); // Storyline steps advance automatically; no manual jumping
    assert(storyQ.chain.roadmapNodes.size() == 2);
    assert(!storyQ.chain.roadmapNodes[0].canJump && !storyQ.chain.roadmapNodes[1].canJump);
    assert(storyQ.chain.roadmapNodes[1].isCurrent);

    // 3. Verify In-Progress achievements fetch & iconUrl population
    svc.RequestLoadInProgressAchievements();
    for (int i = 0; i < 60; ++i) {
        snap = svc.GetUiSnapshot();
        if (!snap.inProgressLoading && !snap.inProgressAchievements.empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    bool found4714Icon = false;
    for (const auto& ip : snap.inProgressAchievements) {
        if (ip.id == 4714 && ip.iconUrl == "https://render.guildwars2.com/file/ACH/4714.png") {
            found4714Icon = true;
            break;
        }
    }
    assert(found4714Icon);

    // 4. Enable Auto-Advance Questline and simulate completing Skyscale Eggs!
    AddonSettings upd = snap.settings;
    upd.autoAdvanceQuestline = true;
    svc.UpdateDisplaySettings(upd);

    rawMock->syncRound.store(1);
    svc.SetApiKey("VALID-TOKEN-1234");

    for (int i = 0; i < 60; ++i) {
        snap = svc.GetUiSnapshot();
        if (!alerts.empty() && snap.settings.tracked[0].id == 4712) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Skyscale Eggs (#4666) automatically advanced in-place to Saving Skyscales (#4712)!
    assert(snap.settings.tracked[0].id == 4712);
    assert(!alerts.empty());
    assert(alerts[0].find("Skyscale Eggs DONE! Up next: Saving Skyscales") != std::string::npos);

    svc.Stop();
    std::filesystem::remove_all(tempDir);
}

void TestCategoryDisplayAndMapFilterCache() {
    auto tempDir = std::filesystem::temp_directory_path() / "gw2qh_v151_test";
    std::filesystem::remove_all(tempDir);

    auto mockPtr = std::make_unique<MockHttpClient>();
    MockHttpClient* rawMock = mockPtr.get();

    TrackerService svc(tempDir.string(), std::move(mockPtr), nullptr, nullptr);
    svc.Start();

    svc.PinEntry(EntryKind::Achievement, 5948, "Return to the Domain of Istan");
    svc.UpdateLiveContext("Lunacrina", 1317);
    svc.SetApiKey("VALID-TOKEN-1234");

    UiSnapshot snap{};
    for (int i = 0; i < 60; ++i) {
        snap = svc.GetUiSnapshot();
        if (snap.hasSyncedOnce && !snap.evaluatedEntries.empty() &&
            snap.evaluatedEntries[0].bits.size() == 2 &&
            snap.evaluatedEntries[0].bits[0].label == "Treasure Hunter: Domain of Istan") {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    const auto& istan = snap.evaluatedEntries[0];
    assert(istan.entry.id == 5948);
    assert(istan.current == 8 && istan.target == 15);
    assert(istan.iconUrl == "https://render.guildwars2.com/file/CAT/310.png");
    assert(istan.bits.size() == 2);
    assert(istan.bits[0].childAchievementId == 5862);
    assert(istan.bits[0].completed == true);
    assert(istan.bits[0].label == "Treasure Hunter: Domain of Istan");
    assert(istan.bits[1].childAchievementId == 5865);
    assert(istan.bits[1].completed == false);
    assert(istan.bits[1].label == "Ore Miner: Domain of Istan (2/10)");

    // Verify 404 map ID (undocumented instance/Fractal) resolves to "Instance #99999", caches, and isn't re-fetched
    svc.UpdateLiveContext("Lunacrina", 99999);
    for (int i = 0; i < 60; ++i) {
        snap = svc.GetUiSnapshot();
        if (snap.activeMapName == "Instance #99999") break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    assert(snap.activeMapName == "Instance #99999");
    assert(!snap.evaluatedEntries.empty() && !snap.evaluatedEntries[0].matchesCurrentMap);
    assert(rawMock->map99999Calls.load() == 1);
    svc.TriggerSyncNow();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(rawMock->map99999Calls.load() == 1);
    assert(std::filesystem::exists(tempDir / "map_names_cache.json"));
    assert(std::filesystem::exists(tempDir / "ach_categories_cache.json"));

    svc.Stop();
    std::filesystem::remove_all(tempDir);
}

void TestOptimisticLocalCheckOffAndReconciliation() {
    auto tempDir = std::filesystem::temp_directory_path() / "gw2qh_v160_test";
    std::filesystem::remove_all(tempDir);

    auto mockPtr = std::make_unique<MockHttpClient>();
    MockHttpClient* rawMock = mockPtr.get();

    std::vector<std::string> alerts;
    TrackerService svc(
        tempDir.string(),
        std::move(mockPtr),
        nullptr,
        [&alerts](const std::string& msg) { alerts.push_back(msg); });
    svc.Start();

    svc.PinEntry(EntryKind::Achievement, 4666, "Skyscale Eggs");
    svc.UpdateLiveContext("Lunacrina", 1317);
    svc.SetApiKey("VALID-TOKEN-1234");

    UiSnapshot snap{};
    for (int i = 0; i < 60; ++i) {
        snap = svc.GetUiSnapshot();
        if (snap.hasSyncedOnce && !snap.syncInProgress &&
            !snap.evaluatedEntries.empty() &&
            snap.evaluatedEntries[0].bits.size() == 2) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    assert(snap.evaluatedEntries[0].current == 1 && snap.evaluatedEntries[0].target == 2);
    assert(!snap.evaluatedEntries[0].completed && !snap.evaluatedEntries[0].apiCompleted);

    // 1. Toggle incomplete bit #1 locally -> instant optimistic progress bump without API wait!
    svc.ToggleLocalBit(0, 1);
    snap = svc.GetUiSnapshot();
    assert(snap.evaluatedEntries[0].current == 2);
    assert(snap.evaluatedEntries[0].completed);
    assert(!snap.evaluatedEntries[0].apiCompleted);
    assert(snap.evaluatedEntries[0].localCheckedCount == 1);
    assert(snap.evaluatedEntries[0].bits[1].localCompleted);
    assert(snap.evaluatedEntries[0].bits[1].completed);

    // 2. Verify local_bits serialization round-trip
    AddonSettings reloaded = DeserializeSettings(SerializeSettings(snap.settings));
    assert(reloaded.tracked.size() == 1);
    assert(reloaded.tracked[0].localCheckedBits == std::vector<int>({1}));

    // 3. UndoLastLocalBit reverts optimistic check-off immediately
    svc.UndoLastLocalBit(0);
    snap = svc.GetUiSnapshot();
    assert(snap.evaluatedEntries[0].current == 1);
    assert(!snap.evaluatedEntries[0].completed);
    assert(snap.evaluatedEntries[0].localCheckedCount == 0);

    // 4. Re-check bit #1 locally, then simulate API sync confirming completion
    svc.ToggleLocalBit(0, 1);
    rawMock->syncRound.store(1);
    svc.SetApiKey("VALID-TOKEN-1234");
    for (int i = 0; i < 60; ++i) {
        snap = svc.GetUiSnapshot();
        if (!snap.syncInProgress && snap.evaluatedEntries[0].apiCompleted) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Reconciled cleanly: localCheckedBits cleared, current == 2 (no double count), completion alert still fires!
    assert(snap.evaluatedEntries[0].apiCompleted);
    assert(snap.evaluatedEntries[0].current == 2);
    assert(snap.evaluatedEntries[0].localCheckedCount == 0);
    assert(snap.settings.tracked[0].localCheckedBits.empty());
    assert(!alerts.empty());

    svc.Stop();
    std::filesystem::remove_all(tempDir);
}

} // namespace

int main() {
    TestQuestlineParsers();
    TestEndToEndQuestlineAndAutoAdvance();
    TestCategoryDisplayAndMapFilterCache();
    TestOptimisticLocalCheckOffAndReconciliation();
    std::cout << "All GW2 Quest Helper v1.6.0 tests passed!\n";
    return 0;
}
