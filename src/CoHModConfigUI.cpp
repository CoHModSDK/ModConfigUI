#include "ConfigCatalog.hpp"
#include "UiInterop.hpp"

#include "CoHModSDK.hpp"

#include <string>

namespace {
    constexpr char kModId[] = "de.tosox.cohmodconfigui";
    constexpr char kModName[] = "CoH Mod Config UI";
    constexpr char kModVersion[] = "0.1.0";
    constexpr char kModAuthor[] = "Tosox";
    constexpr char kTestCategory[] = "UI Test";

    ConfigUi::Catalog gCatalog;

    void LogInfo(const std::string& message) {
        ModSDK::Runtime::Log(CoHModSDKLogLevel_Info, message.c_str());
    }

    std::string FormatConfigValue(const CoHModSDKConfigValueV1& value) {
        switch (value.type) {
        case CoHModSDKConfigType_Bool:
            return value.boolValue != 0u ? "true" : "false";
        case CoHModSDKConfigType_Int:
            return std::to_string(value.intValue);
        case CoHModSDKConfigType_Float:
            return std::to_string(value.floatValue);
        case CoHModSDKConfigType_Enum:
            return std::to_string(value.enumValue);
        default:
            return "<unknown>";
        }
    }

    void OnTestConfigChanged(const char* modId, const char* optionId, const CoHModSDKConfigValueV1* value, void*) {
        if ((modId == nullptr) || (optionId == nullptr) || (value == nullptr)) {
            return;
        }

        LogInfo(
            "CoH Mod Config UI test option updated: " +
            std::string(modId) +
            "." +
            optionId +
            "=" +
            FormatConfigValue(*value)
        );
    }

