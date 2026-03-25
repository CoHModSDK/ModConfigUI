#include "UiInterop.hpp"

#include "CoHModSDK.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ConfigUi::Frontend {
    namespace {
        constexpr char kUserInterfaceModuleName[] = "UserInterface.dll";
        constexpr char kPlatformModuleName[] = "Platform.dll";
        constexpr char kLocalizerModuleName[] = "Localizer.dll";
        constexpr char kToggleKeyName[] = "F10";
        constexpr std::uintptr_t kCreateBlankScreenRva = 0x00036360u;
        constexpr std::size_t kOpaqueGenericWidgetStorageSize = 16384u;
        constexpr std::size_t kOpaqueButtonStorageSize = 16384u;
        constexpr std::size_t kOpaqueTextLabelStorageSize = 8192u;
        constexpr std::size_t kOpaqueLocStringStorageSize = 256u;
        constexpr std::size_t kVisibleRowCount = 5u;
        constexpr char kGroupWidgetTypeName[] = "Group";
        constexpr char kButtonWidgetTypeName[] = "Button";
        constexpr char kTextLabelWidgetTypeName[] = "TextLabel";
        constexpr char kScreenName[] = "cohmodconfigui_screen";
        constexpr char kTemplateScreenName[] = "prompt_performance_test";
        constexpr char kTemplateRootWidgetName[] = "grp_screen";
        constexpr char kTemplatePanelWidgetName[] = "perfGrp";
        constexpr char kTemplateButtonWidgetName[] = "btn_continue";
        constexpr char kTemplateTitleWidgetName[] = "title";
        constexpr char kTemplateLabelWidgetName[] = "minimumResults";
        constexpr char kRootWidgetName[] = "cohmodconfigui_root";
        constexpr char kPanelButtonName[] = "cohmodconfigui_panel";
        constexpr char kTitleLabelName[] = "cohmodconfigui_title";
        constexpr char kSummaryLabelName[] = "cohmodconfigui_summary";
        constexpr char kFooterLabelName[] = "cohmodconfigui_footer";
        constexpr char kRowButtonNamePrefix[] = "cohmodconfigui_row_";
        constexpr std::uintptr_t kWidgetFactoryCreateRva = 0x000421A0u;
        constexpr std::uintptr_t kFindWidgetExtensionRva = 0x00031810u;
        constexpr std::uintptr_t kAddRenderChildRva = 0x0003DBF0u;
        constexpr int kDrawChildrenExtensionId = 1;
        constexpr int kDefaultScreenActivationType = 0;
        constexpr int kWidgetFactoryCreateFlag = 1;
        constexpr float kNativeAspectRatio = 1.77778f;
        constexpr float kRootPositionX = 0.0f;
        constexpr float kRootPositionY = 0.0f;
        constexpr float kRootSizeX = 1.0f;
        constexpr float kRootSizeY = 1.0f;
        constexpr float kPanelPositionX = 0.25f;
        constexpr float kPanelPositionY = 0.15f;
        constexpr float kPanelSizeX = 0.50f;
        constexpr float kPanelSizeY = 0.65f;
        constexpr float kTitlePositionX = 0.05f;
        constexpr float kTitlePositionY = 0.02f;
        constexpr float kTitleSizeX = 0.90f;
        constexpr float kTitleSizeY = 0.06f;
        constexpr float kSummaryPositionX = 0.05f;
        constexpr float kSummaryPositionY = 0.09f;
        constexpr float kSummarySizeX = 0.90f;
        constexpr float kSummarySizeY = 0.06f;
        constexpr float kFooterPositionX = 0.05f;
        constexpr float kFooterPositionY = 0.92f;
        constexpr float kFooterSizeX = 0.90f;
        constexpr float kFooterSizeY = 0.06f;
        constexpr float kFirstRowPositionX = 0.05f;
        constexpr float kFirstRowPositionY = 0.22f;
        constexpr float kRowSpacingY = 0.06f;
        constexpr float kRowSizeX = 0.35f;
        constexpr float kRowSizeY = 0.035f;

        using ScreenManagerHandle = void;

        using GetScreenManagerFn = ScreenManagerHandle* (__stdcall*)();
        using ScreenManagerUpdateFn = void(__thiscall*)(ScreenManagerHandle* manager, float deltaTime);
        using ActivateScreenFn = void(__thiscall*)(ScreenManagerHandle* manager, void* screen, int activationType, bool skipTransition);
        using DeactivateScreenFn = void(__thiscall*)(ScreenManagerHandle* manager, void* screen);
        using CreateBlankScreenFn = void* (__thiscall*)(ScreenManagerHandle* manager);
        using SetTopMostFn = void(__thiscall*)(ScreenManagerHandle* manager, bool topMost);
        using ScreenSetNameFn = void(__thiscall*)(void* screen, const char* name);
        using ScreenSetHiddenFn = void(__thiscall*)(void* screen, bool hidden);
        using ScreenSetNativeAspectFn = void(__thiscall*)(void* screen, float nativeAspect);
        using ScreenGetRootWidgetFn = void* (__thiscall*)(void* screen);
        using LoadScreenByNameFn = void* (__thiscall*)(ScreenManagerHandle* manager, const char* screenName);
        using UnloadScreenFn = void(__thiscall*)(ScreenManagerHandle* manager, void* screen);
        using GetKeyFromNameFn = int(__stdcall*)(const char* keyName);
        using CheckInputQueueForKeyPressFn = bool(__stdcall*)(int key);
        using IsKeyPressedFn = bool(__stdcall*)(int key);

        using WidgetSetNameFn = void(__thiscall*)(void* widget, const char* name);
        using WidgetSetPositionFn = void(__thiscall*)(void* widget, float x, float y);
        using WidgetSetSizeFn = void(__thiscall*)(void* widget, float width, float height);
        using WidgetSetParentFn = void(__thiscall*)(void* widget, void* parent);
        using WidgetProxyBindFn = void(__thiscall*)(void* widgetProxy, void* widget);
        using WidgetProxyBindByNameFn = void(__thiscall*)(void* widgetProxy, const char* screenName, const char* widgetName);
        using WidgetProxyIsValidFn = bool(__thiscall*)(const void* widgetProxy);
        using WidgetProxySetVisibleFn = void(__thiscall*)(void* widgetProxy, bool visible);
        using WidgetProxySetEnabledFn = void(__thiscall*)(void* widgetProxy, bool enabled);
        using WidgetProxyForceActiveFn = void(__thiscall*)(void* widgetProxy, bool active);
        using GenericWidgetCtorFn = void(__thiscall*)(void* genericWidget);
        using GenericWidgetDtorFn = void(__thiscall*)(void* genericWidget);
        using ButtonCtorFn = void(__thiscall*)(void* button);
        using ButtonDtorFn = void(__thiscall*)(void* button);
        using ButtonSetTextFn = void(__thiscall*)(void* button, const void* locString);
        using TextLabelCtorFn = void(__thiscall*)(void* textLabel);
        using TextLabelDtorFn = void(__thiscall*)(void* textLabel);
        using TextLabelSetTextFn = void(__thiscall*)(void* textLabel, const void* locString);
        using TextLabelSetMultilineFn = void(__thiscall*)(void* textLabel, bool multiline);
        using TextLabelSetAutoSizeFn = void(__thiscall*)(void* textLabel, bool autoSize);
        using FindWidgetExtensionFn = void* (__thiscall*)(void* widget, int extensionId);
        using WidgetGetPresentationFn = void* (__thiscall*)(void* widget);
        using WidgetSetPresentationFn = void(__thiscall*)(void* widget, void* presentation);
        using WidgetGetHitAreaFn = void* (__thiscall*)(void* widget);
        using WidgetSetHitAreaFn = void(__thiscall*)(void* widget, void* hitArea);
        using WidgetProxyGetWidgetFn = void* (__thiscall*)(void* widgetProxy);

        // Widget::GetState(WidgetState) -> bool
        // WidgetState enum: Normal=0, Active=1, Hover=2, Disabled=3, Focused=4
        // (bitset<5> backing — these are independent flags)
        using WidgetGetStateFn = bool(__thiscall*)(const void* widget, int widgetState);
        constexpr int kWidgetStateActive = 1;

        using LocStringCtorFn = void(__thiscall*)(void* locString, const wchar_t* text);
        using LocStringDtorFn = void(__thiscall*)(void* locString);


        struct OpaqueGenericWidget {
            alignas(16) std::array<std::byte, kOpaqueGenericWidgetStorageSize> storage = {};
            void* Get() { return storage.data(); }
            const void* Get() const { return storage.data(); }
        };

        struct OpaqueButton {
            alignas(16) std::array<std::byte, kOpaqueButtonStorageSize> storage = {};
            void* Get() { return storage.data(); }
            const void* Get() const { return storage.data(); }
        };

        struct OpaqueTextLabel {
            alignas(16) std::array<std::byte, kOpaqueTextLabelStorageSize> storage = {};
            void* Get() { return storage.data(); }
            const void* Get() const { return storage.data(); }
        };

        struct OpaqueLocString {
            alignas(void*) std::array<std::byte, kOpaqueLocStringStorageSize> storage = {};
            void* Get() { return storage.data(); }
        };

        struct State {
            Catalog* catalog = nullptr;
            bool installed = false;
            bool overlayBuilt = false;
            bool overlayVisible = false;
            std::size_t selectedOptionIndex = 0u;
            int toggleKey = 0;
            void* screenManagerUpdateTarget = nullptr;
            GetScreenManagerFn getScreenManager = nullptr;
            ScreenManagerUpdateFn originalScreenManagerUpdate = nullptr;
            ActivateScreenFn activateScreen = nullptr;
            DeactivateScreenFn deactivateScreen = nullptr;
            CreateBlankScreenFn createBlankScreen = nullptr;
            SetTopMostFn setTopMost = nullptr;
            ScreenSetNameFn screenSetName = nullptr;
            ScreenSetHiddenFn screenSetHidden = nullptr;
            ScreenSetNativeAspectFn screenSetNativeAspect = nullptr;
            ScreenGetRootWidgetFn screenGetRootWidget = nullptr;
            LoadScreenByNameFn loadScreenByName = nullptr;
            UnloadScreenFn unloadScreen = nullptr;
            GetKeyFromNameFn getKeyFromName = nullptr;
            CheckInputQueueForKeyPressFn checkInputQueueForKeyPress = nullptr;
            IsKeyPressedFn isKeyPressed = nullptr;
            WidgetSetNameFn widgetSetName = nullptr;
            WidgetSetPositionFn widgetSetPosition = nullptr;
            WidgetSetSizeFn widgetSetSize = nullptr;
            WidgetSetParentFn widgetSetParent = nullptr;
            WidgetProxyBindFn widgetProxyBind = nullptr;
            WidgetProxyBindByNameFn widgetProxyBindByName = nullptr;
            WidgetProxyIsValidFn widgetProxyIsValid = nullptr;
            WidgetProxySetVisibleFn widgetProxySetVisible = nullptr;
            WidgetProxySetEnabledFn widgetProxySetEnabled = nullptr;
            WidgetProxyForceActiveFn widgetProxyForceActive = nullptr;
            GenericWidgetCtorFn genericWidgetCtor = nullptr;
            GenericWidgetDtorFn genericWidgetDtor = nullptr;
            ButtonCtorFn buttonCtor = nullptr;
            ButtonDtorFn buttonDtor = nullptr;
            ButtonSetTextFn buttonSetText = nullptr;
            TextLabelCtorFn textLabelCtor = nullptr;
            TextLabelDtorFn textLabelDtor = nullptr;
            TextLabelSetTextFn textLabelSetText = nullptr;
            TextLabelSetMultilineFn textLabelSetMultiline = nullptr;
            TextLabelSetAutoSizeFn textLabelSetAutoSize = nullptr;
            FindWidgetExtensionFn findWidgetExtension = nullptr;
            WidgetGetPresentationFn widgetGetPresentation = nullptr;
            WidgetSetPresentationFn widgetSetPresentation = nullptr;
            WidgetGetHitAreaFn widgetGetHitArea = nullptr;
            WidgetSetHitAreaFn widgetSetHitArea = nullptr;
            WidgetProxyGetWidgetFn widgetProxyGetWidget = nullptr;
            WidgetGetStateFn widgetGetState = nullptr;
            LocStringCtorFn locStringCtor = nullptr;
            LocStringDtorFn locStringDtor = nullptr;
            void* addRenderChildAddress = nullptr;
            void* widgetFactoryCreateAddress = nullptr;
            bool updateHookObserved = false;
            bool toggleKeyWasDown = false;
            bool toggleInputObserved = false;
            void* screen = nullptr;
            void* templateScreen = nullptr;
            void* rootWidgetRaw = nullptr;
            void* panelWidgetRaw = nullptr;
            void* titleLabelRaw = nullptr;
            void* summaryLabelRaw = nullptr;
            void* footerLabelRaw = nullptr;
            std::array<void*, kVisibleRowCount> rowButtonWidgets = {};
            OpaqueTextLabel titleLabel = {};
            OpaqueTextLabel summaryLabel = {};
            OpaqueTextLabel footerLabel = {};
            std::array<OpaqueButton, kVisibleRowCount> rowButtons = {};
            std::array<bool, kVisibleRowCount> rowButtonWasActive = {};
        };

        struct SelectedOptionRef {
            std::size_t flatIndex = 0u;
            std::size_t optionCount = 0u;
            ModEntry* modEntry = nullptr;
            OptionEntry* optionEntry = nullptr;
        };

        State& GetState() {
            static State state;
            return state;
        }

        void LogInfo(const std::string& message) {
            ModSDK::Runtime::Log(CoHModSDKLogLevel_Info, message.c_str());
        }

        void LogWarning(const std::string& message) {
            ModSDK::Runtime::Log(CoHModSDKLogLevel_Warning, message.c_str());
        }

        void LogError(const std::string& message) {
            ModSDK::Runtime::Log(CoHModSDKLogLevel_Error, message.c_str());
        }

        template <typename T>
        bool ResolveExport(HMODULE moduleHandle, const char* exportName, T& outFunction) {
            if (moduleHandle == nullptr) {
                return false;
            }

            const FARPROC exportAddress = GetProcAddress(moduleHandle, exportName);
            if (exportAddress == nullptr) {
                return false;
            }

            outFunction = reinterpret_cast<T>(exportAddress);
            return true;
        }

        HMODULE AcquireModule(const char* moduleName) {
            if (moduleName == nullptr) {
                return nullptr;
            }

            HMODULE moduleHandle = GetModuleHandleA(moduleName);
            if (moduleHandle != nullptr) {
                return moduleHandle;
            }

            moduleHandle = LoadLibraryA(moduleName);
            if (moduleHandle != nullptr) {
                LogInfo("CoH Mod Config UI loaded module " + std::string(moduleName) + " explicitly.");
                return moduleHandle;
            }

            LogError(
                "CoH Mod Config UI could not load required module " +
                std::string(moduleName) +
                " (Win32 error " +
                std::to_string(GetLastError()) +
                ")."
            );
            return nullptr;
        }

        template <typename T>
        bool ResolveRequiredExport(HMODULE moduleHandle, const char* moduleName, const char* exportName, T& outFunction) {
            if (ResolveExport(moduleHandle, exportName, outFunction)) {
                return true;
            }

            LogError(
                "CoH Mod Config UI could not resolve required export " +
                std::string(exportName) +
                " from " +
                std::string(moduleName == nullptr ? "<unknown>" : moduleName) +
                "."
            );
            return false;
        }

        template <typename T>
        void ResolveOptionalExport(HMODULE moduleHandle, const char* exportName, T& outFunction) {
            ResolveExport(moduleHandle, exportName, outFunction);
        }

        std::wstring ToWide(const std::string& text) {
            if (text.empty()) {
                return std::wstring();
            }

            const int required = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            if (required <= 0) {
                return std::wstring();
            }

            std::wstring wideText(static_cast<std::size_t>(required), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wideText.data(), required);
            if (!wideText.empty() && (wideText.back() == L'\0')) {
                wideText.pop_back();
            }

            return wideText;
        }

        std::string FormatFloat(float value) {
            std::string text = std::to_string(value);
            const std::size_t decimalPoint = text.find('.');
            if (decimalPoint == std::string::npos) {
                return text;
            }

            while (!text.empty() && (text.back() == '0')) {
                text.pop_back();
            }

            if (!text.empty() && (text.back() == '.')) {
                text.pop_back();
            }

            return text;
        }

        std::string TruncateText(std::string_view text, std::size_t maxLength) {
            if (text.size() <= maxLength) {
                return std::string(text);
            }

            if (maxLength <= 3u) {
                return std::string(maxLength, '.');
            }

            return std::string(text.substr(0u, maxLength - 3u)) + "...";
        }

        std::string FormatValue(const OptionEntry& optionEntry, const CoHModSDKConfigValueV1& value) {
            switch (value.type) {
            case CoHModSDKConfigType_Bool:
                return value.boolValue != 0u ? "true" : "false";
            case CoHModSDKConfigType_Int:
                return std::to_string(value.intValue);
            case CoHModSDKConfigType_Float:
                return FormatFloat(value.floatValue);
            case CoHModSDKConfigType_Enum:
                for (const ChoiceEntry& choiceEntry : optionEntry.choices) {
                    if (choiceEntry.value == value.enumValue) {
                        if (!choiceEntry.label.empty()) {
                            return choiceEntry.label;
                        }

                        if (!choiceEntry.valueId.empty()) {
                            return choiceEntry.valueId;
                        }

                        break;
                    }
                }

                return std::to_string(value.enumValue);
            default:
                return "<unknown>";
            }
        }

        std::string FormatCurrentValue(const OptionEntry& optionEntry) {
            return FormatValue(optionEntry, optionEntry.currentValue);
        }

        std::string FormatDefaultValue(const OptionEntry& optionEntry) {
            return FormatValue(optionEntry, optionEntry.defaultValue);
        }

        std::string BuildDetailedLogSummary(const Catalog& catalog) {
            std::string summary;
            summary.reserve(512u);

            for (const ModEntry& modEntry : catalog.GetMods()) {
                summary += modEntry.modId;
                summary += ": ";

                bool first = true;
                for (const OptionEntry& optionEntry : modEntry.options) {
                    if (!first) {
                        summary += ", ";
                    }

                    summary += optionEntry.optionId;
                    summary += "=";
                    summary += FormatCurrentValue(optionEntry);
                    first = false;

                    if (summary.size() >= 600u) {
                        summary += ", ...";
                        return summary;
                    }
                }

                summary += " | ";

                if (summary.size() >= 600u) {
                    summary += "...";
                    return summary;
                }
            }

            if (summary.size() >= 3u) {
                summary.resize(summary.size() - 3u);
            }

            return summary;
        }

        std::size_t GetOptionCount(const Catalog& catalog) {
            std::size_t optionCount = 0u;
            for (const ModEntry& modEntry : catalog.GetMods()) {
                optionCount += modEntry.options.size();
            }

            return optionCount;
        }

        bool TryGetOptionByFlatIndex(Catalog& catalog, std::size_t flatIndex, SelectedOptionRef& outSelectedOption) {
            outSelectedOption = {};
            outSelectedOption.optionCount = GetOptionCount(catalog);
            if (flatIndex >= outSelectedOption.optionCount) {
                return false;
            }

            std::size_t currentFlatIndex = 0u;
            for (ModEntry& modEntry : catalog.GetMods()) {
                for (OptionEntry& optionEntry : modEntry.options) {
                    if (currentFlatIndex == flatIndex) {
                        outSelectedOption.flatIndex = currentFlatIndex;
                        outSelectedOption.modEntry = &modEntry;
                        outSelectedOption.optionEntry = &optionEntry;
                        return true;
                    }

                    ++currentFlatIndex;
                }
            }

            return false;
        }

        bool TryGetSelectedOption(State& state, SelectedOptionRef& outSelectedOption) {
            if (state.catalog == nullptr) {
                return false;
            }

            outSelectedOption = {};
            outSelectedOption.optionCount = GetOptionCount(*state.catalog);
            if (outSelectedOption.optionCount == 0u) {
                state.selectedOptionIndex = 0u;
                return false;
            }

            if (state.selectedOptionIndex >= outSelectedOption.optionCount) {
                state.selectedOptionIndex = outSelectedOption.optionCount - 1u;
            }

            return TryGetOptionByFlatIndex(*state.catalog, state.selectedOptionIndex, outSelectedOption);
        }

        std::string BuildTitleText(const SelectedOptionRef* selectedOption) {
            if (selectedOption == nullptr) {
                return "Mod Options";
            }

            return "Mod Options  " +
                std::to_string(selectedOption->flatIndex + 1u) +
                "/" +
                std::to_string(selectedOption->optionCount);
        }

        std::string BuildSummaryText(const SelectedOptionRef& selectedOption) {
            const OptionEntry& optionEntry = *selectedOption.optionEntry;

            if (!optionEntry.description.empty()) {
                return TruncateText(optionEntry.description, 90u);
            }

            return TruncateText(optionEntry.optionId, 40u) + "  (default: " + TruncateText(FormatDefaultValue(optionEntry), 18u) + ")";
        }

        std::string BuildRowLabelText(const SelectedOptionRef& optionRef) {
            std::string label;
            label += TruncateText(
                optionRef.optionEntry->label.empty() ? optionRef.optionEntry->optionId : optionRef.optionEntry->label,
                22u
            );

            if ((optionRef.optionEntry->flags & CoHModSDKConfigFlags_RestartRequired) != 0u) {
                label += " *";
            }

            return label;
        }

        std::string BuildRowValueText(const SelectedOptionRef& optionRef) {
            return TruncateText(FormatCurrentValue(*optionRef.optionEntry), 10u);
        }

        bool ResolveInterop(State& state) {
            HMODULE userInterfaceModule = AcquireModule(kUserInterfaceModuleName);
            HMODULE platformModule = AcquireModule(kPlatformModuleName);
            HMODULE localizerModule = AcquireModule(kLocalizerModuleName);

            if ((userInterfaceModule == nullptr) || (platformModule == nullptr) || (localizerModule == nullptr)) {
                return false;
            }

            const bool resolved =
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?i@ScreenManager@UI@@SGPAV12@XZ", state.getScreenManager) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?Update@ScreenManager@UI@@QAEXM@Z", state.screenManagerUpdateTarget) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?ActivateScreen@ScreenManager@UI@@QAEXPAVScreen@2@W4ScreenActivationType@12@_N@Z", state.activateScreen) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?DeactivateScreen@ScreenManager@UI@@QAEXPAVScreen@2@@Z", state.deactivateScreen) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetTopMost@ScreenManager@UI@@QAEX_N@Z", state.setTopMost) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetName@Screen@UI@@QAEXPBD@Z", state.screenSetName) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?GetRootWidget@Screen@UI@@QAEPAVWidget@2@XZ", state.screenGetRootWidget) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?LoadScreen@ScreenManager@UI@@QAEPAVScreen@2@PBD@Z", state.loadScreenByName) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?UnloadScreen@ScreenManager@UI@@QAEXPAVScreen@2@@Z", state.unloadScreen) &&
                ResolveRequiredExport(platformModule, kPlatformModuleName, "?GetKeyFromName@Input@Plat@@YG?AW4InputKey@2@PBD@Z", state.getKeyFromName) &&
                ResolveRequiredExport(platformModule, kPlatformModuleName, "?CheckInputQueueForKeyPress@Input@Plat@@YG_NW4InputKey@2@@Z", state.checkInputQueueForKeyPress) &&
                ResolveRequiredExport(platformModule, kPlatformModuleName, "?IsKeyPressed@Input@Plat@@YG_NW4InputKey@2@@Z", state.isKeyPressed) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetPosition@Widget@UI@@QAEXMM@Z", state.widgetSetPosition) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetSize@Widget@UI@@QAEXMM@Z", state.widgetSetSize) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetParent@Widget@UI@@QAEXPAV12@@Z", state.widgetSetParent) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?Bind@WidgetProxy@UI@@IAEXPAVWidget@2@@Z", state.widgetProxyBind) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?Bind@WidgetProxy@UI@@QAEXPBD0@Z", state.widgetProxyBindByName) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?IsValid@WidgetProxy@UI@@QBE_NXZ", state.widgetProxyIsValid) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetVisible@WidgetProxy@UI@@UAEX_N@Z", state.widgetProxySetVisible) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetEnabled@WidgetProxy@UI@@UAEX_N@Z", state.widgetProxySetEnabled) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?ForceActive@WidgetProxy@UI@@UAEX_N@Z", state.widgetProxyForceActive) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??0GenericWidget@UI@@QAE@XZ", state.genericWidgetCtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??1GenericWidget@UI@@UAE@XZ", state.genericWidgetDtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??0Button@UI@@QAE@XZ", state.buttonCtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??1Button@UI@@UAE@XZ", state.buttonDtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetText@Button@UI@@QAEXABVLocString@@@Z", state.buttonSetText) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??0TextLabel@UI@@QAE@XZ", state.textLabelCtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??1TextLabel@UI@@UAE@XZ", state.textLabelDtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetText@TextLabel@UI@@QAEXABVLocString@@@Z", state.textLabelSetText) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?GetPresentation@Widget@UI@@QAEPAVPresentation@2@XZ", state.widgetGetPresentation) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetPresentation@Widget@UI@@QAEXPAVPresentation@2@@Z", state.widgetSetPresentation) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?GetHitArea@Widget@UI@@QAEPAVHitArea@2@XZ", state.widgetGetHitArea) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetHitArea@Widget@UI@@QAEXPAVHitArea@2@@Z", state.widgetSetHitArea) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?GetWidget@WidgetProxy@UI@@IAEPAVWidget@2@XZ", state.widgetProxyGetWidget) &&
                ResolveRequiredExport(localizerModule, kLocalizerModuleName, "??0LocString@@QAE@PB_W@Z", state.locStringCtor) &&
                ResolveRequiredExport(localizerModule, kLocalizerModuleName, "??1LocString@@QAE@XZ", state.locStringDtor);

            if (!resolved) {
                return false;
            }

            const std::uintptr_t userInterfaceBase = reinterpret_cast<std::uintptr_t>(userInterfaceModule);
            state.createBlankScreen = reinterpret_cast<CreateBlankScreenFn>(userInterfaceBase + kCreateBlankScreenRva);
            state.findWidgetExtension = reinterpret_cast<FindWidgetExtensionFn>(userInterfaceBase + kFindWidgetExtensionRva);
            state.addRenderChildAddress = reinterpret_cast<void*>(userInterfaceBase + kAddRenderChildRva);
            state.widgetFactoryCreateAddress = reinterpret_cast<void*>(userInterfaceBase + kWidgetFactoryCreateRva);

            ResolveOptionalExport(
                userInterfaceModule,
                "?SetName@Widget@UI@@QAEXPBD@Z",
                state.widgetSetName
            );
            ResolveOptionalExport(userInterfaceModule, "?SetHidden@Screen@UI@@QAEX_N@Z", state.screenSetHidden);
            ResolveOptionalExport(userInterfaceModule, "?SetNativeAspect@Screen@UI@@QAEXM@Z", state.screenSetNativeAspect);
            ResolveOptionalExport(userInterfaceModule, "?SetMultiline@TextLabel@UI@@QAEX_N@Z", state.textLabelSetMultiline);
            ResolveOptionalExport(userInterfaceModule, "?SetAutoSize@TextLabel@UI@@QAEX_N@Z", state.textLabelSetAutoSize);
            ResolveOptionalExport(userInterfaceModule, "?GetState@Widget@UI@@QBE_NW4WidgetState@12@@Z", state.widgetGetState);
            return true;
        }

        bool IsEdgePressed(State& state, int key, bool& wasDown) {
            if (key == 0) {
                return false;
            }

            const bool queuePressed =
                (state.checkInputQueueForKeyPress != nullptr) &&
                state.checkInputQueueForKeyPress(key);
            const bool keyDown =
                (state.isKeyPressed != nullptr) &&
                state.isKeyPressed(key);

            const bool isDown = queuePressed || keyDown;
            const bool pressed = isDown && !wasDown;
            wasDown = isDown;
            return pressed;
        }

        ScreenManagerHandle* GetScreenManager(State& state) {
            return state.getScreenManager == nullptr ? nullptr : state.getScreenManager();
        }

        void* GetScreenRootWidget(State& state, void* screen) {
            if ((screen == nullptr) || (state.screenGetRootWidget == nullptr)) {
                return nullptr;
            }

            return state.screenGetRootWidget(screen);
        }

        std::string BuildFooterText() {
            return "F10 close";
        }

        std::string BuildRowButtonText(const SelectedOptionRef& optionRef) {
            std::string rowText = BuildRowLabelText(optionRef);
            rowText += "    ";
            rowText += BuildRowValueText(optionRef);
            return TruncateText(rowText, 64u);
        }

        std::string BuildEmptyButtonText() {
            return " ";
        }

        std::string MakeRowButtonName(std::size_t rowIndex) {
            return std::string(kRowButtonNamePrefix) + std::to_string(rowIndex);
        }

        void ResetOverlayHandles(State& state) {
            state.rootWidgetRaw = nullptr;
            state.panelWidgetRaw = nullptr;
            state.titleLabelRaw = nullptr;
            state.summaryLabelRaw = nullptr;
            state.footerLabelRaw = nullptr;
            state.rowButtonWidgets.fill(nullptr);
        }

        void* CreateRawWidgetByType(State& state, const char* widgetTypeName) {
            if ((state.widgetFactoryCreateAddress == nullptr) || (widgetTypeName == nullptr)) {
                return nullptr;
            }

            void* widget = nullptr;
#if defined(_M_IX86)
            void* createFunction = state.widgetFactoryCreateAddress;
            __asm {
                mov edi, widgetTypeName
                mov eax, createFunction
                push kWidgetFactoryCreateFlag
                call eax
                mov widget, eax
            }
#else
            (void)state;
            (void)widgetTypeName;
#endif
            return widget;
        }

        void ConfigureRawWidget(
            State& state,
            void* widget,
            const char* name,
            float positionX,
            float positionY,
            float sizeX,
            float sizeY,
            void* parent = nullptr
        ) {
            if ((widget == nullptr) || (state.widgetSetPosition == nullptr) || (state.widgetSetSize == nullptr)) {
                return;
            }

            if ((parent != nullptr) && (state.widgetSetParent != nullptr)) {
                state.widgetSetParent(widget, parent);
            }

            if ((state.widgetSetName != nullptr) && (name != nullptr) && (name[0] != '\0')) {
                state.widgetSetName(widget, name);
            }

            state.widgetSetPosition(widget, positionX, positionY);
            state.widgetSetSize(widget, sizeX, sizeY);
        }

        bool BindButtonProxy(State& state, OpaqueButton& button, void* rawWidget) {
            if ((rawWidget == nullptr) || (state.buttonCtor == nullptr) || (state.widgetProxyBind == nullptr)) {
                return false;
            }

            state.buttonCtor(button.Get());
            state.widgetProxyBind(button.Get(), rawWidget);
            return true;
        }

        bool BindTextLabelProxy(State& state, OpaqueTextLabel& textLabel, void* rawWidget) {
            if ((rawWidget == nullptr) || (state.textLabelCtor == nullptr) || (state.widgetProxyBind == nullptr)) {
                return false;
            }

            state.textLabelCtor(textLabel.Get());
            state.widgetProxyBind(textLabel.Get(), rawWidget);
            return true;
        }

        void* FindWidgetExtensionObject(State& state, void* widget, int extensionId) {
            if ((widget == nullptr) || (state.findWidgetExtension == nullptr) || (extensionId == 0)) {
                return nullptr;
            }

            return state.findWidgetExtension(widget, extensionId);
        }

        bool AddRenderChild(State& state, void* parentRenderObject, void* childWidget, int insertionIndex = -1) {
            if ((parentRenderObject == nullptr) || (childWidget == nullptr) || (state.addRenderChildAddress == nullptr)) {
                return false;
            }

#if defined(_M_IX86)
            void* addRenderChildFunction = state.addRenderChildAddress;
            __asm {
                mov edi, parentRenderObject
                mov eax, addRenderChildFunction
                push insertionIndex
                push childWidget
                call eax
            }
            return true;
#else
            (void)state;
            (void)parentRenderObject;
            (void)childWidget;
            (void)insertionIndex;
            return false;
#endif
        }

        bool AttachRenderChild(State& state, void* parentWidget, void* childWidget) {
            void* const parentRenderObject = FindWidgetExtensionObject(state, parentWidget, kDrawChildrenExtensionId);
            if (parentRenderObject == nullptr) {
                return false;
            }

            return AddRenderChild(state, parentRenderObject, childWidget);
        }

        bool EnsureTemplateScreenLoaded(State& state) {
            if (state.templateScreen != nullptr) {
                return true;
            }

            ScreenManagerHandle* const screenManager = GetScreenManager(state);
            if ((screenManager == nullptr) || (state.loadScreenByName == nullptr)) {
                return false;
            }

            LogInfo("CoH Mod Config UI is loading the template donor screen.");
            state.templateScreen = state.loadScreenByName(screenManager, kTemplateScreenName);
            if (state.templateScreen == nullptr) {
                LogError("CoH Mod Config UI failed to load the template donor screen.");
                return false;
            }

            return true;
        }

        void ApplyWidgetProxyState(State& state, void* widgetProxy) {
            if (widgetProxy == nullptr) {
                return;
            }

            if (state.widgetProxySetVisible != nullptr) {
                state.widgetProxySetVisible(widgetProxy, true);
            }

            if (state.widgetProxySetEnabled != nullptr) {
                state.widgetProxySetEnabled(widgetProxy, true);
            }

            if (state.widgetProxyForceActive != nullptr) {
                state.widgetProxyForceActive(widgetProxy, true);
            }
        }

        bool TransferDonorPresentation(State& state, void* targetRawWidget, const char* donorWidgetName) {
            if ((targetRawWidget == nullptr) ||
                (donorWidgetName == nullptr) ||
                (state.widgetGetPresentation == nullptr) ||
                (state.widgetSetPresentation == nullptr) ||
                (state.widgetProxyGetWidget == nullptr) ||
                (state.genericWidgetCtor == nullptr) ||
                (state.genericWidgetDtor == nullptr) ||
                (state.widgetProxyBindByName == nullptr) ||
                (state.widgetProxyIsValid == nullptr)) {
                return false;
            }

            if (!EnsureTemplateScreenLoaded(state)) {
                return false;
            }

            OpaqueGenericWidget donorProxy = {};
            state.genericWidgetCtor(donorProxy.Get());

            const auto cleanup = [&]() {
                state.genericWidgetDtor(donorProxy.Get());
            };

            state.widgetProxyBindByName(donorProxy.Get(), kTemplateScreenName, donorWidgetName);

            if (!state.widgetProxyIsValid(donorProxy.Get())) {
                LogError(
                    "CoH Mod Config UI could not find donor widget '" +
                    std::string(donorWidgetName) +
                    "' in the template screen."
                );
                cleanup();
                return false;
            }

            void* donorRawWidget = state.widgetProxyGetWidget(donorProxy.Get());
            if (donorRawWidget != nullptr) {
                void* presentation = state.widgetGetPresentation(donorRawWidget);
                if (presentation != nullptr) {
                    state.widgetSetPresentation(targetRawWidget, presentation);
                }

                if (state.widgetGetHitArea != nullptr && state.widgetSetHitArea != nullptr) {
                    void* hitArea = state.widgetGetHitArea(donorRawWidget);
                    if (hitArea != nullptr) {
                        state.widgetSetHitArea(targetRawWidget, hitArea);
                    }
                }
            }

            cleanup();
            return true;
        }

        bool TrySetTextLabel(State& state, OpaqueTextLabel& textLabel, const std::string& text, bool multiline) {
            if ((state.textLabelSetText == nullptr) || (state.locStringCtor == nullptr) || (state.locStringDtor == nullptr)) {
                return false;
            }

            if (state.textLabelSetAutoSize != nullptr) {
                state.textLabelSetAutoSize(textLabel.Get(), false);
            }

            if (state.textLabelSetMultiline != nullptr) {
                state.textLabelSetMultiline(textLabel.Get(), multiline);
            }

            const std::wstring wideText = ToWide(text);
            OpaqueLocString locString = {};
            state.locStringCtor(locString.Get(), wideText.c_str());
            state.textLabelSetText(textLabel.Get(), locString.Get());
            state.locStringDtor(locString.Get());
            return true;
        }

        bool TrySetButtonText(State& state, OpaqueButton& button, const std::string& text) {
            if ((state.buttonSetText == nullptr) || (state.locStringCtor == nullptr) || (state.locStringDtor == nullptr)) {
                return false;
            }

            const std::wstring wideText = ToWide(text);
            OpaqueLocString locString = {};
            state.locStringCtor(locString.Get(), wideText.c_str());
            state.buttonSetText(button.Get(), locString.Get());
            state.locStringDtor(locString.Get());
            return true;
        }

        bool BuildOverlay(State& state) {
            if (state.overlayBuilt) {
                return true;
            }

            ScreenManagerHandle* const screenManager = GetScreenManager(state);
            if (screenManager == nullptr) {
                LogError("CoH Mod Config UI cannot build the overlay because the ScreenManager is unavailable.");
                return false;
            }

            if ((state.createBlankScreen == nullptr) ||
                (state.widgetFactoryCreateAddress == nullptr) ||
                (state.buttonCtor == nullptr) ||
                (state.textLabelCtor == nullptr) ||
                (state.widgetProxyBind == nullptr)) {
                LogError("CoH Mod Config UI cannot build the overlay because one or more raw UI construction functions are unavailable.");
                return false;
            }

            LogInfo("CoH Mod Config UI is building the code-generated screen and raw widget tree through the native blank-screen path.");

            state.screen = nullptr;
            state.titleLabel = {};
            state.summaryLabel = {};
            state.footerLabel = {};
            for (OpaqueButton& rowButton : state.rowButtons) {
                rowButton = {};
            }

            ResetOverlayHandles(state);

            bool titleProxyConstructed = false;
            bool summaryProxyConstructed = false;
            bool footerProxyConstructed = false;
            std::size_t rowProxyCount = 0u;

            const auto cleanupFailedBuild = [&]() {
                for (std::size_t rowIndex = rowProxyCount; rowIndex > 0u; --rowIndex) {
                    state.buttonDtor(state.rowButtons[rowIndex - 1u].Get());
                }

                if (footerProxyConstructed) {
                    state.textLabelDtor(state.footerLabel.Get());
                }

                if (summaryProxyConstructed) {
                    state.textLabelDtor(state.summaryLabel.Get());
                }

                if (titleProxyConstructed) {
                    state.textLabelDtor(state.titleLabel.Get());
                }

                if ((state.screen != nullptr) && (state.unloadScreen != nullptr)) {
                    state.unloadScreen(screenManager, state.screen);
                    state.screen = nullptr;
                }

                ResetOverlayHandles(state);
                state.overlayBuilt = false;
                state.overlayVisible = false;
            };

            state.screen = state.createBlankScreen(screenManager);
            if (state.screen == nullptr) {
                LogError("CoH Mod Config UI failed to create a native blank UI::Screen.");
                cleanupFailedBuild();
                return false;
            }

            state.screenSetName(state.screen, kScreenName);
            if (state.screenSetHidden != nullptr) {
                state.screenSetHidden(state.screen, false);
            }
            if (state.screenSetNativeAspect != nullptr) {
                state.screenSetNativeAspect(state.screen, kNativeAspectRatio);
            }

            state.rootWidgetRaw = GetScreenRootWidget(state, state.screen);
            state.panelWidgetRaw = CreateRawWidgetByType(state, kGroupWidgetTypeName);
            state.titleLabelRaw = CreateRawWidgetByType(state, kTextLabelWidgetTypeName);
            state.summaryLabelRaw = CreateRawWidgetByType(state, kTextLabelWidgetTypeName);
            state.footerLabelRaw = CreateRawWidgetByType(state, kTextLabelWidgetTypeName);
            for (std::size_t rowIndex = 0u; rowIndex < state.rowButtonWidgets.size(); ++rowIndex) {
                state.rowButtonWidgets[rowIndex] = CreateRawWidgetByType(state, kButtonWidgetTypeName);
            }

            if ((state.rootWidgetRaw == nullptr) ||
                (state.panelWidgetRaw == nullptr) ||
                (state.titleLabelRaw == nullptr) ||
                (state.summaryLabelRaw == nullptr) ||
                (state.footerLabelRaw == nullptr) ||
                std::any_of(
                    state.rowButtonWidgets.begin(),
                    state.rowButtonWidgets.end(),
                    [](void* widget) { return widget == nullptr; })) {
                LogError("CoH Mod Config UI failed to create one or more raw widgets through WidgetFactory.");
                cleanupFailedBuild();
                return false;
            }

            const bool templateCloned =
                TransferDonorPresentation(state, state.rootWidgetRaw, kTemplateRootWidgetName) &&
                TransferDonorPresentation(state, state.panelWidgetRaw, kTemplatePanelWidgetName) &&
                TransferDonorPresentation(state, state.titleLabelRaw, kTemplateTitleWidgetName) &&
                TransferDonorPresentation(state, state.summaryLabelRaw, kTemplateLabelWidgetName) &&
                TransferDonorPresentation(state, state.footerLabelRaw, kTemplateLabelWidgetName);

            if (!templateCloned) {
                LogError("CoH Mod Config UI failed to transfer Presentation from one or more donor widgets.");
                cleanupFailedBuild();
                return false;
            }

            ConfigureRawWidget(
                state,
                state.rootWidgetRaw,
                nullptr,
                kRootPositionX,
                kRootPositionY,
                kRootSizeX,
                kRootSizeY
            );

            for (std::size_t rowIndex = 0u; rowIndex < state.rowButtonWidgets.size(); ++rowIndex) {
                if (!TransferDonorPresentation(state, state.rowButtonWidgets[rowIndex], kTemplateButtonWidgetName)) {
                    LogError("CoH Mod Config UI failed to transfer Presentation from the donor button to a row widget.");
                    cleanupFailedBuild();
                    return false;
                }
            }

            ConfigureRawWidget(
                state,
                state.panelWidgetRaw,
                kPanelButtonName,
                kPanelPositionX,
                kPanelPositionY,
                kPanelSizeX,
                kPanelSizeY,
                state.rootWidgetRaw
            );
            ConfigureRawWidget(
                state,
                state.titleLabelRaw,
                kTitleLabelName,
                kTitlePositionX,
                kTitlePositionY,
                kTitleSizeX,
                kTitleSizeY,
                state.panelWidgetRaw
            );
            ConfigureRawWidget(
                state,
                state.summaryLabelRaw,
                kSummaryLabelName,
                kSummaryPositionX,
                kSummaryPositionY,
                kSummarySizeX,
                kSummarySizeY,
                state.panelWidgetRaw
            );
            ConfigureRawWidget(
                state,
                state.footerLabelRaw,
                kFooterLabelName,
                kFooterPositionX,
                kFooterPositionY,
                kFooterSizeX,
                kFooterSizeY,
                state.panelWidgetRaw
            );

            for (std::size_t rowIndex = 0u; rowIndex < state.rowButtonWidgets.size(); ++rowIndex) {
                const std::string rowButtonName = MakeRowButtonName(rowIndex);
                ConfigureRawWidget(
                    state,
                    state.rowButtonWidgets[rowIndex],
                    rowButtonName.c_str(),
                    kFirstRowPositionX,
                    kFirstRowPositionY + (static_cast<float>(rowIndex) * kRowSpacingY),
                    kRowSizeX,
                    kRowSizeY,
                    state.panelWidgetRaw
                );
            }


            if (!AttachRenderChild(state, state.rootWidgetRaw, state.panelWidgetRaw) ||
                !AttachRenderChild(state, state.panelWidgetRaw, state.titleLabelRaw) ||
                !AttachRenderChild(state, state.panelWidgetRaw, state.summaryLabelRaw) ||
                !AttachRenderChild(state, state.panelWidgetRaw, state.footerLabelRaw)) {
                LogError("CoH Mod Config UI failed to attach one or more generated widgets to the renderer child graph.");
                cleanupFailedBuild();
                return false;
            }

            for (std::size_t rowIndex = 0u; rowIndex < state.rowButtonWidgets.size(); ++rowIndex) {
                const std::string rowButtonName = MakeRowButtonName(rowIndex);
                if (!AttachRenderChild(state, state.panelWidgetRaw, state.rowButtonWidgets[rowIndex])) {
                    LogError("CoH Mod Config UI failed to attach one of the generated row widgets to the renderer child graph.");
                    cleanupFailedBuild();
                    return false;
                }
            }

            if (!BindTextLabelProxy(state, state.titleLabel, state.titleLabelRaw)) {
                LogError("CoH Mod Config UI failed to bind the title label proxy.");
                cleanupFailedBuild();
                return false;
            }
            titleProxyConstructed = true;

            if (!BindTextLabelProxy(state, state.summaryLabel, state.summaryLabelRaw)) {
                LogError("CoH Mod Config UI failed to bind the summary label proxy.");
                cleanupFailedBuild();
                return false;
            }
            summaryProxyConstructed = true;

            if (!BindTextLabelProxy(state, state.footerLabel, state.footerLabelRaw)) {
                LogError("CoH Mod Config UI failed to bind the footer label proxy.");
                cleanupFailedBuild();
                return false;
            }
            footerProxyConstructed = true;

            ApplyWidgetProxyState(state, state.titleLabel.Get());
            ApplyWidgetProxyState(state, state.summaryLabel.Get());
            ApplyWidgetProxyState(state, state.footerLabel.Get());

            for (std::size_t rowIndex = 0u; rowIndex < state.rowButtons.size(); ++rowIndex) {
                if (!BindButtonProxy(state, state.rowButtons[rowIndex], state.rowButtonWidgets[rowIndex])) {
                    LogError("CoH Mod Config UI failed to bind one of the row button proxies.");
                    cleanupFailedBuild();
                    return false;
                }

                ApplyWidgetProxyState(state, state.rowButtons[rowIndex].Get());
                ++rowProxyCount;
            }

            const bool initialized =
                TrySetTextLabel(state, state.titleLabel, "Mod Options", false) &&
                TrySetTextLabel(state, state.summaryLabel, "Preparing mod configuration catalog...", true) &&
                TrySetTextLabel(state, state.footerLabel, BuildFooterText(), true);

            if (!initialized) {
                LogError("CoH Mod Config UI failed while assigning initial text to the code-generated overlay.");
                cleanupFailedBuild();
                return false;
            }

            for (OpaqueButton& rowButton : state.rowButtons) {
                if (!TrySetButtonText(state, rowButton, BuildEmptyButtonText())) {
                    LogError("CoH Mod Config UI failed while assigning initial text to one of the row widgets.");
                    cleanupFailedBuild();
                    return false;
                }
            }

            state.overlayBuilt = true;
            LogInfo("CoH Mod Config UI finished building the code-generated screen.");
            return true;
        }

        void DestroyOverlay(State& state) {
            if (!state.overlayBuilt && (state.screen == nullptr)) {
                return;
            }

            if (state.overlayBuilt) {
                for (std::size_t rowIndex = state.rowButtons.size(); rowIndex > 0u; --rowIndex) {
                    state.buttonDtor(state.rowButtons[rowIndex - 1u].Get());
                }

                state.textLabelDtor(state.footerLabel.Get());
                state.textLabelDtor(state.summaryLabel.Get());
                state.textLabelDtor(state.titleLabel.Get());
            }

            ScreenManagerHandle* const screenManager = GetScreenManager(state);
            if ((state.screen != nullptr) && (screenManager != nullptr) && (state.unloadScreen != nullptr)) {
                state.unloadScreen(screenManager, state.screen);
                state.screen = nullptr;
            }

            ResetOverlayHandles(state);
            state.overlayBuilt = false;
            state.overlayVisible = false;
        }

        bool RefreshVisibleMenu(State& state) {
            if (!BuildOverlay(state)) {
                return false;
            }

            LogInfo("CoH Mod Config UI is refreshing the visible overlay state.");

            SelectedOptionRef selectedOption = {};
            const bool hasSelection = TryGetSelectedOption(state, selectedOption);

            bool updatedAllWidgets =
                TrySetTextLabel(state, state.titleLabel, BuildTitleText(hasSelection ? &selectedOption : nullptr), false) &&
                TrySetTextLabel(
                    state,
                    state.summaryLabel,
                    hasSelection ? BuildSummaryText(selectedOption) : std::string("No registered mod options were found. F10 closes."),
                    true
                ) &&
                TrySetTextLabel(state, state.footerLabel, BuildFooterText(), true);

            std::size_t firstVisibleIndex = 0u;
            if (hasSelection && (selectedOption.optionCount > state.rowButtons.size())) {
                const std::size_t centerOffset = state.rowButtons.size() / 2u;
                if (selectedOption.flatIndex > centerOffset) {
                    firstVisibleIndex = selectedOption.flatIndex - centerOffset;
                }

                const std::size_t maxFirstVisibleIndex = selectedOption.optionCount - state.rowButtons.size();
                firstVisibleIndex = (std::min)(firstVisibleIndex, maxFirstVisibleIndex);
            }

            for (std::size_t rowIndex = 0u; rowIndex < state.rowButtons.size(); ++rowIndex) {
                std::string rowText = BuildEmptyButtonText();
                if (hasSelection) {
                    const std::size_t flatIndex = firstVisibleIndex + rowIndex;
                    if (flatIndex < selectedOption.optionCount) {
                        SelectedOptionRef rowOption = {};
                        if (TryGetOptionByFlatIndex(*state.catalog, flatIndex, rowOption)) {
                            rowText = BuildRowButtonText(rowOption);
                        }
                    }
                }

                updatedAllWidgets = TrySetButtonText(state, state.rowButtons[rowIndex], rowText) && updatedAllWidgets;
            }

            return updatedAllWidgets;
        }

        void ShowMenuOverlay(State& state, ScreenManagerHandle* screenManager) {
            if (screenManager == nullptr) {
                return;
            }

            if (state.overlayVisible) {
                return;
            }

            if ((state.activateScreen == nullptr) || (state.deactivateScreen == nullptr) || (state.setTopMost == nullptr)) {
                LogError("CoH Mod Config UI cannot activate: pointer-based ScreenManager helpers unavailable.");
                return;
            }

            if (!RefreshVisibleMenu(state)) {
                LogError("CoH Mod Config UI failed to build or refresh the overlay.");
                return;
            }

            state.setTopMost(screenManager, true);
            state.activateScreen(screenManager, state.screen, kDefaultScreenActivationType, false);
            state.overlayVisible = true;
            LogInfo("CoH Mod Config UI activated screen via pointer-based activation.");
        }

        void HideMenuOverlay(State& state, ScreenManagerHandle* screenManager) {
            if (!state.overlayVisible) {
                return;
            }

            if ((screenManager == nullptr) || (state.deactivateScreen == nullptr) || (state.setTopMost == nullptr) || (state.screen == nullptr)) {
                return;
            }

            LogInfo("CoH Mod Config UI is deactivating the screen.");
            state.deactivateScreen(screenManager, state.screen);
            state.setTopMost(screenManager, false);
            state.overlayVisible = false;
        }

        void ToggleMenuOverlay(State& state, ScreenManagerHandle* screenManager) {
            if (state.overlayVisible) {
                HideMenuOverlay(state, screenManager);
                return;
            }

            ShowMenuOverlay(state, screenManager);
        }

        bool AdjustSelectedValue(State& state, int direction) {
            if ((state.catalog == nullptr) || (direction == 0)) {
                return false;
            }

            SelectedOptionRef selectedOption = {};
            if (!TryGetSelectedOption(state, selectedOption) || (selectedOption.modEntry == nullptr) || (selectedOption.optionEntry == nullptr)) {
                return false;
            }

            OptionEntry& optionEntry = *selectedOption.optionEntry;
            ModSDK::Config::Value newValue = optionEntry.currentValue;

            switch (optionEntry.type) {
            case CoHModSDKConfigType_Bool:
                newValue = ModSDK::Config::MakeBoolValue(optionEntry.currentValue.boolValue == 0u);
                break;
            case CoHModSDKConfigType_Int: {
                const std::int32_t step = (std::max)(1, static_cast<std::int32_t>(std::lround(optionEntry.step > 0.0f ? optionEntry.step : 1.0f)));
                std::int32_t candidate = optionEntry.currentValue.intValue + (direction > 0 ? step : -step);
                if (optionEntry.minValue < optionEntry.maxValue) {
                    candidate = std::clamp(
                        candidate,
                        static_cast<std::int32_t>(std::lround(optionEntry.minValue)),
                        static_cast<std::int32_t>(std::lround(optionEntry.maxValue))
                    );
                }

                newValue = ModSDK::Config::MakeIntValue(candidate);
                break;
            }
            case CoHModSDKConfigType_Float: {
                const float step = optionEntry.step > 0.0f ? optionEntry.step : 0.1f;
                float candidate = optionEntry.currentValue.floatValue + (direction > 0 ? step : -step);
                if (optionEntry.minValue < optionEntry.maxValue) {
                    candidate = std::clamp(candidate, optionEntry.minValue, optionEntry.maxValue);
                }

                newValue = ModSDK::Config::MakeFloatValue(candidate);
                break;
            }
            case CoHModSDKConfigType_Enum:
                if (!optionEntry.choices.empty()) {
                    std::size_t choiceIndex = 0u;
                    for (std::size_t index = 0u; index < optionEntry.choices.size(); ++index) {
                        if (optionEntry.choices[index].value == optionEntry.currentValue.enumValue) {
                            choiceIndex = index;
                            break;
                        }
                    }

                    if (direction > 0) {
                        choiceIndex = (choiceIndex + 1u) % optionEntry.choices.size();
                    }
                    else {
                        choiceIndex = (choiceIndex == 0u) ? (optionEntry.choices.size() - 1u) : (choiceIndex - 1u);
                    }

                    newValue = ModSDK::Config::MakeEnumValue(optionEntry.choices[choiceIndex].value);
                }
                else {
                    newValue = ModSDK::Config::MakeEnumValue(optionEntry.currentValue.enumValue + (direction > 0 ? 1 : -1));
                }
                break;
            default:
                return false;
            }

            if (!ModSDK::Config::SetValue(selectedOption.modEntry->modId.c_str(), optionEntry.optionId.c_str(), newValue)) {
                LogWarning("CoH Mod Config UI failed to update config value for " + selectedOption.modEntry->modId + "." + optionEntry.optionId);
                return false;
            }

            optionEntry.currentValue = newValue;
            return true;
        }

        std::size_t ComputeFirstVisibleIndex(State& state) {
            SelectedOptionRef selectedOption = {};
            if (!TryGetSelectedOption(state, selectedOption) || (selectedOption.optionCount <= state.rowButtons.size())) {
                return 0u;
            }

            const std::size_t centerOffset = state.rowButtons.size() / 2u;
            std::size_t firstVisibleIndex = 0u;
            if (selectedOption.flatIndex > centerOffset) {
                firstVisibleIndex = selectedOption.flatIndex - centerOffset;
            }

            const std::size_t maxFirstVisibleIndex = selectedOption.optionCount - state.rowButtons.size();
            return (std::min)(firstVisibleIndex, maxFirstVisibleIndex);
        }

        void OnRowButtonClicked(const void* buttonProxy) {
            State& state = GetState();
            if (!state.overlayVisible || (state.catalog == nullptr)) {
                return;
            }

            std::size_t clickedRow = state.rowButtons.size();
            for (std::size_t i = 0u; i < state.rowButtons.size(); ++i) {
                if (state.rowButtons[i].Get() == buttonProxy) {
                    clickedRow = i;
                    break;
                }
            }

            if (clickedRow >= state.rowButtons.size()) {
                return;
            }

            const std::size_t firstVisible = ComputeFirstVisibleIndex(state);
            const std::size_t flatIndex = firstVisible + clickedRow;
            const std::size_t optionCount = GetOptionCount(*state.catalog);
            if (flatIndex >= optionCount) {
                return;
            }

            state.selectedOptionIndex = flatIndex;
            AdjustSelectedValue(state, 1);
            RefreshVisibleMenu(state);
        }

        void HandleUiTick(State& state) {
            if (state.catalog == nullptr) {
                return;
            }

            if (!state.updateHookObserved) {
                LogInfo("CoH Mod Config UI observed the first ScreenManager::Update tick.");
                state.updateHookObserved = true;
            }

            ScreenManagerHandle* screenManager = GetScreenManager(state);
            if (IsEdgePressed(state, state.toggleKey, state.toggleKeyWasDown)) {
                if (!state.toggleInputObserved) {
                    LogInfo("CoH Mod Config UI detected the first F10 toggle input.");
                    state.toggleInputObserved = true;
                }

                if (screenManager == nullptr) {
                    LogWarning("CoH Mod Config UI received toggle input, but the ScreenManager is unavailable.");
                    return;
                }

                ToggleMenuOverlay(state, screenManager);
                return;
            }

            if (state.overlayVisible && (state.widgetGetState != nullptr) && (state.widgetProxyGetWidget != nullptr)) {
                for (std::size_t i = 0u; i < state.rowButtons.size(); ++i) {
                    void* rawWidget = state.widgetProxyGetWidget(state.rowButtons[i].Get());
                    if (rawWidget == nullptr) {
                        continue;
                    }

                    const bool isActive = state.widgetGetState(rawWidget, kWidgetStateActive);
                    if (!isActive && state.rowButtonWasActive[i]) {
                        OnRowButtonClicked(state.rowButtons[i].Get());
                    }
                    state.rowButtonWasActive[i] = isActive;
                }
            }
        }

        void __fastcall HookedScreenManagerUpdate(ScreenManagerHandle* screenManager, void*, float deltaTime) {
            State& state = GetState();

            if (state.originalScreenManagerUpdate != nullptr) {
                state.originalScreenManagerUpdate(screenManager, deltaTime);
            }

            HandleUiTick(state);
        }
    }

    bool Install(Catalog* catalog) {
        if (catalog == nullptr) {
            return false;
        }

        State& state = GetState();
        if (state.installed) {
            state.catalog = catalog;
            return true;
        }

        state.catalog = catalog;
        state.screen = nullptr;
        state.overlayBuilt = false;
        state.overlayVisible = false;
        state.selectedOptionIndex = 0u;
        state.updateHookObserved = false;
        state.toggleKeyWasDown = false;
        state.toggleInputObserved = false;
        state.templateScreen = nullptr;
        ResetOverlayHandles(state);

        if (!ResolveInterop(state)) {
            LogError("CoH Mod Config UI failed to resolve the required frontend exports.");
            return false;
        }

        state.toggleKey = state.getKeyFromName == nullptr ? 0 : state.getKeyFromName(kToggleKeyName);
        if (state.toggleKey == 0) {
            LogError("CoH Mod Config UI failed to resolve the F10 input key.");
            return false;
        }


        if (!ModSDK::Hooks::CreateHook(
                state.screenManagerUpdateTarget,
                reinterpret_cast<void*>(&HookedScreenManagerUpdate),
                reinterpret_cast<void**>(&state.originalScreenManagerUpdate))) {
            LogError("CoH Mod Config UI failed to create the ScreenManager::Update hook.");
            return false;
        }

        if (!ModSDK::Hooks::EnableHook(state.screenManagerUpdateTarget)) {
            LogError("CoH Mod Config UI failed to enable the ScreenManager::Update hook.");
            return false;
        }

        state.installed = true;
        LogInfo("CoH Mod Config UI installed. F10 toggles the menu.");

        const std::string detailedSummary = BuildDetailedLogSummary(*catalog);
        if (!detailedSummary.empty()) {
            LogInfo("CoH Mod Config UI catalog snapshot: " + detailedSummary);
        }

        return true;
    }

    void Shutdown() {
        State& state = GetState();
        if (!state.installed) {
            state.catalog = nullptr;
            return;
        }

        // Disable the tick hook first to prevent further callbacks.
        if (state.screenManagerUpdateTarget != nullptr) {
            ModSDK::Hooks::DisableHook(state.screenManagerUpdateTarget);
        }

        // Unload our screens from the ScreenManager so the engine doesn't try to
        // iterate/destroy our widget tree during its own shutdown. The widgets are
        // backed by opaque byte arrays in static storage, and the engine walking
        // into them after we're gone causes access violations.
        //
        // We intentionally skip widget proxy dtors — those may call into engine
        // internals that are partially torn down. Unloading the screens is enough
        // to detach our widget tree from the engine's ownership.
        ScreenManagerHandle* screenManager = GetScreenManager(state);
        if (screenManager != nullptr && state.unloadScreen != nullptr) {
            if (state.overlayVisible && state.deactivateScreen != nullptr && state.screen != nullptr) {
                state.deactivateScreen(screenManager, state.screen);
            }

            if (state.screen != nullptr) {
                state.unloadScreen(screenManager, state.screen);
                state.screen = nullptr;
            }

            if (state.templateScreen != nullptr) {
                state.unloadScreen(screenManager, state.templateScreen);
                state.templateScreen = nullptr;
            }
        }

        state.catalog = nullptr;
        state.installed = false;
        state.overlayBuilt = false;
        state.overlayVisible = false;
    }
}
