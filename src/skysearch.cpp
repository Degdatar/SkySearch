#include "skysearch.h"

#include <RE/Skyrim.h>
#include <RE/U/UIUtils.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace SkyOS {
    namespace {
        std::string Escape(std::string_view value)
        {
            std::string out;
            for (const unsigned char c : value) {
                switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: if (c >= 0x20) out += static_cast<char>(c);
                }
            }
            return out;
        }

        std::string Lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool ContainsInsensitive(std::string_view value, std::string_view query)
        {
            if (query.empty()) return true;
            return std::search(value.begin(), value.end(), query.begin(), query.end(), [](unsigned char lhs, unsigned char rhs) {
                return std::tolower(lhs) == std::tolower(rhs);
            }) != value.end();
        }

        const char* Category(RE::FormType type)
        {
            using enum RE::FormType;
            switch (type) {
            case NPC: case ActorCharacter: return "NPC";
            case Quest: return "Quest";
            case Weapon: return "Weapon";
            case Armor: return "Armor";
            case Ammo: return "Ammo";
            case Book: return "Book";
            case KeyMaster: return "Key";
            case Misc: return "Misc";
            case AlchemyItem: case Ingredient: return "Ingredient";
            case Apparatus: return "Apparatus";
            case SoulGem: return "Soul Gem";
            case Scroll: return "Scroll";
            case Spell: return "Spell";
            case Shout: return "Shout";
            case Perk: return "Perk";
            case Cell: return "Cell";
            case Container: return "Container";
            case Furniture: return "Furniture";
            case Light: return "Light";
            case Static: return "Static";
            case Activator: return "Activator";
            case Flora: return "Flora";
            case Tree: return "Tree";
            default: return "Record";
            }
        }

        bool CanInventory(RE::TESForm* form)
        {
            if (!form || form->IsActor()) return false;
            switch (form->GetFormType()) {
            case RE::FormType::Weapon: case RE::FormType::Armor: case RE::FormType::Ammo:
            case RE::FormType::Book: case RE::FormType::KeyMaster: case RE::FormType::Misc:
            case RE::FormType::AlchemyItem: case RE::FormType::Ingredient: case RE::FormType::Apparatus:
            case RE::FormType::SoulGem: case RE::FormType::Scroll: return true;
            default: return false;
            }
        }

        bool Spawnable(RE::TESForm* form)
        {
            if (!form || CanInventory(form) || form->IsActor() || form->Is(RE::FormType::NPC) || form->Is(RE::FormType::ActorCharacter)) return false;
            return form->As<RE::TESBoundObject>() != nullptr;
        }

        std::string QuestDetails(RE::TESQuest* quest)
        {
            if (!quest) return "null";
            const bool completed = quest->IsCompleted();
            const bool running = quest->IsRunning() && !completed;
            std::string details = "{\"currentStage\":" + std::to_string(quest->GetCurrentStageID()) + ",\"running\":" + (running ? "true" : "false") + ",\"completed\":" + (completed ? "true" : "false") + ",\"objectives\":[";
            bool firstObjective = true;
            for (auto* objective : quest->objectives) {
                if (!objective) continue;
                if (!firstObjective) details += ',';
                firstObjective = false;
                details += "{\"index\":" + std::to_string(objective->index) + ",\"text\":\"" + Escape(objective->displayText.c_str() ? objective->displayText.c_str() : "") + "\"}";
            }
            details += "],\"stages\":[";
            bool firstStage = true;
            std::vector<std::uint16_t> seenStages;
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                for (auto* entry : player->GetQuestLog()) {
                    if (!entry || entry->owner != quest || !entry->owningStage) continue;
                    const auto stage = entry->owningStage->data.index;
                    if (std::find(seenStages.begin(), seenStages.end(), stage) != seenStages.end()) continue;
                    seenStages.push_back(stage);
                    if (!firstStage) details += ',';
                    firstStage = false;
                    const auto* logText = entry->GetLogEntry(quest);
                    const std::string stageText = logText ? std::string(logText) : std::string("Stage ") + std::to_string(stage);
                    details += "{\"id\":" + std::to_string(stage) + ",\"text\":\"" + Escape(stageText) + "\"}";
                }
            }
            return details + "]}";
        }

        void Append(std::string& out, bool& first, std::uint32_t id, std::string_view name, std::string_view category, std::string_view kind, bool inventory, bool spawn, bool actor = false, bool learn = false, std::string_view questDetails = "null")
        {
            if (!first) out += ',';
            first = false;
            char idText[9]{};
            std::snprintf(idText, sizeof(idText), "%08X", id);
            out += "{\"id\":\"" + std::string(idText) + "\",\"name\":\"" + Escape(name) + "\",\"category\":\"" + Escape(category) + "\",\"kind\":\"" + Escape(kind) + "\",\"inventory\":" + (inventory ? "true" : "false") + ",\"spawn\":" + (spawn ? "true" : "false") + ",\"actor\":" + (actor ? "true" : "false") + ",\"learn\":" + (learn ? "true" : "false") + ",\"questDetails\":" + std::string(questDetails) + "}";
        }

        bool CategorySelected(const std::string& categories, std::string_view category)
        {
            if (categories.empty() || categories == "all") return true;
            size_t start = 0;
            while (start <= categories.size()) {
                const auto end = categories.find(',', start);
                const auto token = std::string_view(categories).substr(start, end == std::string::npos ? categories.size() - start : end - start);
                if (token == category) return true;
                if (end == std::string::npos) break;
                start = end + 1;
            }
            return false;
        }

        bool MatchesCategory(RE::FormType type, const std::string& categories)
        {
            if (categories.empty() || categories == "all") return true;
            const auto name = Lower(Category(type));
            if (CategorySelected(categories, name)) return true;
            return (type == RE::FormType::AlchemyItem && CategorySelected(categories, "ingredient"));
        }

        void SearchForms(std::string& out, bool& first, const std::string& query, const std::string& category, const std::string& modName, size_t& count, size_t maxResults)
        {
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) return;
            constexpr std::array types{
                RE::FormType::NPC, RE::FormType::Quest, RE::FormType::Weapon,
                RE::FormType::Armor, RE::FormType::Ammo, RE::FormType::Book, RE::FormType::KeyMaster,
                RE::FormType::Misc, RE::FormType::AlchemyItem, RE::FormType::Ingredient, RE::FormType::Apparatus,
                RE::FormType::SoulGem, RE::FormType::Scroll, RE::FormType::Spell, RE::FormType::Shout, RE::FormType::Perk,
                RE::FormType::Container, RE::FormType::Furniture, RE::FormType::Light,
                RE::FormType::Static, RE::FormType::Activator, RE::FormType::Flora, RE::FormType::Tree
            };
            std::uint32_t numeric{};
            const auto parse = std::from_chars(query.data(), query.data() + query.size(), numeric, 16);
            const bool isFormID = parse.ec == std::errc{} && parse.ptr == query.data() + query.size() && query.size() >= 6 && query.size() <= 8;
            const bool modRecords = category.rfind("modrecords:", 0) == 0;
            const auto recordCategories = modRecords ? category.substr(11) : category;
            const bool searchNpcs = MatchesCategory(RE::FormType::NPC, modRecords ? recordCategories : category);
            std::unordered_set<RE::FormID> liveNpcBaseIDs;
            if (searchNpcs) {
                if (auto* processes = RE::ProcessLists::GetSingleton()) {
                    processes->ForAllActors([&](RE::Actor* actor) {
                        if (!actor || actor->IsDeleted() || actor->IsDead() || actor->IsDisabled()) {
                            return RE::BSContainer::ForEachResult::kContinue;
                        }
                        if (auto* base = actor->GetActorBase()) liveNpcBaseIDs.insert(base->GetFormID());
                        return RE::BSContainer::ForEachResult::kContinue;
                    });
                }
            }
            for (const auto type : types) {
                if (count >= maxResults) break;
                if (!modRecords && !MatchesCategory(type, category)) continue;
                if (modRecords && !MatchesCategory(type, recordCategories)) continue;
                auto& forms = data->GetFormArray(type);
                for (auto* form : forms) {
                    if (count >= maxResults) break;
                    if (!form || form->IsDeleted() || !form->GetFormID()) continue;
                    if (type == RE::FormType::NPC && !liveNpcBaseIDs.contains(form->GetFormID())) continue;
                    const auto* rawName = form->GetName();
                    const std::string name = rawName ? rawName : "(unnamed record)";
                    const auto id = form->GetFormID();
                    const auto* file = form->GetFile();
                    if (modRecords && (!file || Lower(std::string(file->GetFilename())) != Lower(modName))) continue;
                    const bool match = isFormID ? id == numeric : ContainsInsensitive(name, query) || (file && ContainsInsensitive(file->GetFilename(), query));
                    if (!match) continue;
                    const bool actor = type == RE::FormType::NPC || form->As<RE::Actor>() != nullptr;
                    const bool learnable = type == RE::FormType::Spell || type == RE::FormType::Shout || type == RE::FormType::Perk;
                    Append(out, first, id, name, Category(type), "record", CanInventory(form), Spawnable(form), actor, learnable, type == RE::FormType::Quest ? QuestDetails(form->As<RE::TESQuest>()) : "null");
                    ++count;
                }
            }
            if (count < maxResults && (modRecords ? MatchesCategory(RE::FormType::Cell, recordCategories) : MatchesCategory(RE::FormType::Cell, category))) {
                std::vector<RE::TESObjectCELL*> cells;
                std::unordered_set<std::uint32_t> seenCellIds;
                for (auto* form : data->GetFormArray(RE::FormType::Cell)) {
                    auto* cell = form ? form->As<RE::TESObjectCELL>() : nullptr;
                    if (cell && seenCellIds.insert(cell->GetFormID()).second) cells.push_back(cell);
                }
                for (auto* cell : data->interiorCells) {
                    if (cell && seenCellIds.insert(cell->GetFormID()).second) cells.push_back(cell);
                }
                for (auto* cell : cells) {
                    if (count >= maxResults) break;
                    if (!cell || cell->IsDeleted() || !cell->GetFormID()) continue;
                    const auto* file = cell->GetFile();
                    if (modRecords && (!file || Lower(std::string(file->GetFilename())) != Lower(modName))) continue;
                    const char* cellName = cell->GetName();
                    const char* editorId = cell->GetFormEditorID();
                    const std::string displayName = cellName && *cellName ? cellName : editorId && *editorId ? editorId : "Unnamed cell";
                    const bool match = isFormID ? cell->GetFormID() == numeric : ContainsInsensitive(displayName, query) || (editorId && ContainsInsensitive(editorId, query)) || (file && ContainsInsensitive(file->GetFilename(), query));
                    if (!match) continue;
                    Append(out, first, cell->GetFormID(), displayName, "Cell", "record", false, false);
                    ++count;
                }
            }
            const auto* mods = data->GetLoadedMods();
            const auto modCount = data->GetLoadedModCount();
            for (std::uint32_t i = 0; mods && i < modCount && count < maxResults && !modRecords && CategorySelected(category, "mod"); ++i) {
                if (!mods[i]) continue;
                const auto fileName = mods[i]->GetFilename();
                if (!ContainsInsensitive(fileName, query)) continue;
                Append(out, first, 0, fileName, "Mod", "mod", false, false);
                ++count;
            }
        }

    }

    std::string SearchSkyrim(const std::string& rawQuery, const std::string& rawCategory, const std::string& modName, std::size_t maxResults)
    {
        auto query = Lower(rawQuery);
        const auto category = Lower(rawCategory);
        std::string out = "[";
        bool first = true;
        size_t count = 0;
        maxResults = std::clamp<std::size_t>(maxResults, 1, 5000);
        SearchForms(out, first, query, category, modName, count, maxResults);
        return out + "]";
    }

    bool ExecuteSkySearchAction(const std::string& action, std::uint32_t formId, std::int32_t count, std::string& error, std::string& feedback)
    {
        if (action != "add" && action != "spawn" && action != "learn" && action != "teleport-player" && action != "teleport-actor" && action != "teleport-cell" && action != "quest-start" && action != "quest-reset" && action != "quest-complete" && action != "quest-stage") { error = "Unknown SkySearch action."; return false; }
        if ((action == "add" || action == "spawn") && (count < 1 || count > (action == "add" ? 2140000000 : 100))) { error = "Requested count is outside the allowed range."; return false; }
        if (action == "quest-stage" && (count < 0 || count > 65535)) { error = "Quest stage must be between 0 and 65535."; return false; }
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!data || !player) { error = "Skyrim data or player is unavailable."; return false; }
        RE::TESForm* form = RE::TESForm::LookupByID(formId);
        constexpr std::array actionTypes{RE::FormType::NPC, RE::FormType::ActorCharacter, RE::FormType::Cell, RE::FormType::Weapon, RE::FormType::Armor, RE::FormType::Ammo, RE::FormType::Book, RE::FormType::KeyMaster, RE::FormType::Misc, RE::FormType::AlchemyItem, RE::FormType::Ingredient, RE::FormType::Apparatus, RE::FormType::SoulGem, RE::FormType::Scroll, RE::FormType::Spell, RE::FormType::Container, RE::FormType::Furniture, RE::FormType::Static, RE::FormType::Activator, RE::FormType::Flora, RE::FormType::Tree, RE::FormType::Light};
        for (const auto type : actionTypes) {
            if (form) break;
            for (auto* candidate : data->GetFormArray(type)) if (candidate && candidate->GetFormID() == formId) { form = candidate; break; }
            if (form) break;
        }
        if (!form) { error = "The selected FormID is not loaded."; return false; }
        if (action == "teleport-cell") {
            auto* cell = form->As<RE::TESObjectCELL>();
            if (!cell) { error = "The selected record is not a cell."; return false; }
            if (!player->CenterOnCell(cell)) { error = "Skyrim could not teleport to this cell. Try a named interior cell."; return false; }
            feedback = "Teleporting to " + std::string(cell->GetName() ? cell->GetName() : "cell");
            return true;
        }
        if (action == "learn") {
            if (auto* spell = form->As<RE::SpellItem>()) { player->AddSpell(spell); feedback = std::string(spell->GetName() ? spell->GetName() : "Spell") + " Learned"; }
            else if (auto* shout = form->As<RE::TESShout>()) { player->AddShout(shout); feedback = std::string(shout->GetName() ? shout->GetName() : "Shout") + " Learned"; }
            else if (auto* perk = form->As<RE::BGSPerk>()) player->AddPerk(perk, 1);
            else { error = "This record is not a spell, shout, or perk."; return false; }
            if (feedback.empty()) feedback = std::string(form->GetName() ? form->GetName() : "Perk") + " Learned";
            return true;
        }
        if (action == "quest-start" || action == "quest-reset" || action == "quest-complete" || action == "quest-stage") {
            auto* quest = form->As<RE::TESQuest>();
            if (!quest) { error = "The selected record is not a quest."; return false; }
            auto* skyrimVM = RE::SkyrimVM::GetSingleton();
            if (!skyrimVM) { error = "Skyrim's Papyrus VM is unavailable."; return false; }
            auto& runtime = skyrimVM->GetVMRuntimeData();
            if (!runtime.impl) { error = "Skyrim's Papyrus VM is unavailable."; return false; }
            RE::VMTypeID questType{};
            if (!runtime.impl->GetTypeIDForScriptObject(RE::BSFixedString("Quest"), questType)) { error = "Skyrim's Quest script type is unavailable."; return false; }
            const auto handle = runtime.handlePolicy.GetHandleForObject(questType, quest);
            if (handle == runtime.handlePolicy.EmptyHandle()) { error = "Could not resolve the quest's Papyrus handle."; return false; }
            const std::string questName = quest->GetName() ? quest->GetName() : "Quest";
            if (action == "quest-start") {
                if (!quest->Start()) { error = "Skyrim could not start this quest."; return false; }
                feedback = "Started quest: " + questName;
                if (const auto sound = RE::TESForm::LookupByEditorID<RE::BGSSoundDescriptorForm>("UIQuestAdded")) RE::UIUtils::PlayMenuSound(sound);
            } else if (action == "quest-reset") {
                quest->ResetAndUpdate();
                feedback = "Reset quest: " + questName;
            } else if (action == "quest-stage") {
                auto* args = RE::MakeFunctionArguments(static_cast<std::uint32_t>(count));
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
                if (!runtime.impl->DispatchMethodCall2(handle, RE::BSFixedString("Quest"), RE::BSFixedString("SetStage"), args, result)) {
                    error = "Papyrus rejected the quest stage request.";
                    return false;
                }
                feedback = "Advanced " + questName + " to stage " + std::to_string(count);
            } else {
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
                if (!runtime.impl->DispatchMethodCall2(handle, RE::BSFixedString("Quest"), RE::BSFixedString("CompleteQuest"), RE::MakeFunctionArguments(), result)) {
                    error = "Papyrus rejected the quest completion request.";
                    return false;
                }
                feedback = "Completed quest: " + questName;
                if (const auto sound = RE::TESForm::LookupByEditorID<RE::BGSSoundDescriptorForm>("UIQuestCompleted")) RE::UIUtils::PlayMenuSound(sound);
            }
            return true;
        }
        if (action == "add") {
            auto* bound = form->As<RE::TESBoundObject>();
            if (!CanInventory(form) || !bound) { error = "This record cannot be added to inventory."; return false; }
            player->AddObjectToContainer(bound, nullptr, count, nullptr);
            feedback = std::string(form->GetName() ? form->GetName() : "Item") + " added";
            return true;
        }
        if (action == "spawn") {
            auto* bound = form->As<RE::TESBoundObject>();
            if (!Spawnable(form) || !bound) { error = "This record cannot be spawned as a world object."; return false; }
            for (int i = 0; i < count; ++i) player->PlaceObjectAtMe(bound, false);
            feedback = std::string(form->GetName() ? form->GetName() : "Item") + " spawned";
            return true;
        }
        auto* actor = form->As<RE::Actor>();
        if (!actor && form->Is(RE::FormType::NPC)) {
            if (auto* processes = RE::ProcessLists::GetSingleton()) {
                processes->ForAllActors([&](RE::Actor* candidate) {
                    if (candidate && !candidate->IsDeleted() && !candidate->IsDead() && candidate->GetActorBase() == form->As<RE::TESNPC>()) {
                        actor = candidate;
                        return RE::BSContainer::ForEachResult::kStop;
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                });
            }
        }
        if (action == "teleport-actor") {
            if (!actor || actor->IsDead()) { error = "The selected record is not a living actor reference."; return false; }
            actor->MoveTo(player);
            feedback = std::string(actor->GetName() ? actor->GetName() : "NPC") + " teleported to you";
            return true;
        }
        if (!actor || actor->IsDead()) { error = "Select a live NPC reference from search results to teleport to it."; return false; }
        player->MoveTo(actor);
        feedback = "Teleported to " + std::string(actor->GetName() ? actor->GetName() : "NPC");
        return true;
    }
}
