#pragma once

#include "CoHModSDK.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ConfigUi {
    struct ChoiceEntry {
        std::int32_t value = 0;
        std::string valueId;
        std::string label;
    };

    struct OptionEntry {
        std::string optionId;
        std::string category;
        std::string label;
        std::string description;
        CoHModSDKConfigType type = CoHModSDKConfigType_Bool;
        CoHModSDKConfigValueV1 defaultValue = {};
        CoHModSDKConfigValueV1 currentValue = {};
        float minValue = 0.0f;
        float maxValue = 0.0f;
        float step = 0.0f;
        std::uint32_t flags = CoHModSDKConfigFlags_None;
        std::vector<ChoiceEntry> choices;
    };

    struct ModEntry {
        std::string modId;
        std::string displayName;
        std::vector<OptionEntry> options;
    };

    class Catalog {
    public:
        bool Refresh();
        void Clear();

        std::vector<ModEntry>& GetMods();
        const std::vector<ModEntry>& GetMods() const;
        std::size_t GetModCount() const;
        std::size_t GetOptionCount() const;

    private:
        std::vector<ModEntry> mods;
        std::size_t optionCount = 0u;
    };
}