    bool RegisterTestSchema() {
        static const CoHModSDKConfigChoiceV1 kLayoutChoices[] = {
            { 0, "compact", "Compact" },
            { 1, "standard", "Standard" },
            { 2, "verbose", "Verbose" },
        };

        static const CoHModSDKConfigChoiceV1 kAccentChoices[] = {
            { 0, "classic", "Classic" },
            { 1, "allied", "Allied" },
            { 2, "axis", "Axis" },
            { 3, "industrial", "Industrial" },
            { 4, "field", "Field" },
            { 5, "signal", "Signal" },
        };

        static const CoHModSDKConfigOptionV1 kOptions[] = {
            {
                "showDebugHeader",
                kTestCategory,
                "Show Debug Header",
                "Adds a metadata-heavy header line to the test menu output.",
                CoHModSDKConfigType_Bool,
                ModSDK::Config::MakeBoolValue(true),
                0.0f,
                0.0f,
                0.0f,
                CoHModSDKConfigFlags_None,
                nullptr,
                0u,
                &OnTestConfigChanged,
                nullptr,
            },
            {
                "previewSelectionIndex",
                kTestCategory,
                "Preview Selection Index",
                "Integer test option for menu value editing and persistence.",
                CoHModSDKConfigType_Int,
                ModSDK::Config::MakeIntValue(2),
                0.0f,
                8.0f,
                1.0f,
                CoHModSDKConfigFlags_None,
                nullptr,
                0u,
                &OnTestConfigChanged,
                nullptr,
            },
            {
                "panelOpacity",
                kTestCategory,
                "Panel Opacity",
                "Float test option for menu value editing and persistence.",
                CoHModSDKConfigType_Float,
                ModSDK::Config::MakeFloatValue(0.85f),
                0.25f,
                1.0f,
                0.05f,
                CoHModSDKConfigFlags_None,
                nullptr,
                0u,
                &OnTestConfigChanged,
                nullptr,
            },
            {
                "layoutPreset",
                kTestCategory,
                "Layout Preset",
                "Enum test option for cycling through named UI presets.",
                CoHModSDKConfigType_Enum,
                ModSDK::Config::MakeEnumValue(1),
                0.0f,
                0.0f,
                0.0f,
                CoHModSDKConfigFlags_RestartRequired,
                kLayoutChoices,
                static_cast<std::uint32_t>(sizeof(kLayoutChoices) / sizeof(kLayoutChoices[0])),
                &OnTestConfigChanged,
                nullptr,
            },
            {
                "enableRowAnimations",
                kTestCategory,
                "Enable Row Animations",
                "Bool test option for validating additional checkbox rows and scroll behavior.",
                CoHModSDKConfigType_Bool,
                ModSDK::Config::MakeBoolValue(false),
                0.0f,
                0.0f,
                0.0f,
                CoHModSDKConfigFlags_None,
                nullptr,
                0u,
                &OnTestConfigChanged,
                nullptr,
            },
            {
                "controllerNavigation",
                kTestCategory,
                "Controller Navigation",
                "Bool test option for validating longer labels and another checkbox row.",
                CoHModSDKConfigType_Bool,
                ModSDK::Config::MakeBoolValue(false),
                0.0f,
                0.0f,
                0.0f,
                CoHModSDKConfigFlags_RestartRequired,
                nullptr,
                0u,
                &OnTestConfigChanged,
                nullptr,
            },
            {
                "panelScale",
                kTestCategory,
                "Panel Scale",
                "Float test option for slider interaction across an additional numeric row.",
                CoHModSDKConfigType_Float,
                ModSDK::Config::MakeFloatValue(1.0f),
                0.75f,
                1.25f,
                0.05f,
                CoHModSDKConfigFlags_None,
                nullptr,
                0u,
                &OnTestConfigChanged,
                nullptr,
            },
            {
                "menuBlurStrength",
                kTestCategory,
                "Menu Blur Strength",
                "Float test option for slider interaction and persisted value updates.",
                CoHModSDKConfigType_Float,
                ModSDK::Config::MakeFloatValue(0.40f),
                0.0f,
                1.0f,
                0.05f,
                CoHModSDKConfigFlags_None,
                nullptr,
                0u,
                &OnTestConfigChanged,
                nullptr,
            },
            {
                "previewThumbnailIndex",
                kTestCategory,
                "Preview Thumbnail Index",
                "Integer test option for driving deeper row scrolling in the panel.",
                CoHModSDKConfigType_Int,
                ModSDK::Config::MakeIntValue(3),
                0.0f,
                12.0f,
                1.0f,
                CoHModSDKConfigFlags_None,
                nullptr,
                0u,
                &OnTestConfigChanged,
                nullptr,
            },
            {
                "accentPreset",
                kTestCategory,
                "Accent Preset",
                "Enum test option with enough choices to keep exercising the dropdown scrollbar path.",
                CoHModSDKConfigType_Enum,
                ModSDK::Config::MakeEnumValue(0),
                0.0f,
                0.0f,
                0.0f,
                CoHModSDKConfigFlags_None,
                kAccentChoices,
                static_cast<std::uint32_t>(sizeof(kAccentChoices) / sizeof(kAccentChoices[0])),
                &OnTestConfigChanged,
                nullptr,
            },
        };

        static const CoHModSDKConfigSchemaV1 kSchema = {
            kModId,
            kOptions,
            static_cast<std::uint32_t>(sizeof(kOptions) / sizeof(kOptions[0])),
        };

        return ModSDK::Config::RegisterSchema(kSchema);
    }

    bool OnInitialize() {
        if (!RegisterTestSchema()) {
            ModSDK::Dialogs::ShowError("Failed to register the CoH Mod Config UI test configuration schema.");
            return false;
        }

        LogInfo("CoH Mod Config UI initialized with built-in test options");
        return true;
    }

    bool OnModsLoaded() {
        if (!gCatalog.Refresh()) {
            ModSDK::Dialogs::ShowError("Failed to enumerate registered mod configuration schemas.");
            return false;
        }

        LogInfo(
            "CoH Mod Config UI cataloged " +
            std::to_string(gCatalog.GetModCount()) +
            " mods and " +
            std::to_string(gCatalog.GetOptionCount()) +
            " options"
        );

        if (!ConfigUi::Frontend::Install(&gCatalog)) {
            ModSDK::Dialogs::ShowError("Failed to install the CoH Mod Config UI frontend bridge.");
            return false;
        }

        return true;
    }

    void OnShutdown() {
        ConfigUi::Frontend::Shutdown();
        gCatalog.Clear();
    }

    const CoHModSDKModuleV1 kModule = {
        COHMODSDK_ABI_VERSION,
        sizeof(CoHModSDKModuleV1),
        kModId,
        kModName,
        kModVersion,
        kModAuthor,
        &OnInitialize,
        &OnModsLoaded,
        &OnShutdown,
    };
}

COHMODSDK_EXPORT_MODULE(kModule);
