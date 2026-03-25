#include "ConfigCatalog.hpp"

#include <algorithm>
#include <utility>

namespace ConfigUi {
    namespace {
        bool CopyOption(const CoHModSDKConfigOptionV1* option, const CoHModSDKConfigValueV1* currentValue, void* userData) {
            if ((option == nullptr) || (currentValue == nullptr) || (userData == nullptr)) {
                return false;
            }

            auto* modEntry = static_cast<ModEntry*>(userData);

            OptionEntry optionEntry = {};
            optionEntry.optionId = option->optionId == nullptr ? std::string() : std::string(option->optionId);
            optionEntry.category = option->category == nullptr ? std::string() : std::string(option->category);
            optionEntry.label = option->label == nullptr ? optionEntry.optionId : std::string(option->label);
            optionEntry.description = option->description == nullptr ? std::string() : std::string(option->description);
            optionEntry.type = option->type;
            optionEntry.defaultValue = option->defaultValue;
            optionEntry.currentValue = *currentValue;
            optionEntry.minValue = option->minValue;
            optionEntry.maxValue = option->maxValue;
            optionEntry.step = option->step;
            optionEntry.flags = option->flags;

            if ((option->choices != nullptr) && (option->choiceCount > 0u)) {
                optionEntry.choices.reserve(option->choiceCount);
                for (std::uint32_t index = 0; index < option->choiceCount; ++index) {
                    const CoHModSDKConfigChoiceV1& sourceChoice = option->choices[index];

                    ChoiceEntry choiceEntry = {};
                    choiceEntry.value = sourceChoice.value;
                    choiceEntry.valueId = sourceChoice.valueId == nullptr ? std::string() : std::string(sourceChoice.valueId);
                    choiceEntry.label = sourceChoice.label == nullptr ? std::string() : std::string(sourceChoice.label);

                    optionEntry.choices.push_back(std::move(choiceEntry));
                }
            }

            modEntry->options.push_back(std::move(optionEntry));
            return true;
        }
    }

    bool Catalog::Refresh() {
        Clear();

        struct RefreshState {
            std::vector<std::string> modIds;
        };

        auto enumerateMod = [](const char* modId, void* userData) -> bool {
            if ((modId == nullptr) || (userData == nullptr)) {
                return false;
            }

            auto* state = static_cast<RefreshState*>(userData);
            state->modIds.emplace_back(modId);
            return true;
        };

        RefreshState state = {};
        if (!ModSDK::Config::EnumerateMods(enumerateMod, &state)) {
            Clear();
            return false;
        }

        mods.reserve(state.modIds.size());
        for (const std::string& modId : state.modIds) {
            ModEntry modEntry = {};
            modEntry.modId = modId;

            if (!ModSDK::Config::EnumerateOptions(modEntry.modId.c_str(), &CopyOption, &modEntry)) {
                Clear();
                return false;
            }

            std::sort(
                modEntry.options.begin(),
                modEntry.options.end(),
                [](const OptionEntry& left, const OptionEntry& right) {
                    return left.optionId < right.optionId;
                }
            );

            optionCount += modEntry.options.size();
            mods.push_back(std::move(modEntry));
        }

        std::sort(
            mods.begin(),
            mods.end(),
            [](const ModEntry& left, const ModEntry& right) {
                return left.modId < right.modId;
            }
        );

        return true;
    }

    void Catalog::Clear() {
        mods.clear();
        optionCount = 0u;
    }

    std::vector<ModEntry>& Catalog::GetMods() {
        return mods;
    }

    const std::vector<ModEntry>& Catalog::GetMods() const {
        return mods;
    }

    std::size_t Catalog::GetModCount() const {
        return mods.size();
    }

    std::size_t Catalog::GetOptionCount() const {
        return optionCount;
    }
}
