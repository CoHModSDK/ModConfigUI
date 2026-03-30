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
        constexpr char kFilesystemModuleName[] = "Filesystem.dll";

        // Filesystem.dll exports for registering our .screen file override.
        // FilePathHD::Create(wchar_t const*, StreamMode) -> Source*   (__stdcall)
        constexpr char kFilePathHDCreateExport[] = "?Create@FilePathHD@@SGPAV1@PB_WW4StreamMode@@@Z";
        // FilePath::AddAlias(char const*, char const*, long, Source*) -> bool  (__stdcall)
        constexpr char kFilePathAddAliasExport[] = "?AddAlias@FilePath@@SG_NPBD0JPAVSource@1@@Z";
        constexpr long kFileOverridePriority = 100;
        constexpr int kStreamModeRead = 1;
        constexpr char kFileOverrideAlias[] = "DATA";
        constexpr char kFileOverrideSubPath[] = "";
        constexpr wchar_t kScreenFileRelativeDir[] = L"mods\\cohmodconfigui_data\\";
        constexpr char kScreenFileName[] = "cohmodconfigui";

        using FilePathHDCreateFn = void* (__stdcall*)(const wchar_t* path, int streamMode);
        using FilePathAddAliasFn = bool(__stdcall*)(const char* alias, const char* subPath, long priority, void* source);
        constexpr char kToggleKeyName[] = "F10";
        constexpr std::uintptr_t kCreateBlankScreenRva = 0x00036360u;
        constexpr std::size_t kOpaqueGenericWidgetStorageSize = 16384u;
        constexpr std::size_t kOpaqueButtonStorageSize = 16384u;
        constexpr std::size_t kOpaqueTextLabelStorageSize = 8192u;
        constexpr std::size_t kOpaqueLocStringStorageSize = 256u;
        constexpr std::size_t kOpaqueCustomListBoxStorageSize = 16384u;
        constexpr std::size_t kOpaqueCustomListBoxItemOldStorageSize = 16384u;
        constexpr std::size_t kVisibleRowCount = 5u;
        constexpr char kGroupWidgetTypeName[] = "Group";
        constexpr char kComboBoxWidgetTypeName[] = "ComboBox";
        constexpr char kCheckButtonWidgetTypeName[] = "CheckButton";
        constexpr char kProgressBarWidgetTypeName[] = "ProgressBar";
        constexpr char kTextLabelWidgetTypeName[] = "TextLabel";
        constexpr char kScreenName[] = "cohmodconfigui";
        constexpr char kTemplateScreenName[] = "prompt_performance_test";
        constexpr char kTemplatePanelWidgetName[] = "perfGrp";
        constexpr char kTemplateLabelWidgetName[] = "minimumResults";
        constexpr char kRootWidgetName[] = "cohmodconfigui_root";
        constexpr char kPanelButtonName[] = "cohmodconfigui_panel";
        constexpr char kTitleLabelName[] = "cohmodconfigui_title";
        constexpr char kSummaryLabelName[] = "cohmodconfigui_summary";
        constexpr char kFooterLabelName[] = "cohmodconfigui_footer";
        constexpr char kRowLabelNamePrefix[] = "cohmodconfigui_rowlabel_";
        constexpr char kRowButtonNamePrefix[] = "cohmodconfigui_row_";
        constexpr char kRowCheckButtonNamePrefix[] = "cohmodconfigui_rowcheck_";
        constexpr char kRowProgressBarNamePrefix[] = "cohmodconfigui_rowprog_";
        // Keep the options-menu radio-button donor recorded for later investigation.
        constexpr char kReferenceRadioButtonDonorScreenName[] = "optionsmenu";
        constexpr char kReferenceRadioButtonDonorWidgetName[] = "rdo_graphics_custom";
        constexpr char kCheckButtonDonorScreenName[] = "messageboxpopup2";
        constexpr char kCheckButtonDonorWidgetName[] = "checkbutton_ShowOnce";
        // Keep the skirmish ready donor recorded as a fallback reference.
        constexpr char kReferenceCheckButtonDonorScreenName[] = "skirmishmissionsetup";
        constexpr char kReferenceCheckButtonDonorWidgetName[] = "btnReady";
        constexpr char kDefaultStyleSetName[] = "DefaultStyles";
        constexpr char kDefaultCheckButtonStyleName[] = "defStyleCheckBox";
        constexpr char kProgressBarDonorScreenName[] = "optionsmenu";
        constexpr char kProgressBarDonorWidgetName[] = "progress_memory_used";
        constexpr char kOptionsmenuDonorScreenName[] = "optionsmenu";
        constexpr char kDropdownDonorWidgetName[] = "drop_enviromental_reverb";
        constexpr char kDropdownButtonDonorWidgetName[] = "btn_drop_enviromental_reverb";
        constexpr char kDropdownLabelDonorWidgetName[] = "lbl_drop_enviromental_reverb";
        constexpr char kDropdownListBoxDonorWidgetName[] = "lstBox_drop_enviromental_reverb";
        constexpr char kDropdownListBoxItemsDonorWidgetName[] = "items_lstBox_drop_enviromental_reverb";
        constexpr char kDropdownListBoxScrollBarDonorWidgetName[] = "scrlBar_lstBox_drop_enviromental_reverb";
        constexpr char kDropdownListBoxScrollBarDecDonorWidgetName[] = "btnDec_scrlBar_lstBox_drop_enviromental_reverb";
        constexpr char kDropdownListBoxScrollBarIncDonorWidgetName[] = "btnInc_scrlBar_lstBox_drop_enviromental_reverb";
        constexpr char kDropdownListBoxScrollBarTrackDonorWidgetName[] = "btnTrk_scrlBar_lstBox_drop_enviromental_reverb";
        constexpr char kDropdownListBoxScrollBarPageDownDonorWidgetName[] = "btnPgDn_scrlBar_lstBox_drop_enviromental_reverb";
        constexpr char kDropdownListBoxScrollBarPageUpDonorWidgetName[] = "btnPgUp_scrlBar_lstBox_drop_enviromental_reverb";
        constexpr char kDropdownListBoxItemTemplateDonorWidgetName[] = "itemTmplt_lstBox_drop_enviromental_reverb";
        constexpr std::uintptr_t kWidgetFactoryCreateRva = 0x000421A0u;
        constexpr std::uintptr_t kFindWidgetExtensionRva = 0x00031810u;
        constexpr std::uintptr_t kAddRenderChildRva = 0x0003DBF0u;
        constexpr std::uintptr_t kFindWidgetByNameRva = 0x0003BAB0u;
        constexpr int kRenderExtensionId = 6;
        constexpr int kDrawChildrenExtensionId = 1;
        constexpr int kProgressChildExtensionId = 14;
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
        constexpr float kTitlePositionY = 0.03f;
        constexpr float kTitleSizeX = 0.90f;
        constexpr float kTitleSizeY = 0.06f;
        constexpr float kSummaryPositionX = 0.05f;
        constexpr float kSummaryPositionY = 0.095f;
        constexpr float kSummarySizeX = 0.90f;
        constexpr float kSummarySizeY = 0.06f;
        constexpr float kFooterPositionX = 0.05f;
        constexpr float kFooterPositionY = 0.57f;
        constexpr float kFooterSizeX = 0.90f;
        constexpr float kFooterSizeY = 0.04f;
        constexpr float kFirstRowPositionX = 0.03f;
        constexpr float kFirstRowPositionY = 0.22f;
        constexpr float kRowSpacingY = 0.06f;
        constexpr float kRowLabelSizeX = 0.18f;
        constexpr float kRowLabelSizeY = 0.035f;
        constexpr float kRowControlPositionX = 0.22f;
        constexpr float kRowValueLabelSizeX = 0.12f;
        constexpr float kRowValueLabelSizeY = 0.035f;
        constexpr float kRowArrowButtonOffsetX = 0.12f;
        constexpr float kRowArrowButtonSizeX = 0.025f;
        constexpr float kRowArrowButtonSizeY = 0.035f;
        constexpr float kRowComboBoxSizeX = kRowArrowButtonOffsetX + kRowArrowButtonSizeX;
        constexpr float kRowComboBoxSizeY = kRowValueLabelSizeY;
        constexpr float kRowListBoxPositionX = 0.0f;
        constexpr float kRowListBoxPositionY = kRowComboBoxSizeY;
        constexpr float kRowListBoxSizeX = kRowComboBoxSizeX;
        constexpr float kRowListBoxSizeY = kRowComboBoxSizeY * 4.0f;
        constexpr float kRowListBoxScrollBarSizeX = 0.015f;
        constexpr float kRowListBoxContentSizeX = kRowListBoxSizeX - kRowListBoxScrollBarSizeX;
        constexpr float kDropdownItemLabelPositionX = 0.02f;
        constexpr float kDropdownItemLabelPositionY = 0.0f;
        constexpr float kDropdownItemLabelSizeX = 0.96f;
        constexpr float kDropdownItemLabelSizeY = 1.0f;
        constexpr float kReferenceRadioButtonSizeX = 0.21809f;
        constexpr float kReferenceRadioButtonSizeY = 0.05085f;
        constexpr float kRowCheckButtonSizeX = 0.18750f;
        constexpr float kRowCheckButtonSizeY = 0.04167f;
        constexpr float kRowProgressBarSizeX = 0.18f;
        constexpr float kRowProgressBarSizeY = 0.030f;

        using ScreenManagerHandle = void;
        using StyleManagerHandle = void;

        constexpr std::uintptr_t kFindStyleSetRva = 0x00047950u;
        constexpr std::uintptr_t kFindStyleInSetRva = 0x0003caf0u;

        using GetScreenManagerFn = ScreenManagerHandle * (__stdcall*)();
        using GetStyleManagerFn = StyleManagerHandle * (__thiscall*)(ScreenManagerHandle* manager);
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
        using CheckButtonCtorFn = void(__thiscall*)(void* checkButton);
        using CheckButtonDtorFn = void(__thiscall*)(void* checkButton);
        using CheckButtonSetCheckedFn = void(__thiscall*)(void* checkButton, bool checked);
        using CheckButtonGetCheckedFn = bool(__thiscall*)(const void* checkButton);
        using CheckButtonSetTextFn = void(__thiscall*)(void* checkButton, const void* locString);
        using ProgressBarCtorFn = void(__thiscall*)(void* progressBar);
        using ProgressBarDtorFn = void(__thiscall*)(void* progressBar);
        using ProgressBarSetProgressFn = void(__thiscall*)(void* progressBar, float progress);
        using ProgressBarSetRangeFn = void(__thiscall*)(void* progressBar, float min, float max);
        using ProgressBarGetProgressFn = float(__thiscall*)(const void* progressBar);
        using ProgressBarSetTextureFn = void(__thiscall*)(void* progressBar, const char* texturePath);
        using ProgressBarSetProgressBarTypeFn = void(__thiscall*)(void* progressBar, bool type);
        using ProgressBarSetStepSizeFn = void(__thiscall*)(void* progressBar, float stepSize);
        using ProgressBarIncrementFn = void(__thiscall*)(void* progressBar, long delta);

        struct MathPrimColour {
            std::uint8_t r;
            std::uint8_t g;
            std::uint8_t b;
            std::uint8_t a;
        };
        using ProgressBarSetProgressColourFn = void(__thiscall*)(void* progressBar, const MathPrimColour& colour);
        using TextLabelCtorFn = void(__thiscall*)(void* textLabel);
        using TextLabelDtorFn = void(__thiscall*)(void* textLabel);
        using TextLabelSetTextFn = void(__thiscall*)(void* textLabel, const void* locString);
        using TextLabelSetMultilineFn = void(__thiscall*)(void* textLabel, bool multiline);
        using TextLabelSetAutoSizeFn = void(__thiscall*)(void* textLabel, bool autoSize);
        using CustomListBoxCtorFn = void(__thiscall*)(void* customListBox);
        using CustomListBoxDtorFn = void(__thiscall*)(void* customListBox);
        using CustomListBoxAddItemFn = long(__thiscall*)(void* customListBox, const char* itemName, bool enabled);
        using CustomListBoxDeleteAllItemsFn = void(__thiscall*)(void* customListBox);
        using CustomListBoxSelectItemFn = bool(__thiscall*)(void* customListBox, long itemIndex);
        using CustomListBoxGetSelectedIndexFn = long(__thiscall*)(const void* customListBox);
        using CustomListBoxScrollToTopFn = void(__thiscall*)(void* customListBox);
        using CustomListBoxGetOldCustomItemFn = void* (__thiscall*)(void* customListBox);
        using CustomListBoxItemOldBindFn = void(__thiscall*)(void* customListBoxItemOld, const void* listBoxProxy, const char* itemName, long itemIndex);
        using CustomListBoxItemOldSetTextFn = void(__thiscall*)(void* customListBoxItemOld, const void* locString);
        using FindWidgetExtensionFn = void* (__thiscall*)(void* widget, int extensionId);
        using FindWidgetByNameFn = void* (__stdcall*)(void* rootWidget, const char* name, int flags);
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

        struct OpaqueCheckButton {
            alignas(16) std::array<std::byte, kOpaqueButtonStorageSize> storage = {};
            void* Get() { return storage.data(); }
            const void* Get() const { return storage.data(); }
        };

        struct OpaqueProgressBar {
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

        struct OpaqueCustomListBox {
            alignas(16) std::array<std::byte, kOpaqueCustomListBoxStorageSize> storage = {};
            void* Get() { return storage.data(); }
            const void* Get() const { return storage.data(); }
        };

        struct OpaqueCustomListBoxItemOld {
            alignas(16) std::array<std::byte, kOpaqueCustomListBoxItemOldStorageSize> storage = {};
            void* Get() { return storage.data(); }
            const void* Get() const { return storage.data(); }
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
            GetStyleManagerFn getStyleManager = nullptr;
            std::uintptr_t userInterfaceBase = 0u;
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
            CheckButtonCtorFn checkButtonCtor = nullptr;
            CheckButtonDtorFn checkButtonDtor = nullptr;
            CheckButtonSetCheckedFn checkButtonSetChecked = nullptr;
            CheckButtonGetCheckedFn checkButtonGetChecked = nullptr;
            CheckButtonSetTextFn checkButtonSetText = nullptr;
            ProgressBarCtorFn progressBarCtor = nullptr;
            ProgressBarDtorFn progressBarDtor = nullptr;
            ProgressBarSetProgressFn progressBarSetProgress = nullptr;
            ProgressBarSetRangeFn progressBarSetRange = nullptr;
            ProgressBarGetProgressFn progressBarGetProgress = nullptr;
            ProgressBarSetTextureFn progressBarSetTexture = nullptr;
            ProgressBarSetProgressBarTypeFn progressBarSetProgressBarType = nullptr;
            ProgressBarSetStepSizeFn progressBarSetStepSize = nullptr;
            ProgressBarIncrementFn progressBarIncrement = nullptr;
            ProgressBarSetProgressColourFn progressBarSetProgressColour = nullptr;
            TextLabelCtorFn textLabelCtor = nullptr;
            TextLabelDtorFn textLabelDtor = nullptr;
            TextLabelSetTextFn textLabelSetText = nullptr;
            TextLabelSetMultilineFn textLabelSetMultiline = nullptr;
            TextLabelSetAutoSizeFn textLabelSetAutoSize = nullptr;
            CustomListBoxCtorFn customListBoxCtor = nullptr;
            CustomListBoxDtorFn customListBoxDtor = nullptr;
            CustomListBoxAddItemFn customListBoxAddItem = nullptr;
            CustomListBoxDeleteAllItemsFn customListBoxDeleteAllItems = nullptr;
            CustomListBoxSelectItemFn customListBoxSelectItem = nullptr;
            CustomListBoxGetSelectedIndexFn customListBoxGetSelectedIndex = nullptr;
            CustomListBoxScrollToTopFn customListBoxScrollToTop = nullptr;
            CustomListBoxGetOldCustomItemFn customListBoxGetOldCustomItem = nullptr;
            CustomListBoxItemOldBindFn customListBoxItemOldBind = nullptr;
            CustomListBoxItemOldSetTextFn customListBoxItemOldSetText = nullptr;
            FindWidgetExtensionFn findWidgetExtension = nullptr;
            FindWidgetByNameFn findWidgetByName = nullptr;
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
            bool fileOverrideRegistered = false;
            bool updateHookObserved = false;
            bool toggleKeyWasDown = false;
            bool toggleInputObserved = false;
            void* screen = nullptr;
            void* rootWidgetRaw = nullptr;
            void* panelWidgetRaw = nullptr;
            void* titleLabelRaw = nullptr;
            void* summaryLabelRaw = nullptr;
            void* footerLabelRaw = nullptr;
            std::array<void*, kVisibleRowCount> rowLabelWidgets = {};
            // Enum widgets
            std::array<void*, kVisibleRowCount> rowComboBoxWidgets = {};
            std::array<void*, kVisibleRowCount> rowListBoxWidgets = {};
            std::array<void*, kVisibleRowCount> rowValueLabelWidgets = {};
            std::array<void*, kVisibleRowCount> rowArrowButtonWidgets = {};
            // Bool widgets
            std::array<void*, kVisibleRowCount> rowCheckButtonWidgets = {};
            // Int/Float widgets
            std::array<void*, kVisibleRowCount> rowProgressBarWidgets = {};
            OpaqueTextLabel titleLabel = {};
            OpaqueTextLabel summaryLabel = {};
            OpaqueTextLabel footerLabel = {};
            std::array<OpaqueTextLabel, kVisibleRowCount> rowLabels = {};
            // Enum proxies
            std::array<OpaqueTextLabel, kVisibleRowCount> rowValueLabels = {};
            std::array<OpaqueButton, kVisibleRowCount> rowArrowButtons = {};
            // Bool proxies
            std::array<OpaqueCheckButton, kVisibleRowCount> rowCheckButtons = {};
            // Int/Float proxies
            std::array<OpaqueProgressBar, kVisibleRowCount> rowProgressBars = {};
            // State tracking
            std::array<bool, kVisibleRowCount> rowArrowButtonWasActive = {};
            std::array<bool, kVisibleRowCount> rowCheckButtonWasActive = {};
            std::array<bool, kVisibleRowCount> rowProgressBarWasActive = {};
            std::array<long, kVisibleRowCount> rowObservedListBoxSelection = {};
            std::array<bool, kVisibleRowCount> rowHasObservedListBoxSelection = {};
            std::array<CoHModSDKConfigType, kVisibleRowCount> rowActiveControlType = {};
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

        bool RefreshVisibleMenu(State& state);
        std::size_t ComputeFirstVisibleIndex(State& state);

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

        bool TryGetVisibleRowOption(State& state, std::size_t rowIndex, SelectedOptionRef& outSelectedOption) {
            outSelectedOption = {};
            if ((state.catalog == nullptr) || (rowIndex >= kVisibleRowCount)) {
                return false;
            }

            const std::size_t firstVisibleIndex = ComputeFirstVisibleIndex(state);
            const std::size_t flatIndex = firstVisibleIndex + rowIndex;
            return TryGetOptionByFlatIndex(*state.catalog, flatIndex, outSelectedOption);
        }

        bool TryGetCurrentEnumChoiceIndex(const OptionEntry& optionEntry, long& outChoiceIndex) {
            outChoiceIndex = -1;
            if ((optionEntry.type != CoHModSDKConfigType_Enum) || optionEntry.choices.empty()) {
                return false;
            }

            for (std::size_t choiceIndex = 0u; choiceIndex < optionEntry.choices.size(); ++choiceIndex) {
                if (optionEntry.choices[choiceIndex].value == optionEntry.currentValue.enumValue) {
                    outChoiceIndex = static_cast<long>(choiceIndex);
                    return true;
                }
            }

            return false;
        }

        bool TryMapEnumChoiceIndexToNativeListIndex(const OptionEntry& optionEntry, long choiceIndex, long& outNativeIndex) {
            outNativeIndex = -1;
            if ((optionEntry.type != CoHModSDKConfigType_Enum) ||
                optionEntry.choices.empty() ||
                (choiceIndex < 0) ||
                (choiceIndex >= static_cast<long>(optionEntry.choices.size()))) {
                return false;
            }

            outNativeIndex = static_cast<long>(optionEntry.choices.size() - 1u) - choiceIndex;
            return true;
        }

        bool TryMapNativeListIndexToEnumChoiceIndex(const OptionEntry& optionEntry, long nativeIndex, long& outChoiceIndex) {
            outChoiceIndex = -1;
            if ((optionEntry.type != CoHModSDKConfigType_Enum) ||
                optionEntry.choices.empty() ||
                (nativeIndex < 0) ||
                (nativeIndex >= static_cast<long>(optionEntry.choices.size()))) {
                return false;
            }

            outChoiceIndex = static_cast<long>(optionEntry.choices.size() - 1u) - nativeIndex;
            return true;
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
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??0CheckButton@UI@@QAE@XZ", state.checkButtonCtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??1CheckButton@UI@@UAE@XZ", state.checkButtonDtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetChecked@CheckButton@UI@@QAEX_N@Z", state.checkButtonSetChecked) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?GetChecked@CheckButton@UI@@QBE_NXZ", state.checkButtonGetChecked) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??0ProgressBar@UI@@QAE@XZ", state.progressBarCtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??1ProgressBar@UI@@UAE@XZ", state.progressBarDtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetProgress@ProgressBar@UI@@QAEXM@Z", state.progressBarSetProgress) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetRange@ProgressBar@UI@@QAEXMM@Z", state.progressBarSetRange) &&
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
            state.userInterfaceBase = userInterfaceBase;
            state.createBlankScreen = reinterpret_cast<CreateBlankScreenFn>(userInterfaceBase + kCreateBlankScreenRva);
            state.findWidgetExtension = reinterpret_cast<FindWidgetExtensionFn>(userInterfaceBase + kFindWidgetExtensionRva);
            state.findWidgetByName = reinterpret_cast<FindWidgetByNameFn>(userInterfaceBase + kFindWidgetByNameRva);
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
            ResolveOptionalExport(userInterfaceModule, "?SetText@CheckButton@UI@@QAEXABVLocString@@@Z", state.checkButtonSetText);
            ResolveOptionalExport(userInterfaceModule, "??0CustomListBox@UI@@QAE@XZ", state.customListBoxCtor);
            ResolveOptionalExport(userInterfaceModule, "??1CustomListBox@UI@@UAE@XZ", state.customListBoxDtor);
            ResolveOptionalExport(userInterfaceModule, "?AddItem@CustomListBox@UI@@QAEJPBD_N@Z", state.customListBoxAddItem);
            ResolveOptionalExport(userInterfaceModule, "?DeleteAllItems@CustomListBox@UI@@QAEXXZ", state.customListBoxDeleteAllItems);
            ResolveOptionalExport(userInterfaceModule, "?SelectItem@CustomListBox@UI@@QAE_NJ@Z", state.customListBoxSelectItem);
            ResolveOptionalExport(userInterfaceModule, "?GetSelectedIndex@CustomListBox@UI@@QBEJXZ", state.customListBoxGetSelectedIndex);
            ResolveOptionalExport(userInterfaceModule, "?ScrollToTop@CustomListBox@UI@@QAEXXZ", state.customListBoxScrollToTop);
            ResolveOptionalExport(userInterfaceModule, "?GetOldCustomItem@CustomListBox@UI@@QAEPAVCustomListBoxItemOld@2@XZ", state.customListBoxGetOldCustomItem);
            ResolveOptionalExport(userInterfaceModule, "?Bind@CustomListBoxItemOld@UI@@QAEXABVWidgetProxy@2@PBDJ@Z", state.customListBoxItemOldBind);
            ResolveOptionalExport(userInterfaceModule, "?SetText@CustomListBoxItemOld@UI@@QAEXABVLocString@@@Z", state.customListBoxItemOldSetText);
            ResolveOptionalExport(userInterfaceModule, "?GetProgress@ProgressBar@UI@@QBEMXZ", state.progressBarGetProgress);
            ResolveOptionalExport(userInterfaceModule, "?GetStyleManager@ScreenManager@UI@@QAEPAVStyleManager@2@XZ", state.getStyleManager);
            ResolveOptionalExport(userInterfaceModule, "?SetTexture@ProgressBar@UI@@QAEXPBD@Z", state.progressBarSetTexture);
            ResolveOptionalExport(userInterfaceModule, "?SetProgressBarType@ProgressBar@UI@@QAEX_N@Z", state.progressBarSetProgressBarType);
            ResolveOptionalExport(userInterfaceModule, "?SetStepSize@ProgressBar@UI@@QAEXM@Z", state.progressBarSetStepSize);
            ResolveOptionalExport(userInterfaceModule, "?Increment@ProgressBar@UI@@QAEXJ@Z", state.progressBarIncrement);
            ResolveOptionalExport(userInterfaceModule, "?SetProgressColour@ProgressBar@UI@@QAEXABVColour@MathPrim@@@Z", state.progressBarSetProgressColour);
            return true;
        }

        bool RegisterFileOverride(State& state) {
            if (state.fileOverrideRegistered) {
                return true;
            }

            HMODULE filesystemModule = AcquireModule(kFilesystemModuleName);
            if (filesystemModule == nullptr) {
                LogError("CoH Mod Config UI could not load Filesystem.dll for file override.");
                return false;
            }

            FilePathHDCreateFn filePathHDCreate = nullptr;
            FilePathAddAliasFn filePathAddAlias = nullptr;
            if (!ResolveRequiredExport(filesystemModule, kFilesystemModuleName, kFilePathHDCreateExport, filePathHDCreate) ||
                !ResolveRequiredExport(filesystemModule, kFilesystemModuleName, kFilePathAddAliasExport, filePathAddAlias)) {
                LogError("CoH Mod Config UI could not resolve Filesystem.dll exports for file override.");
                return false;
            }

            // Build absolute path to our data directory next to the game executable.
            wchar_t gameDir[MAX_PATH] = {};
            const DWORD len = GetModuleFileNameW(nullptr, gameDir, MAX_PATH);
            if ((len == 0u) || (len >= MAX_PATH)) {
                LogError("CoH Mod Config UI could not determine the game directory.");
                return false;
            }

            // Strip executable name to get directory.
            wchar_t* lastSlash = wcsrchr(gameDir, L'\\');
            if (lastSlash != nullptr) {
                *(lastSlash + 1) = L'\0';
            }

            std::wstring dataDir = std::wstring(gameDir) + kScreenFileRelativeDir;
            {
                std::string narrowDir(dataDir.size(), '\0');
                for (size_t i = 0; i < dataDir.size(); ++i) narrowDir[i] = static_cast<char>(dataDir[i]);
                LogInfo("CoH Mod Config UI registering file override at: " + narrowDir);
            }

            void* source = filePathHDCreate(dataDir.c_str(), kStreamModeRead);
            if (source == nullptr) {
                LogError("CoH Mod Config UI failed to create FilePathHD source for override directory.");
                return false;
            }

            // Patch FilePathHD::FileOpen to accept forward slashes in file paths.
            // The engine's screen loader formats paths as "Screens/%s.screen" with a
            // forward slash, but FilePathHD::FileOpen explicitly rejects paths containing
            // '/'.  We NOP the jne that triggers the rejection (offset +0x4E from the
            // function entry, verified as opcode 0x75).  Windows APIs accept '/' in
            // paths, so this is safe.
            {
                void** vtable = *reinterpret_cast<void***>(source);
                constexpr int kFileOpenVtableIndex = 4;
                constexpr int kForwardSlashJneOffset = 0x4E;
                auto* fileOpenFn = reinterpret_cast<unsigned char*>(vtable[kFileOpenVtableIndex]);
                if (fileOpenFn[kForwardSlashJneOffset] == 0x75) {
                    DWORD oldProtect = 0;
                    if (VirtualProtect(fileOpenFn + kForwardSlashJneOffset, 2, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                        fileOpenFn[kForwardSlashJneOffset] = 0x90;
                        fileOpenFn[kForwardSlashJneOffset + 1] = 0x90;
                        VirtualProtect(fileOpenFn + kForwardSlashJneOffset, 2, oldProtect, &oldProtect);
                        LogInfo("CoH Mod Config UI patched FilePathHD forward-slash rejection.");
                    }
                }
            }

            const bool added = filePathAddAlias(kFileOverrideAlias, kFileOverrideSubPath, kFileOverridePriority, source);
            if (!added) {
                LogError("CoH Mod Config UI failed to register file override alias.");
                return false;
            }

            state.fileOverrideRegistered = true;
            LogInfo("CoH Mod Config UI registered DATA: file override with priority " + std::to_string(kFileOverridePriority) + ".");
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

        std::string BuildEmptyButtonText() {
            return " ";
        }

        std::string MakeRowLabelName(std::size_t rowIndex) {
            return std::string(kRowLabelNamePrefix) + std::to_string(rowIndex);
        }

        std::string MakeRowButtonName(std::size_t rowIndex) {
            return std::string(kRowButtonNamePrefix) + std::to_string(rowIndex);
        }

        std::string MakeRowComboBoxName(std::size_t rowIndex) {
            return std::string("drop_") + MakeRowButtonName(rowIndex);
        }

        std::string MakeRowComboBoxLabelName(std::size_t rowIndex) {
            return std::string("lbl_") + MakeRowComboBoxName(rowIndex);
        }

        std::string MakeRowComboBoxButtonName(std::size_t rowIndex) {
            return std::string("btn_") + MakeRowComboBoxName(rowIndex);
        }

        std::string MakeRowComboBoxListBoxName(std::size_t rowIndex) {
            return std::string("lstBox_") + MakeRowComboBoxName(rowIndex);
        }

        std::string MakeRowComboBoxListBoxItemsName(std::size_t rowIndex) {
            return std::string("items_") + MakeRowComboBoxListBoxName(rowIndex);
        }

        std::string MakeRowComboBoxListBoxScrollBarName(std::size_t rowIndex) {
            return std::string("scrlBar_") + MakeRowComboBoxListBoxName(rowIndex);
        }

        std::string MakeRowComboBoxListBoxScrollBarDecName(std::size_t rowIndex) {
            return std::string("btnDec_") + MakeRowComboBoxListBoxScrollBarName(rowIndex);
        }

        std::string MakeRowComboBoxListBoxScrollBarIncName(std::size_t rowIndex) {
            return std::string("btnInc_") + MakeRowComboBoxListBoxScrollBarName(rowIndex);
        }

        std::string MakeRowComboBoxListBoxScrollBarTrackName(std::size_t rowIndex) {
            return std::string("btnTrk_") + MakeRowComboBoxListBoxScrollBarName(rowIndex);
        }

        std::string MakeRowComboBoxListBoxScrollBarPageDownName(std::size_t rowIndex) {
            return std::string("btnPgDn_") + MakeRowComboBoxListBoxScrollBarName(rowIndex);
        }

        std::string MakeRowComboBoxListBoxScrollBarPageUpName(std::size_t rowIndex) {
            return std::string("btnPgUp_") + MakeRowComboBoxListBoxScrollBarName(rowIndex);
        }

        std::string MakeRowComboBoxListBoxItemTemplateName(std::size_t rowIndex) {
            return std::string("itemTmplt_") + MakeRowComboBoxListBoxName(rowIndex);
        }

        std::string MakeRowComboBoxListItemName(std::size_t rowIndex, std::size_t choiceIndex) {
            return std::string("cohmodconfigui_rowitem_") + std::to_string(rowIndex) + "_" + std::to_string(choiceIndex);
        }

        std::string MakeRowComboBoxListItemLabelName(std::size_t rowIndex, std::size_t choiceIndex) {
            return std::string("lbl_") + MakeRowComboBoxListItemName(rowIndex, choiceIndex);
        }

        std::string MakeRowValueLabelName(std::size_t rowIndex) {
            return std::string("cohmodconfigui_rowval_") + std::to_string(rowIndex);
        }

        std::string MakeRowCheckButtonName(std::size_t rowIndex) {
            return std::string(kRowCheckButtonNamePrefix) + std::to_string(rowIndex);
        }

        std::string ReadWidgetNameForLog(const void* rawWidget) {
            if (rawWidget == nullptr) {
                return "<null>";
            }

            const char* nameAt4 = reinterpret_cast<const char*>(reinterpret_cast<std::uintptr_t>(rawWidget) + 4u);
            char nameBuf[65] = {};
            for (int i = 0; i < 64; ++i) {
                const char c = nameAt4[i];
                if (c == '\0') {
                    break;
                }

                nameBuf[i] = (c >= 0x20 && c <= 0x7E) ? c : '?';
            }

            return std::string(nameBuf);
        }

        void ResetOverlayHandles(State& state) {
            state.rootWidgetRaw = nullptr;
            state.panelWidgetRaw = nullptr;
            state.titleLabelRaw = nullptr;
            state.summaryLabelRaw = nullptr;
            state.footerLabelRaw = nullptr;
            state.rowLabelWidgets.fill(nullptr);
            state.rowComboBoxWidgets.fill(nullptr);
            state.rowListBoxWidgets.fill(nullptr);
            state.rowValueLabelWidgets.fill(nullptr);
            state.rowArrowButtonWidgets.fill(nullptr);
            state.rowCheckButtonWidgets.fill(nullptr);
            state.rowProgressBarWidgets.fill(nullptr);
            state.rowObservedListBoxSelection.fill(-1);
            state.rowHasObservedListBoxSelection.fill(false);
            state.rowActiveControlType.fill(CoHModSDKConfigType_Bool);
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

        bool BindCheckButtonProxy(State& state, OpaqueCheckButton& checkButton, void* rawWidget) {
            if ((rawWidget == nullptr) || (state.checkButtonCtor == nullptr) || (state.widgetProxyBind == nullptr)) {
                return false;
            }

            state.checkButtonCtor(checkButton.Get());
            state.widgetProxyBind(checkButton.Get(), rawWidget);
            return true;
        }

        bool BindProgressBarProxy(State& state, OpaqueProgressBar& progressBar, void* rawWidget) {
            if ((rawWidget == nullptr) || (state.progressBarCtor == nullptr) || (state.widgetProxyBind == nullptr)) {
                return false;
            }

            state.progressBarCtor(progressBar.Get());
            state.widgetProxyBind(progressBar.Get(), rawWidget);
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

        bool SetRawWidgetVisible(State& state, void* rawWidget, bool visible) {
            if ((rawWidget == nullptr) ||
                (state.genericWidgetCtor == nullptr) ||
                (state.genericWidgetDtor == nullptr) ||
                (state.widgetProxyBind == nullptr) ||
                (state.widgetProxySetVisible == nullptr)) {
                return false;
            }

            OpaqueGenericWidget widgetProxy = {};
            state.genericWidgetCtor(widgetProxy.Get());
            state.widgetProxyBind(widgetProxy.Get(), rawWidget);
            state.widgetProxySetVisible(widgetProxy.Get(), visible);
            state.genericWidgetDtor(widgetProxy.Get());
            return true;
        }

        void* FindWidgetExtensionObject(State& state, void* widget, int extensionId) {
            if ((widget == nullptr) || (state.findWidgetExtension == nullptr) || (extensionId == 0)) {
                return nullptr;
            }

            return state.findWidgetExtension(widget, extensionId);
        }

        void* FindNamedWidget(State& state, void* searchRoot, const char* widgetName) {
            if ((searchRoot == nullptr) || (widgetName == nullptr) || (state.findWidgetByName == nullptr)) {
                return nullptr;
            }

            void* widget = state.findWidgetByName(searchRoot, widgetName, 0);
            if (widget == nullptr) {
                widget = state.findWidgetByName(searchRoot, widgetName, 1);
            }

            return widget;
        }

        bool ResolveComboBoxChildWidgets(
            State& state,
            void* comboBoxWidget,
            const std::string& comboBoxName,
            void*& outLabelWidget,
            void*& outButtonWidget,
            void*& outListBoxWidget
        ) {
            outLabelWidget = nullptr;
            outButtonWidget = nullptr;
            outListBoxWidget = nullptr;

            const std::string labelName = std::string("lbl_") + comboBoxName;
            const std::string buttonName = std::string("btn_") + comboBoxName;
            const std::string listBoxName = std::string("lstBox_") + comboBoxName;

            outLabelWidget = FindNamedWidget(state, comboBoxWidget, labelName.c_str());
            outButtonWidget = FindNamedWidget(state, comboBoxWidget, buttonName.c_str());
            outListBoxWidget = FindNamedWidget(state, comboBoxWidget, listBoxName.c_str());

            if (((outLabelWidget == nullptr) || (outButtonWidget == nullptr) || (outListBoxWidget == nullptr)) &&
                (state.rootWidgetRaw != nullptr) &&
                (state.rootWidgetRaw != comboBoxWidget)) {
                if (outLabelWidget == nullptr) {
                    outLabelWidget = FindNamedWidget(state, state.rootWidgetRaw, labelName.c_str());
                }
                if (outButtonWidget == nullptr) {
                    outButtonWidget = FindNamedWidget(state, state.rootWidgetRaw, buttonName.c_str());
                }
                if (outListBoxWidget == nullptr) {
                    outListBoxWidget = FindNamedWidget(state, state.rootWidgetRaw, listBoxName.c_str());
                }
            }

            return (outLabelWidget != nullptr) && (outButtonWidget != nullptr) && (outListBoxWidget != nullptr);
        }

        bool ResolveListBoxChildWidgets(
            State& state,
            void* listBoxWidget,
            const std::string& listBoxName,
            void*& outItemsWidget,
            void*& outScrollBarWidget,
            void*& outItemTemplateWidget
        ) {
            outItemsWidget = nullptr;
            outScrollBarWidget = nullptr;
            outItemTemplateWidget = nullptr;

            const std::string itemsName = std::string("items_") + listBoxName;
            const std::string scrollBarName = std::string("scrlBar_") + listBoxName;
            const std::string itemTemplateName = std::string("itemTmplt_") + listBoxName;

            outItemsWidget = FindNamedWidget(state, listBoxWidget, itemsName.c_str());
            outScrollBarWidget = FindNamedWidget(state, listBoxWidget, scrollBarName.c_str());
            outItemTemplateWidget = FindNamedWidget(state, listBoxWidget, itemTemplateName.c_str());

            if (((outItemsWidget == nullptr) || (outScrollBarWidget == nullptr) || (outItemTemplateWidget == nullptr)) &&
                (state.rootWidgetRaw != nullptr) &&
                (state.rootWidgetRaw != listBoxWidget)) {
                if (outItemsWidget == nullptr) {
                    outItemsWidget = FindNamedWidget(state, state.rootWidgetRaw, itemsName.c_str());
                }
                if (outScrollBarWidget == nullptr) {
                    outScrollBarWidget = FindNamedWidget(state, state.rootWidgetRaw, scrollBarName.c_str());
                }
                if (outItemTemplateWidget == nullptr) {
                    outItemTemplateWidget = FindNamedWidget(state, state.rootWidgetRaw, itemTemplateName.c_str());
                }
            }

            return (outItemsWidget != nullptr) && (outScrollBarWidget != nullptr) && (outItemTemplateWidget != nullptr);
        }

        bool ResolveScrollBarChildWidgets(
            State& state,
            void* scrollBarWidget,
            const std::string& scrollBarName,
            void*& outDecButtonWidget,
            void*& outIncButtonWidget,
            void*& outTrackButtonWidget,
            void*& outPageDownButtonWidget,
            void*& outPageUpButtonWidget
        ) {
            outDecButtonWidget = nullptr;
            outIncButtonWidget = nullptr;
            outTrackButtonWidget = nullptr;
            outPageDownButtonWidget = nullptr;
            outPageUpButtonWidget = nullptr;

            const std::string decName = std::string("btnDec_") + scrollBarName;
            const std::string incName = std::string("btnInc_") + scrollBarName;
            const std::string trackName = std::string("btnTrk_") + scrollBarName;
            const std::string pageDownName = std::string("btnPgDn_") + scrollBarName;
            const std::string pageUpName = std::string("btnPgUp_") + scrollBarName;

            outDecButtonWidget = FindNamedWidget(state, scrollBarWidget, decName.c_str());
            outIncButtonWidget = FindNamedWidget(state, scrollBarWidget, incName.c_str());
            outTrackButtonWidget = FindNamedWidget(state, scrollBarWidget, trackName.c_str());
            outPageDownButtonWidget = FindNamedWidget(state, scrollBarWidget, pageDownName.c_str());
            outPageUpButtonWidget = FindNamedWidget(state, scrollBarWidget, pageUpName.c_str());

            if (((outDecButtonWidget == nullptr) ||
                (outIncButtonWidget == nullptr) ||
                (outTrackButtonWidget == nullptr) ||
                (outPageDownButtonWidget == nullptr) ||
                (outPageUpButtonWidget == nullptr)) &&
                (state.rootWidgetRaw != nullptr) &&
                (state.rootWidgetRaw != scrollBarWidget)) {
                if (outDecButtonWidget == nullptr) {
                    outDecButtonWidget = FindNamedWidget(state, state.rootWidgetRaw, decName.c_str());
                }
                if (outIncButtonWidget == nullptr) {
                    outIncButtonWidget = FindNamedWidget(state, state.rootWidgetRaw, incName.c_str());
                }
                if (outTrackButtonWidget == nullptr) {
                    outTrackButtonWidget = FindNamedWidget(state, state.rootWidgetRaw, trackName.c_str());
                }
                if (outPageDownButtonWidget == nullptr) {
                    outPageDownButtonWidget = FindNamedWidget(state, state.rootWidgetRaw, pageDownName.c_str());
                }
                if (outPageUpButtonWidget == nullptr) {
                    outPageUpButtonWidget = FindNamedWidget(state, state.rootWidgetRaw, pageUpName.c_str());
                }
            }

            return (outDecButtonWidget != nullptr) &&
                (outIncButtonWidget != nullptr) &&
                (outTrackButtonWidget != nullptr) &&
                (outPageDownButtonWidget != nullptr) &&
                (outPageUpButtonWidget != nullptr);
        }

        std::string BuildChoiceDisplayText(const ChoiceEntry& choiceEntry) {
            if (!choiceEntry.label.empty()) {
                return choiceEntry.label;
            }

            if (!choiceEntry.valueId.empty()) {
                return choiceEntry.valueId;
            }

            return std::to_string(choiceEntry.value);
        }

        bool TransferDonorPresentationFrom(State& state, void* targetRawWidget, const char* donorScreenName, const char* donorWidgetName);
        bool AttachRenderChild(State& state, void* parentWidget, void* childWidget);
        bool TrySetTextLabel(State& state, OpaqueTextLabel& textLabel, const std::string& text, bool multiline);

        bool EnsureDropdownListItemLabel(
            State& state,
            void* itemWidget,
            std::size_t rowIndex,
            std::size_t choiceIndex,
            const std::string& displayText
        ) {
            constexpr int kTextLabelExtensionId = 7;

            if ((itemWidget == nullptr) || (state.textLabelCtor == nullptr) || (state.widgetProxyBind == nullptr)) {
                return false;
            }

            const std::string itemName = MakeRowComboBoxListItemName(rowIndex, choiceIndex);
            if (FindWidgetExtensionObject(state, itemWidget, kTextLabelExtensionId) == nullptr) {
                LogWarning(
                    "CoH Mod Config UI: List item '" +
                    itemName +
                    "' has no text extension for direct fallback text binding."
                );
                return false;
            }

            OpaqueTextLabel textLabel = {};
            if (!BindTextLabelProxy(state, textLabel, itemWidget)) {
                LogWarning(
                    "CoH Mod Config UI: Failed to bind direct fallback text proxy for list item '" +
                    itemName +
                    "'."
                );
                return false;
            }

            const bool textSet = TrySetTextLabel(state, textLabel, displayText, false);
            if (state.textLabelDtor != nullptr) {
                state.textLabelDtor(textLabel.Get());
            }

            if (textSet) {
                LogInfo(
                    "CoH Mod Config UI: Set direct fallback text on list item '" +
                    itemName +
                    "' to '" +
                    displayText +
                    "'."
                );
            } else {
                LogWarning(
                    "CoH Mod Config UI: Failed to set direct fallback text on list item '" +
                    itemName +
                    "'."
                );
            }

            return textSet;
        }

        bool TryResolveListItemTextSubItemIndex(
            State& state,
            void* itemWidget,
            std::size_t rowIndex,
            const std::string& itemName,
            long& outSubItemIndex
        ) {
            constexpr int kItemSubItemListExtensionId = 17;
            constexpr int kTextLabelExtensionId = 7;
            constexpr std::ptrdiff_t kExtensionHeaderAdjust = 0x14;
            constexpr std::ptrdiff_t kExtensionChildArrayOffset = 0x1C;
            constexpr std::ptrdiff_t kExtensionChildCountOffset = 0x20;
            constexpr long kMaxExpectedSubItemCount = 8;

            outSubItemIndex = 0;
            if (itemWidget == nullptr) {
                return false;
            }

            const void* const subItemListExtension = FindWidgetExtensionObject(state, itemWidget, kItemSubItemListExtensionId);
            if (subItemListExtension == nullptr) {
                LogWarning(
                    "CoH Mod Config UI: Row " + std::to_string(rowIndex) +
                    " list item '" + itemName +
                    "' has no extension 17 child list; falling back to subitem index 0."
                );
                return false;
            }

            const std::byte* const extensionBase =
                reinterpret_cast<const std::byte*>(subItemListExtension) - kExtensionHeaderAdjust;
            const void* const* const childWidgets =
                *reinterpret_cast<void* const* const*>(extensionBase + kExtensionChildArrayOffset);
            const long childCount = *reinterpret_cast<const long*>(extensionBase + kExtensionChildCountOffset);

            LogInfo(
                "CoH Mod Config UI: Row " + std::to_string(rowIndex) +
                " list item '" + itemName +
                "' exposes " + std::to_string(childCount) +
                " subitems via extension 17."
            );

            if ((childWidgets == nullptr) || (childCount <= 0) || (childCount > kMaxExpectedSubItemCount)) {
                LogWarning(
                    "CoH Mod Config UI: Row " + std::to_string(rowIndex) +
                    " list item '" + itemName +
                    "' reported an invalid subitem list; falling back to subitem index 0."
                );
                return false;
            }

            for (long childIndex = 0; childIndex < childCount; ++childIndex) {
                void* const childWidget = const_cast<void*>(childWidgets[childIndex]);
                const bool childHasTextExtension =
                    FindWidgetExtensionObject(state, childWidget, kTextLabelExtensionId) != nullptr;
                LogInfo(
                    "CoH Mod Config UI: Row " + std::to_string(rowIndex) +
                    " list item '" + itemName +
                    "' subitem[" + std::to_string(childIndex) +
                    "]='" + ReadWidgetNameForLog(childWidget) +
                    "', textExtension=" + (childHasTextExtension ? "yes" : "no") + "."
                );

                if (childHasTextExtension) {
                    outSubItemIndex = childIndex;
                    return true;
                }
            }

            LogWarning(
                "CoH Mod Config UI: Row " + std::to_string(rowIndex) +
                " list item '" + itemName +
                "' exposed no text-capable subitem; falling back to subitem index 0."
            );
            return false;
        }

        bool PopulateEnumListBox(State& state, void* listBoxWidget, const OptionEntry& optionEntry, std::size_t rowIndex) {
            if ((listBoxWidget == nullptr) ||
                (state.widgetProxyBind == nullptr) ||
                (state.locStringCtor == nullptr) ||
                (state.locStringDtor == nullptr) ||
                (state.customListBoxCtor == nullptr) ||
                (state.customListBoxDtor == nullptr) ||
                (state.customListBoxAddItem == nullptr) ||
                (state.customListBoxDeleteAllItems == nullptr) ||
                (state.customListBoxSelectItem == nullptr) ||
                (state.customListBoxGetOldCustomItem == nullptr) ||
                (state.customListBoxItemOldBind == nullptr) ||
                (state.customListBoxItemOldSetText == nullptr)) {
                return false;
            }

            OpaqueCustomListBox listBoxProxy = {};
            state.customListBoxCtor(listBoxProxy.Get());
            state.widgetProxyBind(listBoxProxy.Get(), listBoxWidget);
            LogInfo(
                "CoH Mod Config UI: Bound CustomListBox proxy for row " +
                std::to_string(rowIndex) +
                " to raw widget '" +
                ReadWidgetNameForLog(listBoxWidget) +
                "'."
            );

            void* const oldItemProxy = state.customListBoxGetOldCustomItem(listBoxProxy.Get());
            if (oldItemProxy == nullptr) {
                LogWarning("CoH Mod Config UI: CustomListBox returned null old-item proxy for row " + std::to_string(rowIndex) + ".");
                state.customListBoxDtor(listBoxProxy.Get());
                return false;
            }
            {
                char addrBuf[32] = {};
                std::snprintf(addrBuf, sizeof(addrBuf), "0x%08X", reinterpret_cast<std::uintptr_t>(oldItemProxy));
                LogInfo(
                    "CoH Mod Config UI: CustomListBox old-item proxy for row " +
                    std::to_string(rowIndex) +
                    " is at " +
                    std::string(addrBuf) +
                    "."
                );
            }

            state.customListBoxDeleteAllItems(listBoxProxy.Get());
            LogInfo("CoH Mod Config UI: Cleared existing CustomListBox items for row " + std::to_string(rowIndex) + ".");
            if (optionEntry.choices.empty()) {
                LogWarning("CoH Mod Config UI: Enum row " + std::to_string(rowIndex) + " has no registered choices to populate.");
                state.customListBoxDtor(listBoxProxy.Get());
                return true;
            }

            long selectedChoiceIndex = 0;
            long listItemTextSubItemIndex = 0;
            bool listItemTextSubItemIndexResolved = false;
            for (std::size_t choiceIndex = 0u; choiceIndex < optionEntry.choices.size(); ++choiceIndex) {
                const ChoiceEntry& choiceEntry = optionEntry.choices[choiceIndex];
                const std::string itemName = MakeRowComboBoxListItemName(rowIndex, choiceIndex);
                const std::string displayText = BuildChoiceDisplayText(choiceEntry);
                const std::wstring wideText = ToWide(displayText);
                OpaqueLocString locString = {};
                state.locStringCtor(locString.Get(), wideText.c_str());
                const long addResult = state.customListBoxAddItem(
                    listBoxProxy.Get(),
                    itemName.c_str(),
                    true
                );
                void* const itemWidget = FindNamedWidget(state, listBoxWidget, itemName.c_str());
                if (!listItemTextSubItemIndexResolved) {
                    if (itemWidget == nullptr) {
                        LogWarning(
                            "CoH Mod Config UI: Row " + std::to_string(rowIndex) +
                            " could not resolve list item widget '" + itemName +
                            "' after AddItem; falling back to subitem index 0."
                        );
                    } else {
                        listItemTextSubItemIndexResolved = TryResolveListItemTextSubItemIndex(
                            state,
                            itemWidget,
                            rowIndex,
                            itemName,
                            listItemTextSubItemIndex
                        );
                    }
                }
                LogInfo(
                    "CoH Mod Config UI: Row " + std::to_string(rowIndex) +
                    " CustomListBox::AddItem added '" + itemName +
                    "' with result " + std::to_string(addResult) +
                    ", text subitem index " + std::to_string(listItemTextSubItemIndex) +
                    ", display text '" + displayText + "'."
                );
                bool usedNativeItemTextPath = false;
                if (addResult >= 0 && listItemTextSubItemIndexResolved) {
                    state.customListBoxItemOldBind(
                        oldItemProxy,
                        listBoxProxy.Get(),
                        itemName.c_str(),
                        listItemTextSubItemIndex
                    );
                    state.customListBoxItemOldSetText(oldItemProxy, locString.Get());
                    usedNativeItemTextPath = true;
                } else if (addResult >= 0 && itemWidget != nullptr) {
                    const bool usedFallbackLabel = EnsureDropdownListItemLabel(
                        state,
                        itemWidget,
                        rowIndex,
                        choiceIndex,
                        displayText
                    );
                    if (!usedFallbackLabel) {
                        LogWarning(
                            "CoH Mod Config UI: Fallback dropdown item label path failed for list item '" +
                            itemName +
                            "'."
                        );
                    }
                } else if (addResult >= 0) {
                    LogWarning(
                        "CoH Mod Config UI: Row " + std::to_string(rowIndex) +
                        " could not resolve list item widget '" + itemName +
                        "' for fallback dropdown label creation."
                    );
                }
                if (usedNativeItemTextPath) {
                    LogInfo(
                        "CoH Mod Config UI: Used native CustomListBoxItemOld text path for list item '" +
                        itemName +
                        "'."
                    );
                }
                state.locStringDtor(locString.Get());

                if (choiceEntry.value == optionEntry.currentValue.enumValue) {
                    selectedChoiceIndex = static_cast<long>(choiceIndex);
                }
            }

            long selectedNativeIndex = selectedChoiceIndex;
            if (!TryMapEnumChoiceIndexToNativeListIndex(optionEntry, selectedChoiceIndex, selectedNativeIndex)) {
                selectedNativeIndex = selectedChoiceIndex;
            }

            if (selectedNativeIndex >= 0) {
                state.customListBoxSelectItem(listBoxProxy.Get(), selectedNativeIndex);
            }
            if (state.customListBoxScrollToTop != nullptr) {
                state.customListBoxScrollToTop(listBoxProxy.Get());
            }
            state.customListBoxDtor(listBoxProxy.Get());

            LogInfo(
                "CoH Mod Config UI: Populated enum list box for row " +
                std::to_string(rowIndex) +
                " with " +
                std::to_string(optionEntry.choices.size()) +
                " choices; selected choice index=" +
                std::to_string(selectedChoiceIndex) +
                ", native selected index=" +
                std::to_string(selectedNativeIndex) +
                "."
            );
            return true;
        }

        float ComputeRowListBoxHeight(const OptionEntry& optionEntry) {
            const std::size_t choiceCount = optionEntry.choices.empty() ? 1u : optionEntry.choices.size();
            const std::size_t visibleRowCount = (std::min)(choiceCount, static_cast<std::size_t>(4u));
            return kRowComboBoxSizeY * static_cast<float>(visibleRowCount);
        }

        void ConfigureRowListBoxGeometry(State& state, std::size_t rowIndex, const OptionEntry& optionEntry) {
            if ((rowIndex >= kVisibleRowCount) ||
                (state.rowComboBoxWidgets[rowIndex] == nullptr) ||
                (state.rowListBoxWidgets[rowIndex] == nullptr)) {
                return;
            }

            const std::size_t choiceCount = optionEntry.choices.empty() ? 1u : optionEntry.choices.size();
            const bool needsScrollBar = choiceCount > 4u;
            const std::string listBoxName = MakeRowComboBoxListBoxName(rowIndex);
            const std::string itemsName = MakeRowComboBoxListBoxItemsName(rowIndex);
            const std::string scrollBarName = MakeRowComboBoxListBoxScrollBarName(rowIndex);
            const std::string itemTemplateName = MakeRowComboBoxListBoxItemTemplateName(rowIndex);
            const float listBoxSizeY = ComputeRowListBoxHeight(optionEntry);
            const float listBoxSizeX = needsScrollBar ? kRowListBoxSizeX : kRowListBoxContentSizeX;
            const float listContentSizeX = kRowListBoxContentSizeX;
            const float scrollBarPositionX = kRowListBoxContentSizeX;
            const float scrollBarSizeX = needsScrollBar ? kRowListBoxScrollBarSizeX : 0.0f;

            ConfigureRawWidget(
                state,
                state.rowListBoxWidgets[rowIndex],
                listBoxName.c_str(),
                kRowListBoxPositionX,
                kRowListBoxPositionY,
                listBoxSizeX,
                listBoxSizeY,
                state.rowComboBoxWidgets[rowIndex]
            );

            void* listItemsWidget = nullptr;
            void* listScrollBarWidget = nullptr;
            void* listItemTemplateWidget = nullptr;
            if (!ResolveListBoxChildWidgets(
                state,
                state.rowListBoxWidgets[rowIndex],
                listBoxName,
                listItemsWidget,
                listScrollBarWidget,
                listItemTemplateWidget
            )) {
                LogWarning("CoH Mod Config UI: Failed to reconfigure list box geometry for row " + std::to_string(rowIndex) + ".");
                return;
            }

            ConfigureRawWidget(
                state,
                listItemsWidget,
                itemsName.c_str(),
                0.0f,
                0.0f,
                listContentSizeX,
                listBoxSizeY,
                state.rowListBoxWidgets[rowIndex]
            );
            ConfigureRawWidget(
                state,
                listScrollBarWidget,
                scrollBarName.c_str(),
                scrollBarPositionX,
                0.0f,
                scrollBarSizeX,
                listBoxSizeY,
                state.rowListBoxWidgets[rowIndex]
            );
            ConfigureRawWidget(
                state,
                listItemTemplateWidget,
                itemTemplateName.c_str(),
                0.0f,
                0.0f,
                listContentSizeX,
                kRowComboBoxSizeY,
                state.rowListBoxWidgets[rowIndex]
            );

            if (!SetRawWidgetVisible(state, listScrollBarWidget, needsScrollBar)) {
                LogWarning(
                    "CoH Mod Config UI: Failed to set row " +
                    std::to_string(rowIndex) +
                    " list scrollbar visibility."
                );
            }

            LogInfo(
                "CoH Mod Config UI: Configured row " +
                std::to_string(rowIndex) +
                " list box height for " +
                std::to_string(optionEntry.choices.size()) +
                " choices to " +
                std::to_string(listBoxSizeY) +
                " with scrollbar " +
                (needsScrollBar ? std::string("visible") : std::string("hidden")) +
                "."
            );
        }

        bool TryGetCustomListBoxSelectedIndex(State& state, void* listBoxWidget, long& outSelectedIndex) {
            outSelectedIndex = -1;
            if ((listBoxWidget == nullptr) ||
                (state.widgetProxyBind == nullptr) ||
                (state.customListBoxCtor == nullptr) ||
                (state.customListBoxDtor == nullptr) ||
                (state.customListBoxGetSelectedIndex == nullptr)) {
                return false;
            }

            OpaqueCustomListBox listBoxProxy = {};
            state.customListBoxCtor(listBoxProxy.Get());
            state.widgetProxyBind(listBoxProxy.Get(), listBoxWidget);
            outSelectedIndex = state.customListBoxGetSelectedIndex(listBoxProxy.Get());
            state.customListBoxDtor(listBoxProxy.Get());
            return outSelectedIndex >= 0;
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

        bool RemoveRenderChild(State& state, void* parentWidget, void* childWidget) {
            void* const parentRenderObject = FindWidgetExtensionObject(state, parentWidget, kDrawChildrenExtensionId);
            if ((parentRenderObject == nullptr) || (childWidget == nullptr)) {
                return false;
            }

            const std::uintptr_t renderObjectAddress = reinterpret_cast<std::uintptr_t>(parentRenderObject);
            void** children = *reinterpret_cast<void***>(renderObjectAddress + 0x1Cu);
            unsigned int& count = *reinterpret_cast<unsigned int*>(renderObjectAddress + 0x20u);
            if ((children == nullptr) || (count == 0u)) {
                return false;
            }

            for (unsigned int index = 0u; index < count; ++index) {
                if (children[index] != childWidget) {
                    continue;
                }

                for (unsigned int shiftIndex = index + 1u; shiftIndex < count; ++shiftIndex) {
                    children[shiftIndex - 1u] = children[shiftIndex];
                }

                children[count - 1u] = nullptr;
                --count;
                return true;
            }

            return false;
        }

        bool EnsureDonorScreenLoaded(State& state, void*& screenSlot, const char* screenName) {
            if (screenSlot != nullptr) {
                return true;
            }

            ScreenManagerHandle* const screenManager = GetScreenManager(state);
            if ((screenManager == nullptr) || (state.loadScreenByName == nullptr)) {
                return false;
            }

            LogInfo("CoH Mod Config UI is loading donor screen: " + std::string(screenName));
            screenSlot = state.loadScreenByName(screenManager, screenName);
            if (screenSlot == nullptr) {
                LogError("CoH Mod Config UI failed to load donor screen: " + std::string(screenName));
                return false;
            }

            // Hide the donor screen so it doesn't render on top of the game.
            if (state.screenSetHidden != nullptr) {
                state.screenSetHidden(screenSlot, true);
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

        bool TransferDonorPresentationFrom(State& state, void* targetRawWidget, const char* donorScreenName, const char* donorWidgetName) {
            if ((targetRawWidget == nullptr) ||
                (donorScreenName == nullptr) ||
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

            OpaqueGenericWidget donorProxy = {};
            state.genericWidgetCtor(donorProxy.Get());

            const auto cleanup = [&]() {
                state.genericWidgetDtor(donorProxy.Get());
                };

            state.widgetProxyBindByName(donorProxy.Get(), donorScreenName, donorWidgetName);

            if (!state.widgetProxyIsValid(donorProxy.Get())) {
                LogError(
                    "CoH Mod Config UI could not find donor widget '" +
                    std::string(donorWidgetName) +
                    "' in screen '" +
                    std::string(donorScreenName) +
                    "'."
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

        // Transfer Presentation from donor ProgressBar's internal Progress child to target's child.
        // ProgressBar widgets have an extension (ID 14) at offset +8 pointing to the Progress fill child.
        bool TransferProgressBarChildPresentation(State& state, void* targetProgressBar, const char* donorScreenName, const char* donorWidgetName) {
            if ((targetProgressBar == nullptr) || (state.findWidgetExtension == nullptr) ||
                (state.widgetGetPresentation == nullptr) || (state.widgetSetPresentation == nullptr)) {
                return false;
            }

            // Find donor ProgressBar raw widget via proxy bind.
            OpaqueGenericWidget donorProxy = {};
            state.genericWidgetCtor(donorProxy.Get());
            const auto cleanup = [&]() { state.genericWidgetDtor(donorProxy.Get()); };

            state.widgetProxyBindByName(donorProxy.Get(), donorScreenName, donorWidgetName);
            if (!state.widgetProxyIsValid(donorProxy.Get())) {
                cleanup();
                return false;
            }

            void* donorRawWidget = state.widgetProxyGetWidget(donorProxy.Get());
            if (donorRawWidget == nullptr) { cleanup(); return false; }

            // Get extension 14 from both widgets to access Progress children.
            void* donorExt = state.findWidgetExtension(donorRawWidget, kProgressChildExtensionId);
            void* targetExt = state.findWidgetExtension(targetProgressBar, kProgressChildExtensionId);
            if ((donorExt == nullptr) || (targetExt == nullptr)) {
                LogWarning("CoH Mod Config UI: ProgressBar missing extension 14 (donor=" +
                    std::to_string(donorExt != nullptr) + ", target=" + std::to_string(targetExt != nullptr) + ").");
                cleanup();
                return false;
            }

            // Extension 14, offset +8 = Progress child widget pointer.
            void* donorChild = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(donorExt) + 8u);
            void* targetChild = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(targetExt) + 8u);
            if ((donorChild == nullptr) || (targetChild == nullptr)) {
                LogWarning("CoH Mod Config UI: ProgressBar Progress child is null.");
                cleanup();
                return false;
            }

            void* childPresentation = state.widgetGetPresentation(donorChild);
            if (childPresentation == nullptr) {
                LogWarning("CoH Mod Config UI: donor Progress child has null Presentation.");
                cleanup();
                return false;
            }

            state.widgetSetPresentation(targetChild, childPresentation);
            LogInfo("CoH Mod Config UI: transferred Progress child Presentation (addr=" +
                std::to_string(reinterpret_cast<std::uintptr_t>(childPresentation)) + ").");

            cleanup();
            return true;
        }

        // Direct presentation transfer using screen pointer + internal widget search.
        // Recursively search a widget tree for a widget by name.
        // Uses only findWidgetExtension (exported) and raw memory layout from RE:
        //   - Widget name: char buffer at widget + 0x04
        //   - DrawChildren extension (ID 1): children array at ext + 0x1C, count at ext + 0x20
        void* FindWidgetInTree(State& state, void* widget, const char* targetName) {
            if (widget == nullptr || targetName == nullptr) {
                return nullptr;
            }

            // Compare widget name at offset +4 (fixed-size char buffer, case-insensitive).
            const char* widgetName = reinterpret_cast<const char*>(
                reinterpret_cast<std::uintptr_t>(widget) + 4u
                );
            if (_stricmp(widgetName, targetName) == 0) {
                return widget;
            }

            // Get DrawChildren extension to access children.
            if (state.findWidgetExtension == nullptr) {
                return nullptr;
            }
            void* ext = state.findWidgetExtension(widget, kDrawChildrenExtensionId);
            if (ext == nullptr) {
                return nullptr;
            }

            const std::uintptr_t extAddr = reinterpret_cast<std::uintptr_t>(ext);
            void** children = *reinterpret_cast<void***>(extAddr + 0x1Cu);
            const unsigned int count = *reinterpret_cast<const unsigned int*>(extAddr + 0x20u);

            for (unsigned int i = 0u; i < count; ++i) {
                void* found = FindWidgetInTree(state, children[i], targetName);
                if (found != nullptr) {
                    return found;
                }
            }

            return nullptr;
        }

        // Transfer Presentation from a loaded (not necessarily activated) donor screen.
        // Walks the widget tree directly instead of using WidgetProxy::Bind.
        bool TransferDonorPresentationDirect(
            State& state,
            void* targetRawWidget,
            void* donorScreen,
            const char* donorWidgetName,
            bool allowNullPresentation = false
        ) {
            if ((targetRawWidget == nullptr) ||
                (donorScreen == nullptr) ||
                (donorWidgetName == nullptr) ||
                (state.widgetGetPresentation == nullptr) ||
                (state.widgetSetPresentation == nullptr) ||
                (state.screenGetRootWidget == nullptr)) {
                return false;
            }

            void* rootWidget = state.screenGetRootWidget(donorScreen);
            if (rootWidget == nullptr) {
                LogError("CoH Mod Config UI: donor screen has no root widget.");
                return false;
            }

            void* donorRawWidget = FindWidgetInTree(state, rootWidget, donorWidgetName);
            if (donorRawWidget == nullptr) {
                LogError(
                    "CoH Mod Config UI could not find donor widget '" +
                    std::string(donorWidgetName) +
                    "' via tree search."
                );
                return false;
            }

            void* presentation = state.widgetGetPresentation(donorRawWidget);
            if (presentation == nullptr) {
                if (!allowNullPresentation) {
                    LogWarning("CoH Mod Config UI: donor widget '" + std::string(donorWidgetName) + "' has null Presentation.");
                    return false;
                }

                state.widgetSetPresentation(targetRawWidget, nullptr);
                LogInfo(
                    "CoH Mod Config UI: donor widget '" +
                    std::string(donorWidgetName) +
                    "' has null Presentation; cleared target Presentation to match donor."
                );
            }
            else {
                state.widgetSetPresentation(targetRawWidget, presentation);
                LogInfo("CoH Mod Config UI: transferred Presentation from '" + std::string(donorWidgetName) +
                    "' (addr=" + std::to_string(reinterpret_cast<std::uintptr_t>(presentation)) + ").");
            }

            if (state.widgetGetHitArea != nullptr && state.widgetSetHitArea != nullptr) {
                void* hitArea = state.widgetGetHitArea(donorRawWidget);
                state.widgetSetHitArea(targetRawWidget, hitArea);
            }

            return true;
        }

#if defined(_M_IX86)
        // Naked trampoline: calls a game function that expects EAX as an implicit context parameter.
        // Stack layout at entry: [esp+4]=eaxContext, [esp+8]=targetFn, [esp+12]=arg1
        // Returns the game function's EAX result. Preserves ESI/EDI/EBX.
        __declspec(naked) void* __cdecl CallWithEaxContext(void* /*eaxContext*/, void* /*targetFn*/, const void* /*arg1*/) {
            __asm {
                push esi
                push edi
                push ebx
                mov eax, [esp + 16]; eaxContext(esp + 4 + 12 for 3 pushes)
                mov ecx, [esp + 20]; targetFn
                push dword ptr[esp + 24]; arg1
                call ecx
                ; callee cleans up the stack arg(ret 4), so no add esp needed
                pop ebx
                pop edi
                pop esi
                ret
            }
        }

        // Naked trampoline: calls a __thiscall function with ECX = thisPtr and one stack argument.
        // Stack layout at entry: [esp+4]=thisPtr, [esp+8]=targetFn, [esp+12]=arg1
        // Naked trampoline: calls a __cdecl function with 3 args while preserving callee-saved registers.
        // Stack layout at entry: [esp+4]=targetFn, [esp+8]=arg1, [esp+12]=arg2, [esp+16]=arg3
        // Returns the function's EAX result.
        __declspec(naked) void* __cdecl SafeCallCdecl3(void* /*targetFn*/, void* /*arg1*/, const void* /*arg2*/, int /*arg3*/) {
            __asm {
                push esi
                push edi
                push ebx
                mov eax, [esp + 16]; targetFn(esp + 4 + 12 for 3 pushes)
                push dword ptr[esp + 28]; arg3
                push dword ptr[esp + 28]; arg2(shifted by one push)
                push dword ptr[esp + 28]; arg1(shifted by two pushes)
                call eax
                add esp, 12
                pop ebx
                pop edi
                pop esi
                ret
            }
        }

        __declspec(naked) void __cdecl CallThiscall1(void* /*thisPtr*/, void* /*targetFn*/, void* /*arg1*/) {
            __asm {
                push esi
                push edi
                push ebx
                mov ecx, [esp + 16]; thisPtr
                mov eax, [esp + 20]; targetFn
                push dword ptr[esp + 24]; arg1
                call eax
                ; __thiscall is callee - cleanup, so no add esp needed
                pop ebx
                pop edi
                pop esi
                ret
            }
        }
#endif

        bool ApplyWidgetStyle(State& state, void* rawWidget, const char* styleSetName, const char* styleName) {
            if ((rawWidget == nullptr) || (styleSetName == nullptr) || (styleName == nullptr) ||
                (state.getStyleManager == nullptr) || (state.userInterfaceBase == 0u)) {
                return false;
            }

            ScreenManagerHandle* const screenManager = GetScreenManager(state);
            if (screenManager == nullptr) {
                return false;
            }

            void* styleManager = state.getStyleManager(screenManager);
            if (styleManager == nullptr) {
                LogWarning("CoH Mod Config UI: GetStyleManager returned null.");
                return false;
            }

#if defined(_M_IX86)
            void* findStyleSetFn = reinterpret_cast<void*>(state.userInterfaceBase + kFindStyleSetRva);
            void* findStyleFn = reinterpret_cast<void*>(state.userInterfaceBase + kFindStyleInSetRva);

            // FUN_10047950(styleSetName) with EAX = styleManager
            void* styleSet = CallWithEaxContext(styleManager, findStyleSetFn, styleSetName);
            if (styleSet == nullptr) {
                LogWarning("CoH Mod Config UI: Style set '" + std::string(styleSetName) + "' not found.");
                return false;
            }

            // FUN_1003caf0(styleName) with EAX = styleSet
            void* style = CallWithEaxContext(styleSet, findStyleFn, styleName);
            if (style == nullptr) {
                LogWarning("CoH Mod Config UI: Style '" + std::string(styleName) + "' not found in set '" + std::string(styleSetName) + "'.");
                return false;
            }

            // Call style->Apply(rawWidget) via vtable[2]
            // Game code: (**(code **)(*style + 8))(widget)  — __thiscall: ECX = style
            void* vtablePtr = *reinterpret_cast<void**>(style);
            void* applyFnAddr = reinterpret_cast<void**>(vtablePtr)[2];
            using StyleApplyFn = void(__thiscall*)(void* thisStyle, void* widget);
            auto applyFn = reinterpret_cast<StyleApplyFn>(applyFnAddr);
            applyFn(style, rawWidget);

            return true;
#else
            (void)styleManager;
            return false;
#endif
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

        bool TrySetCheckButtonText(State& state, OpaqueCheckButton& checkButton, const std::string& text) {
            if ((state.checkButtonSetText == nullptr) || (state.locStringCtor == nullptr) || (state.locStringDtor == nullptr)) {
                return false;
            }

            const std::wstring wideText = ToWide(text);
            OpaqueLocString locString = {};
            state.locStringCtor(locString.Get(), wideText.c_str());
            state.checkButtonSetText(checkButton.Get(), locString.Get());
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

            if ((state.loadScreenByName == nullptr) ||
                (state.findWidgetByName == nullptr) ||
                (state.buttonCtor == nullptr) ||
                (state.textLabelCtor == nullptr) ||
                (state.widgetProxyBind == nullptr)) {
                LogError("CoH Mod Config UI cannot build the overlay because one or more UI functions are unavailable.");
                return false;
            }

            // Step 1: Register file override so the engine can find our custom .screen file.
            if (!RegisterFileOverride(state)) {
                LogError("CoH Mod Config UI failed to register file override.");
                return false;
            }

            // Step 2: Load our custom screen.
            LogInfo("CoH Mod Config UI: calling LoadScreen('" + std::string(kScreenName) + "')...");
            state.screen = state.loadScreenByName(screenManager, kScreenName);
            if (state.screen == nullptr) {
                LogError("CoH Mod Config UI: LoadScreen returned null.");
                return false;
            }

            LogInfo("CoH Mod Config UI: LoadScreen returned non-null. Calling SetHidden...");
            if (state.screenSetHidden != nullptr) {
                state.screenSetHidden(state.screen, true);
            }

            // Step 3: Get root widget.
            state.rootWidgetRaw = GetScreenRootWidget(state, state.screen);
            {
                char addrBuf[32] = {};
                std::snprintf(addrBuf, sizeof(addrBuf), "0x%08X", reinterpret_cast<std::uintptr_t>(state.rootWidgetRaw));
                LogInfo(std::string("CoH Mod Config UI: GetRootWidget returned ") + addrBuf);
            }

            // Step 4: Dump the widget name at rootWidget+4 to see what the engine stored.
            if (state.rootWidgetRaw != nullptr) {
                const char* nameAt4 = reinterpret_cast<const char*>(reinterpret_cast<std::uintptr_t>(state.rootWidgetRaw) + 4);
                // Safely read up to 64 chars.
                char nameBuf[65] = {};
                for (int i = 0; i < 64; ++i) {
                    char c = nameAt4[i];
                    if (c == '\0') break;
                    if (c < 0x20 || c > 0x7E) { nameBuf[i] = '?'; } else { nameBuf[i] = c; }
                }
                LogInfo(std::string("CoH Mod Config UI: rootWidget+4 name string = '") + nameBuf + "'");

                // Also dump first 64 bytes as hex for inspection.
                const unsigned char* raw = reinterpret_cast<const unsigned char*>(state.rootWidgetRaw);
                std::string hexDump;
                for (int i = 0; i < 64; ++i) {
                    char hex[4] = {};
                    std::snprintf(hex, sizeof(hex), "%02X ", raw[i]);
                    hexDump += hex;
                }
                LogInfo("CoH Mod Config UI: rootWidget first 64 bytes: " + hexDump);
            }

            // Step 5: Try findWidgetByName with flags=0 and flags=1.
            LogInfo("CoH Mod Config UI: Calling findWidgetByName('cohmodconfigui_root', flags=0)...");
            void* rootGroup0 = state.findWidgetByName(state.rootWidgetRaw, "cohmodconfigui_root", 0);
            {
                char addrBuf[32] = {};
                std::snprintf(addrBuf, sizeof(addrBuf), "0x%08X", reinterpret_cast<std::uintptr_t>(rootGroup0));
                LogInfo(std::string("CoH Mod Config UI: flags=0 returned ") + addrBuf);
            }

            LogInfo("CoH Mod Config UI: Calling findWidgetByName('cohmodconfigui_root', flags=1)...");
            void* rootGroup1 = state.findWidgetByName(state.rootWidgetRaw, "cohmodconfigui_root", 1);
            {
                char addrBuf[32] = {};
                std::snprintf(addrBuf, sizeof(addrBuf), "0x%08X", reinterpret_cast<std::uintptr_t>(rootGroup1));
                LogInfo(std::string("CoH Mod Config UI: flags=1 returned ") + addrBuf);
            }

            // Step 6: Create panel Group and attach to root.
            state.panelWidgetRaw = CreateRawWidgetByType(state, kGroupWidgetTypeName);
            if (state.panelWidgetRaw == nullptr) {
                LogError("CoH Mod Config UI: Failed to create panel Group widget.");
                return false;
            }
            LogInfo("CoH Mod Config UI: Created panel Group widget.");

            // Transfer Presentation from donor screen so the panel has a visible background.
            void* donorScreen = nullptr;
            if (!EnsureDonorScreenLoaded(state, donorScreen, kTemplateScreenName)) {
                LogError("CoH Mod Config UI: Failed to load donor screen '" + std::string(kTemplateScreenName) + "'.");
                return false;
            }
            if (!TransferDonorPresentationDirect(state, state.panelWidgetRaw, donorScreen, kTemplatePanelWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer panel Presentation from donor. Panel may be invisible.");
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

            if (!AttachRenderChild(state, state.rootWidgetRaw, state.panelWidgetRaw)) {
                LogError("CoH Mod Config UI: Failed to attach panel to root render tree.");
                return false;
            }
            LogInfo("CoH Mod Config UI: Panel attached to root render tree.");

            // Step 7: Create title TextLabel with donor Presentation, attach to panel.
            state.titleLabelRaw = CreateRawWidgetByType(state, kTextLabelWidgetTypeName);
            if (state.titleLabelRaw == nullptr) {
                LogError("CoH Mod Config UI: Failed to create title TextLabel widget.");
                return false;
            }
            if (!TransferDonorPresentationDirect(state, state.titleLabelRaw, donorScreen, kTemplateLabelWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer title Presentation from donor.");
            }
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
            if (!AttachRenderChild(state, state.panelWidgetRaw, state.titleLabelRaw)) {
                LogError("CoH Mod Config UI: Failed to attach title label to panel render tree.");
                return false;
            }
            LogInfo("CoH Mod Config UI: Title label created and attached.");

            // Step 8: Create summary TextLabel with donor Presentation, attach to panel.
            state.summaryLabelRaw = CreateRawWidgetByType(state, kTextLabelWidgetTypeName);
            if (state.summaryLabelRaw == nullptr) {
                LogError("CoH Mod Config UI: Failed to create summary TextLabel widget.");
                return false;
            }
            if (!TransferDonorPresentationDirect(state, state.summaryLabelRaw, donorScreen, kTemplateLabelWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer summary Presentation from donor.");
            }
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
            if (!AttachRenderChild(state, state.panelWidgetRaw, state.summaryLabelRaw)) {
                LogError("CoH Mod Config UI: Failed to attach summary label to panel render tree.");
                return false;
            }
            LogInfo("CoH Mod Config UI: Summary label created and attached.");

            // Step 9: Create footer TextLabel with donor Presentation, attach to panel.
            state.footerLabelRaw = CreateRawWidgetByType(state, kTextLabelWidgetTypeName);
            if (state.footerLabelRaw == nullptr) {
                LogError("CoH Mod Config UI: Failed to create footer TextLabel widget.");
                return false;
            }
            if (!TransferDonorPresentationDirect(state, state.footerLabelRaw, donorScreen, kTemplateLabelWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer footer Presentation from donor.");
            }
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
            if (!AttachRenderChild(state, state.panelWidgetRaw, state.footerLabelRaw)) {
                LogError("CoH Mod Config UI: Failed to attach footer label to panel render tree.");
                return false;
            }
            LogInfo("CoH Mod Config UI: Footer label created and attached.");

            // Step 10: Create row name TextLabels and attach them to the panel.
            for (std::size_t i = 0u; i < kVisibleRowCount; ++i) {
                state.rowLabelWidgets[i] = CreateRawWidgetByType(state, kTextLabelWidgetTypeName);
                if (state.rowLabelWidgets[i] == nullptr) {
                    LogError("CoH Mod Config UI: Failed to create row label TextLabel widget for row " + std::to_string(i) + ".");
                    return false;
                }
                if (!TransferDonorPresentationDirect(state, state.rowLabelWidgets[i], donorScreen, kTemplateLabelWidgetName)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer row label Presentation from donor for row " + std::to_string(i) + ".");
                }

                const std::string rowLabelName = MakeRowLabelName(i);
                ConfigureRawWidget(
                    state,
                    state.rowLabelWidgets[i],
                    rowLabelName.c_str(),
                    kFirstRowPositionX,
                    kFirstRowPositionY + (static_cast<float>(i) * kRowSpacingY),
                    kRowLabelSizeX,
                    kRowLabelSizeY,
                    state.panelWidgetRaw
                );
                if (!AttachRenderChild(state, state.panelWidgetRaw, state.rowLabelWidgets[i])) {
                    LogError("CoH Mod Config UI: Failed to attach row label to panel render tree for row " + std::to_string(i) + ".");
                    return false;
                }
            }
            LogInfo("CoH Mod Config UI: Row label widgets created and attached.");

            // Step 11: Resolve native CheckButton widgets preloaded by cohmodconfigui.screen.
            for (std::size_t i = 0u; i < kVisibleRowCount; ++i) {
                const std::string rowCheckButtonName = MakeRowCheckButtonName(i);
                state.rowCheckButtonWidgets[i] = state.findWidgetByName(state.rootWidgetRaw, rowCheckButtonName.c_str(), 0);
                if (state.rowCheckButtonWidgets[i] == nullptr) {
                    state.rowCheckButtonWidgets[i] = state.findWidgetByName(state.rootWidgetRaw, rowCheckButtonName.c_str(), 1);
                    if (state.rowCheckButtonWidgets[i] == nullptr) {
                        LogError("CoH Mod Config UI: Failed to resolve preloaded CheckButton widget '" + rowCheckButtonName + "' for row " + std::to_string(i) + ".");
                        return false;
                    }
                }

                if (!RemoveRenderChild(state, state.rootWidgetRaw, state.rowCheckButtonWidgets[i])) {
                    LogError("CoH Mod Config UI: Failed to remove preloaded CheckButton widget '" + rowCheckButtonName + "' from the root render tree for row " + std::to_string(i) + ".");
                    return false;
                }

                ConfigureRawWidget(
                    state,
                    state.rowCheckButtonWidgets[i],
                    rowCheckButtonName.c_str(),
                    kRowControlPositionX,
                    kFirstRowPositionY + (static_cast<float>(i) * kRowSpacingY),
                    kRowCheckButtonSizeX,
                    kRowCheckButtonSizeY,
                    state.panelWidgetRaw
                );
                if (!AttachRenderChild(state, state.panelWidgetRaw, state.rowCheckButtonWidgets[i])) {
                    LogError("CoH Mod Config UI: Failed to attach preloaded CheckButton widget '" + rowCheckButtonName + "' to the panel render tree for row " + std::to_string(i) + ".");
                    return false;
                }

                SetRawWidgetVisible(state, state.rowCheckButtonWidgets[i], false);
                LogInfo("CoH Mod Config UI: Resolved, moved, and attached preloaded CheckButton widget '" + rowCheckButtonName + "' for row " + std::to_string(i) + ".");
            }
            LogInfo("CoH Mod Config UI: Row bool CheckButton widgets resolved from the active screen.");

            // Step 12: Create native ComboBox widgets for enum rows and resolve their child widgets.
            void* optionsMenuDonorScreen = nullptr;
            if (!EnsureDonorScreenLoaded(state, optionsMenuDonorScreen, kOptionsmenuDonorScreenName)) {
                LogError("CoH Mod Config UI: Failed to load donor screen '" + std::string(kOptionsmenuDonorScreenName) + "' for native ComboBox widgets.");
                return false;
            }

            for (std::size_t i = 0u; i < kVisibleRowCount; ++i) {
                state.rowComboBoxWidgets[i] = CreateRawWidgetByType(state, kComboBoxWidgetTypeName);
                if (state.rowComboBoxWidgets[i] == nullptr) {
                    LogError("CoH Mod Config UI: Failed to create native ComboBox widget for row " + std::to_string(i) + ".");
                    return false;
                }

                if (!TransferDonorPresentationDirect(state, state.rowComboBoxWidgets[i], optionsMenuDonorScreen, kDropdownDonorWidgetName)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer ComboBox root Presentation from donor for row " + std::to_string(i) + ".");
                }

                const std::string rowComboBoxName = MakeRowComboBoxName(i);
                ConfigureRawWidget(
                    state,
                    state.rowComboBoxWidgets[i],
                    rowComboBoxName.c_str(),
                    kRowControlPositionX,
                    kFirstRowPositionY + (static_cast<float>(i) * kRowSpacingY),
                    kRowComboBoxSizeX,
                    kRowComboBoxSizeY,
                    state.panelWidgetRaw
                );
                if (!AttachRenderChild(state, state.panelWidgetRaw, state.rowComboBoxWidgets[i])) {
                    LogError("CoH Mod Config UI: Failed to attach ComboBox root to panel render tree for row " + std::to_string(i) + ".");
                    return false;
                }

                void* rowListBoxWidget = nullptr;
                if (!ResolveComboBoxChildWidgets(
                    state,
                    state.rowComboBoxWidgets[i],
                    rowComboBoxName,
                    state.rowValueLabelWidgets[i],
                    state.rowArrowButtonWidgets[i],
                    rowListBoxWidget
                )) {
                    LogError("CoH Mod Config UI: Failed to resolve DropDownExt child widgets for row " + std::to_string(i) + ".");
                    return false;
                }

                const std::string expectedLabelName = MakeRowComboBoxLabelName(i);
                const std::string expectedButtonName = MakeRowComboBoxButtonName(i);
                const std::string expectedListBoxName = MakeRowComboBoxListBoxName(i);
                const std::string actualLabelName = ReadWidgetNameForLog(state.rowValueLabelWidgets[i]);
                const std::string actualButtonName = ReadWidgetNameForLog(state.rowArrowButtonWidgets[i]);
                const std::string actualListBoxName = ReadWidgetNameForLog(rowListBoxWidget);
                LogInfo(
                    "CoH Mod Config UI: Row " + std::to_string(i) +
                    " ComboBox child widgets resolved as label='" + actualLabelName +
                    "', button='" + actualButtonName +
                    "', listBox='" + actualListBoxName + "'."
                );

                if ((_stricmp(actualLabelName.c_str(), expectedLabelName.c_str()) != 0) ||
                    (_stricmp(actualButtonName.c_str(), expectedButtonName.c_str()) != 0) ||
                    (_stricmp(actualListBoxName.c_str(), expectedListBoxName.c_str()) != 0)) {
                    LogWarning(
                        "CoH Mod Config UI: Row " + std::to_string(i) +
                        " ComboBox child name mismatch. Expected '" + expectedLabelName +
                        "', '" + expectedButtonName +
                        "', '" + expectedListBoxName +
                        "' but saw '" + actualLabelName +
                        "', '" + actualButtonName +
                        "', '" + actualListBoxName + "'."
                    );
                }

                state.rowListBoxWidgets[i] = rowListBoxWidget;
                ConfigureRawWidget(
                    state,
                    state.rowValueLabelWidgets[i],
                    expectedLabelName.c_str(),
                    0.0f,
                    0.0f,
                    kRowValueLabelSizeX,
                    kRowValueLabelSizeY,
                    state.rowComboBoxWidgets[i]
                );
                ConfigureRawWidget(
                    state,
                    state.rowArrowButtonWidgets[i],
                    expectedButtonName.c_str(),
                    kRowArrowButtonOffsetX,
                    0.0f,
                    kRowArrowButtonSizeX,
                    kRowArrowButtonSizeY,
                    state.rowComboBoxWidgets[i]
                );
                ConfigureRawWidget(
                    state,
                    state.rowListBoxWidgets[i],
                    expectedListBoxName.c_str(),
                    kRowListBoxPositionX,
                    kRowListBoxPositionY,
                    kRowListBoxSizeX,
                    kRowListBoxSizeY,
                    state.rowComboBoxWidgets[i]
                );
                LogInfo("CoH Mod Config UI: Row " + std::to_string(i) + " ComboBox label/button geometry configured.");

                if (!TransferDonorPresentationDirect(state, state.rowValueLabelWidgets[i], optionsMenuDonorScreen, kDropdownLabelDonorWidgetName)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer ComboBox label Presentation from donor for row " + std::to_string(i) + ".");
                }
                if (!TransferDonorPresentationDirect(state, state.rowArrowButtonWidgets[i], optionsMenuDonorScreen, kDropdownButtonDonorWidgetName)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer ComboBox button Presentation from donor for row " + std::to_string(i) + ".");
                }
                if (!TransferDonorPresentationDirect(state, rowListBoxWidget, optionsMenuDonorScreen, kDropdownListBoxDonorWidgetName, true)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer ComboBox list box Presentation from donor for row " + std::to_string(i) + ".");
                }

                void* rowListItemsWidget = nullptr;
                void* rowListScrollBarWidget = nullptr;
                void* rowListItemTemplateWidget = nullptr;
                if (ResolveListBoxChildWidgets(
                    state,
                    rowListBoxWidget,
                    expectedListBoxName,
                    rowListItemsWidget,
                    rowListScrollBarWidget,
                    rowListItemTemplateWidget
                )) {
                    const std::string expectedItemsName = MakeRowComboBoxListBoxItemsName(i);
                    const std::string expectedScrollBarName = MakeRowComboBoxListBoxScrollBarName(i);
                    const std::string expectedItemTemplateName = MakeRowComboBoxListBoxItemTemplateName(i);
                    LogInfo(
                        "CoH Mod Config UI: Row " + std::to_string(i) +
                        " list box child widgets resolved as items='" + ReadWidgetNameForLog(rowListItemsWidget) +
                        "', scrollBar='" + ReadWidgetNameForLog(rowListScrollBarWidget) +
                        "', itemTemplate='" + ReadWidgetNameForLog(rowListItemTemplateWidget) + "'."
                    );

                    ConfigureRawWidget(
                        state,
                        rowListItemsWidget,
                        expectedItemsName.c_str(),
                        0.0f,
                        0.0f,
                        kRowListBoxContentSizeX,
                        kRowListBoxSizeY,
                        rowListBoxWidget
                    );
                    ConfigureRawWidget(
                        state,
                        rowListScrollBarWidget,
                        expectedScrollBarName.c_str(),
                        kRowListBoxContentSizeX,
                        0.0f,
                        kRowListBoxScrollBarSizeX,
                        kRowListBoxSizeY,
                        rowListBoxWidget
                    );
                    ConfigureRawWidget(
                        state,
                        rowListItemTemplateWidget,
                        expectedItemTemplateName.c_str(),
                        0.0f,
                        0.0f,
                        kRowListBoxContentSizeX,
                        kRowComboBoxSizeY,
                        rowListBoxWidget
                    );

                    if (!TransferDonorPresentationDirect(state, rowListItemsWidget, optionsMenuDonorScreen, kDropdownListBoxItemsDonorWidgetName)) {
                        LogWarning("CoH Mod Config UI: Failed to transfer ComboBox list items Presentation from donor for row " + std::to_string(i) + ".");
                    }
                    if (!TransferDonorPresentationDirect(state, rowListScrollBarWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarDonorWidgetName)) {
                        LogWarning("CoH Mod Config UI: Failed to transfer ComboBox list scrollbar Presentation from donor for row " + std::to_string(i) + ".");
                    }
                    void* rowScrollBarDecButtonWidget = nullptr;
                    void* rowScrollBarIncButtonWidget = nullptr;
                    void* rowScrollBarTrackButtonWidget = nullptr;
                    void* rowScrollBarPageDownButtonWidget = nullptr;
                    void* rowScrollBarPageUpButtonWidget = nullptr;
                    if (ResolveScrollBarChildWidgets(
                        state,
                        rowListScrollBarWidget,
                        expectedScrollBarName,
                        rowScrollBarDecButtonWidget,
                        rowScrollBarIncButtonWidget,
                        rowScrollBarTrackButtonWidget,
                        rowScrollBarPageDownButtonWidget,
                        rowScrollBarPageUpButtonWidget
                    )) {
                        LogInfo(
                            "CoH Mod Config UI: Row " + std::to_string(i) +
                            " scrollbar child widgets resolved as dec='" + ReadWidgetNameForLog(rowScrollBarDecButtonWidget) +
                            "', inc='" + ReadWidgetNameForLog(rowScrollBarIncButtonWidget) +
                            "', track='" + ReadWidgetNameForLog(rowScrollBarTrackButtonWidget) +
                            "', pgDn='" + ReadWidgetNameForLog(rowScrollBarPageDownButtonWidget) +
                            "', pgUp='" + ReadWidgetNameForLog(rowScrollBarPageUpButtonWidget) + "'."
                        );

                        if (!TransferDonorPresentationDirect(state, rowScrollBarDecButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarDecDonorWidgetName)) {
                            LogWarning("CoH Mod Config UI: Failed to transfer scrollbar decrement button Presentation from donor for row " + std::to_string(i) + ".");
                        }
                        if (!TransferDonorPresentationDirect(state, rowScrollBarIncButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarIncDonorWidgetName)) {
                            LogWarning("CoH Mod Config UI: Failed to transfer scrollbar increment button Presentation from donor for row " + std::to_string(i) + ".");
                        }
                        if (!TransferDonorPresentationDirect(state, rowScrollBarTrackButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarTrackDonorWidgetName)) {
                            LogWarning("CoH Mod Config UI: Failed to transfer scrollbar track Presentation from donor for row " + std::to_string(i) + ".");
                        }
                        if (!TransferDonorPresentationDirect(state, rowScrollBarPageDownButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarPageDownDonorWidgetName, true)) {
                            LogWarning("CoH Mod Config UI: Failed to transfer scrollbar page-down Presentation from donor for row " + std::to_string(i) + ".");
                        }
                        if (!TransferDonorPresentationDirect(state, rowScrollBarPageUpButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarPageUpDonorWidgetName, true)) {
                            LogWarning("CoH Mod Config UI: Failed to transfer scrollbar page-up Presentation from donor for row " + std::to_string(i) + ".");
                        }
                    }
                    else {
                        LogWarning("CoH Mod Config UI: Failed to resolve scrollbar subtree widgets for row " + std::to_string(i) + ".");
                    }
                    if (!TransferDonorPresentationDirect(state, rowListItemTemplateWidget, optionsMenuDonorScreen, kDropdownListBoxItemTemplateDonorWidgetName)) {
                        LogWarning("CoH Mod Config UI: Failed to transfer ComboBox list item template Presentation from donor for row " + std::to_string(i) + ".");
                    }
                }
                else {
                    LogWarning("CoH Mod Config UI: Failed to resolve list box subtree widgets for row " + std::to_string(i) + ".");
                }
            }
            LogInfo("CoH Mod Config UI: Native row ComboBox widgets created, attached, and child widgets resolved.");

            // Step 13: Construct and bind title, summary, footer, row label, bool, and enum proxies.
            state.textLabelCtor(state.titleLabel.Get());
            state.widgetProxyBind(state.titleLabel.Get(), state.titleLabelRaw);
            ApplyWidgetProxyState(state, state.titleLabel.Get());
            TrySetTextLabel(state, state.titleLabel, "Mod Options", false);

            state.textLabelCtor(state.summaryLabel.Get());
            state.widgetProxyBind(state.summaryLabel.Get(), state.summaryLabelRaw);
            ApplyWidgetProxyState(state, state.summaryLabel.Get());
            TrySetTextLabel(state, state.summaryLabel, "Preparing mod configuration catalog...", true);

            state.textLabelCtor(state.footerLabel.Get());
            state.widgetProxyBind(state.footerLabel.Get(), state.footerLabelRaw);
            ApplyWidgetProxyState(state, state.footerLabel.Get());
            TrySetTextLabel(state, state.footerLabel, BuildFooterText(), false);

            for (std::size_t i = 0u; i < kVisibleRowCount; ++i) {
                state.textLabelCtor(state.rowLabels[i].Get());
                state.widgetProxyBind(state.rowLabels[i].Get(), state.rowLabelWidgets[i]);
                ApplyWidgetProxyState(state, state.rowLabels[i].Get());
                TrySetTextLabel(state, state.rowLabels[i], BuildEmptyButtonText(), false);

                state.textLabelCtor(state.rowValueLabels[i].Get());
                state.widgetProxyBind(state.rowValueLabels[i].Get(), state.rowValueLabelWidgets[i]);
                ApplyWidgetProxyState(state, state.rowValueLabels[i].Get());
                TrySetTextLabel(state, state.rowValueLabels[i], BuildEmptyButtonText(), false);

                if (!BindButtonProxy(state, state.rowArrowButtons[i], state.rowArrowButtonWidgets[i])) {
                    LogError("CoH Mod Config UI: Failed to bind ComboBox button proxy for row " + std::to_string(i) + ".");
                    return false;
                }
                if (state.widgetProxySetVisible != nullptr) {
                    state.widgetProxySetVisible(state.rowArrowButtons[i].Get(), true);
                }
                if (state.widgetProxySetEnabled != nullptr) {
                    state.widgetProxySetEnabled(state.rowArrowButtons[i].Get(), true);
                }

                if (!BindCheckButtonProxy(state, state.rowCheckButtons[i], state.rowCheckButtonWidgets[i])) {
                    LogError("CoH Mod Config UI: Failed to bind CheckButton proxy for row " + std::to_string(i) + ".");
                    return false;
                }
                ApplyWidgetProxyState(state, state.rowCheckButtons[i].Get());
                if (!TrySetCheckButtonText(state, state.rowCheckButtons[i], BuildEmptyButtonText())) {
                    LogWarning("CoH Mod Config UI: Failed to blank CheckButton text for row " + std::to_string(i) + ".");
                }

                state.progressBarCtor(state.rowProgressBars[i].Get());
            }
            LogInfo("CoH Mod Config UI: All proxy objects constructed.");

            state.overlayBuilt = true;
            LogInfo("CoH Mod Config UI: Overlay built successfully (panel + title + summary + footer + row labels + row CheckButtons + native enum ComboBox child binding milestone).");
            return true;
        }

        void DestroyOverlay(State& state) {
            if (!state.overlayBuilt && (state.screen == nullptr)) {
                return;
            }

            if (state.overlayBuilt) {
                for (std::size_t i = kVisibleRowCount; i > 0u; --i) {
                    state.progressBarDtor(state.rowProgressBars[i - 1u].Get());
                    state.checkButtonDtor(state.rowCheckButtons[i - 1u].Get());
                    state.buttonDtor(state.rowArrowButtons[i - 1u].Get());
                    state.textLabelDtor(state.rowValueLabels[i - 1u].Get());
                    state.textLabelDtor(state.rowLabels[i - 1u].Get());
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

        void HideAllRowControls(State& state, std::size_t rowIndex) {
            if (state.widgetProxySetVisible != nullptr) {
                state.widgetProxySetVisible(state.rowLabels[rowIndex].Get(), false);
                state.widgetProxySetVisible(state.rowValueLabels[rowIndex].Get(), false);
                state.widgetProxySetVisible(state.rowArrowButtons[rowIndex].Get(), false);
                state.widgetProxySetVisible(state.rowCheckButtons[rowIndex].Get(), false);
                state.widgetProxySetVisible(state.rowProgressBars[rowIndex].Get(), false);
            }
            SetRawWidgetVisible(state, state.rowComboBoxWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowListBoxWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowCheckButtonWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowProgressBarWidgets[rowIndex], false);
            state.rowObservedListBoxSelection[rowIndex] = -1;
            state.rowHasObservedListBoxSelection[rowIndex] = false;
            state.rowActiveControlType[rowIndex] = CoHModSDKConfigType_Bool;
        }

        void UpdateRowForOption(State& state, std::size_t rowIndex, const SelectedOptionRef& rowOption) {
            const OptionEntry& opt = *rowOption.optionEntry;
            state.rowObservedListBoxSelection[rowIndex] = -1;
            state.rowHasObservedListBoxSelection[rowIndex] = false;

            TrySetTextLabel(state, state.rowLabels[rowIndex], BuildRowLabelText(rowOption), false);

            // Hide all control widgets first, then show the right ones for this option type.
            if (state.widgetProxySetVisible != nullptr) {
                state.widgetProxySetVisible(state.rowValueLabels[rowIndex].Get(), false);
                state.widgetProxySetVisible(state.rowArrowButtons[rowIndex].Get(), false);
                state.widgetProxySetVisible(state.rowCheckButtons[rowIndex].Get(), false);
                state.widgetProxySetVisible(state.rowProgressBars[rowIndex].Get(), false);
            }
            SetRawWidgetVisible(state, state.rowComboBoxWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowListBoxWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowCheckButtonWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowProgressBarWidgets[rowIndex], false);

            switch (opt.type) {
            case CoHModSDKConfigType_Bool:
                // Show checkbox.
                if (state.checkButtonSetChecked != nullptr) {
                    state.checkButtonSetChecked(state.rowCheckButtons[rowIndex].Get(), opt.currentValue.boolValue != 0u);
                }
                SetRawWidgetVisible(state, state.rowCheckButtonWidgets[rowIndex], true);
                if (state.widgetProxySetVisible != nullptr) {
                    state.widgetProxySetVisible(state.rowCheckButtons[rowIndex].Get(), true);
                }
                break;

            case CoHModSDKConfigType_Int:
            case CoHModSDKConfigType_Float: {
                // Show progress bar as a slider.
                float progress = 0.0f;
                if (opt.minValue < opt.maxValue) {
                    if (opt.type == CoHModSDKConfigType_Int) {
                        progress = static_cast<float>(opt.currentValue.intValue - static_cast<std::int32_t>(std::lround(opt.minValue))) /
                            static_cast<float>(static_cast<std::int32_t>(std::lround(opt.maxValue)) - static_cast<std::int32_t>(std::lround(opt.minValue)));
                    }
                    else {
                        progress = (opt.currentValue.floatValue - opt.minValue) / (opt.maxValue - opt.minValue);
                    }
                    progress = std::clamp(progress, 0.0f, 1.0f);
                }
                state.progressBarSetRange(state.rowProgressBars[rowIndex].Get(), 0.0f, 1.0f);
                state.progressBarSetProgress(state.rowProgressBars[rowIndex].Get(), progress);
                SetRawWidgetVisible(state, state.rowProgressBarWidgets[rowIndex], true);
                if (state.widgetProxySetVisible != nullptr) {
                    state.widgetProxySetVisible(state.rowProgressBars[rowIndex].Get(), true);
                }
                break;
            }

            case CoHModSDKConfigType_Enum:
            default:
                // Show dropdown-style value label + arrow button.
                SetRawWidgetVisible(state, state.rowComboBoxWidgets[rowIndex], true);
                SetRawWidgetVisible(state, state.rowListBoxWidgets[rowIndex], true);
                ConfigureRowListBoxGeometry(state, rowIndex, opt);
                if (!PopulateEnumListBox(state, state.rowListBoxWidgets[rowIndex], opt, rowIndex)) {
                    LogWarning("CoH Mod Config UI: Failed to populate enum list box for row " + std::to_string(rowIndex) + ".");
                }
                TrySetTextLabel(state, state.rowValueLabels[rowIndex], BuildRowValueText(rowOption), false);
                if (state.widgetProxySetVisible != nullptr) {
                    state.widgetProxySetVisible(state.rowValueLabels[rowIndex].Get(), true);
                    state.widgetProxySetVisible(state.rowArrowButtons[rowIndex].Get(), true);
                }
                break;
            }

            if (state.widgetProxySetVisible != nullptr) {
                state.widgetProxySetVisible(state.rowLabels[rowIndex].Get(), true);
            }

            state.rowActiveControlType[rowIndex] = opt.type;
        }

        bool RefreshVisibleMenu(State& state) {
            if (!BuildOverlay(state)) {
                return false;
            }

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
                TrySetTextLabel(state, state.footerLabel, BuildFooterText(), false);

            std::size_t firstVisibleIndex = 0u;
            if (hasSelection && (selectedOption.optionCount > kVisibleRowCount)) {
                const std::size_t centerOffset = kVisibleRowCount / 2u;
                if (selectedOption.flatIndex > centerOffset) {
                    firstVisibleIndex = selectedOption.flatIndex - centerOffset;
                }

                const std::size_t maxFirstVisibleIndex = selectedOption.optionCount - kVisibleRowCount;
                firstVisibleIndex = (std::min)(firstVisibleIndex, maxFirstVisibleIndex);
            }

            for (std::size_t rowIndex = 0u; rowIndex < kVisibleRowCount; ++rowIndex) {
                if (!hasSelection) {
                    HideAllRowControls(state, rowIndex);
                    continue;
                }

                const std::size_t flatIndex = firstVisibleIndex + rowIndex;
                if (flatIndex >= selectedOption.optionCount) {
                    HideAllRowControls(state, rowIndex);
                    continue;
                }

                SelectedOptionRef rowOption = {};
                if (TryGetOptionByFlatIndex(*state.catalog, flatIndex, rowOption)) {
                    UpdateRowForOption(state, rowIndex, rowOption);
                }
                else {
                    HideAllRowControls(state, rowIndex);
                }
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

        bool TryApplyEnumRowSelectionChange(State& state, std::size_t rowIndex, long nativeSelectedIndex) {
            if ((nativeSelectedIndex < 0) || (state.catalog == nullptr)) {
                return false;
            }

            SelectedOptionRef rowOption = {};
            if (!TryGetVisibleRowOption(state, rowIndex, rowOption) ||
                (rowOption.modEntry == nullptr) ||
                (rowOption.optionEntry == nullptr)) {
                return false;
            }

            OptionEntry& optionEntry = *rowOption.optionEntry;
            long selectedChoiceIndex = -1;
            if ((optionEntry.type != CoHModSDKConfigType_Enum) ||
                !TryMapNativeListIndexToEnumChoiceIndex(optionEntry, nativeSelectedIndex, selectedChoiceIndex)) {
                return false;
            }

            const ChoiceEntry& selectedChoice = optionEntry.choices[static_cast<std::size_t>(selectedChoiceIndex)];
            if (selectedChoice.value == optionEntry.currentValue.enumValue) {
                return false;
            }

            const ModSDK::Config::Value newValue = ModSDK::Config::MakeEnumValue(selectedChoice.value);
            if (!ModSDK::Config::SetValue(rowOption.modEntry->modId.c_str(), optionEntry.optionId.c_str(), newValue)) {
                LogWarning(
                    "CoH Mod Config UI failed to update enum value for " +
                    rowOption.modEntry->modId +
                    "." +
                    optionEntry.optionId +
                    " via native ComboBox selection."
                );
                return false;
            }

            state.selectedOptionIndex = rowOption.flatIndex;
            optionEntry.currentValue = newValue;
            LogInfo(
                "CoH Mod Config UI: Native ComboBox selection changed for row " +
                std::to_string(rowIndex) +
                " to choice " +
                std::to_string(selectedChoiceIndex) +
                " from native index " +
                std::to_string(nativeSelectedIndex) +
                " ('" +
                BuildChoiceDisplayText(selectedChoice) +
                "')."
            );
            RefreshVisibleMenu(state);
            return true;
        }

        std::size_t ComputeFirstVisibleIndex(State& state) {
            SelectedOptionRef selectedOption = {};
            if (!TryGetSelectedOption(state, selectedOption) || (selectedOption.optionCount <= kVisibleRowCount)) {
                return 0u;
            }

            const std::size_t centerOffset = kVisibleRowCount / 2u;
            std::size_t firstVisibleIndex = 0u;
            if (selectedOption.flatIndex > centerOffset) {
                firstVisibleIndex = selectedOption.flatIndex - centerOffset;
            }

            const std::size_t maxFirstVisibleIndex = selectedOption.optionCount - kVisibleRowCount;
            return (std::min)(firstVisibleIndex, maxFirstVisibleIndex);
        }

        void OnRowControlClicked(State& state, std::size_t rowIndex) {
            if (!state.overlayVisible || (state.catalog == nullptr)) {
                return;
            }

            SelectedOptionRef clickedOption = {};
            if (!TryGetVisibleRowOption(state, rowIndex, clickedOption) || (clickedOption.optionEntry == nullptr)) {
                return;
            }

            state.selectedOptionIndex = clickedOption.flatIndex;
            if (clickedOption.optionEntry->type == CoHModSDKConfigType_Enum) {
                LogInfo("CoH Mod Config UI: Native ComboBox button clicked for row " + std::to_string(rowIndex) + ".");
                return;
            }

            AdjustSelectedValue(state, 1);
            RefreshVisibleMenu(state);
        }

        bool PollWidgetActiveEdge(State& state, void* proxyWidget, bool& wasActive) {
            if ((state.widgetGetState == nullptr) || (state.widgetProxyGetWidget == nullptr)) {
                return false;
            }

            void* rawWidget = state.widgetProxyGetWidget(proxyWidget);
            if (rawWidget == nullptr) {
                return false;
            }

            const bool isActive = state.widgetGetState(rawWidget, kWidgetStateActive);
            const bool released = !isActive && wasActive;
            wasActive = isActive;
            return released;
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

            if (!state.overlayVisible) {
                return;
            }

            for (std::size_t i = 0u; i < kVisibleRowCount; ++i) {
                // Poll all widget types — only the visible one will have Active state.
                if (PollWidgetActiveEdge(state, state.rowArrowButtons[i].Get(), state.rowArrowButtonWasActive[i])) {
                    OnRowControlClicked(state, i);
                }
                if (PollWidgetActiveEdge(state, state.rowCheckButtons[i].Get(), state.rowCheckButtonWasActive[i])) {
                    OnRowControlClicked(state, i);
                }
                if (PollWidgetActiveEdge(state, state.rowProgressBars[i].Get(), state.rowProgressBarWasActive[i])) {
                    OnRowControlClicked(state, i);
                }
                if ((state.rowActiveControlType[i] == CoHModSDKConfigType_Enum) && (state.rowListBoxWidgets[i] != nullptr)) {
                    long selectedIndex = -1;
                    if (TryGetCustomListBoxSelectedIndex(state, state.rowListBoxWidgets[i], selectedIndex)) {
                        if (!state.rowHasObservedListBoxSelection[i]) {
                            state.rowObservedListBoxSelection[i] = selectedIndex;
                            state.rowHasObservedListBoxSelection[i] = true;
                        }
                        else if (selectedIndex != state.rowObservedListBoxSelection[i]) {
                            state.rowObservedListBoxSelection[i] = selectedIndex;
                            if (TryApplyEnumRowSelectionChange(state, i, selectedIndex)) {
                                return;
                            }
                        }
                    }
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
        state.fileOverrideRegistered = false;
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

            state.fileOverrideRegistered = false;
        }

        state.catalog = nullptr;
        state.installed = false;
        state.overlayBuilt = false;
        state.overlayVisible = false;
    }
}
