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
        constexpr char kGameExecutableModuleName[] = "RelicCOH.exe";

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
        constexpr std::size_t kOpaqueNativeSliderStorageSize = 1024u;
        constexpr std::size_t kVisibleRowCount = 7u;
        constexpr char kGroupWidgetTypeName[] = "Group";
        constexpr char kArtLabelWidgetTypeName[] = "ArtLabel";
        constexpr char kComboBoxWidgetTypeName[] = "ComboBox";
        constexpr char kCheckButtonWidgetTypeName[] = "CheckButton";
        constexpr char kTextLabelWidgetTypeName[] = "TextLabel";
        constexpr char kScrollBarWidgetTypeName[] = "ScrollBar";
        constexpr char kScreenName[] = "cohmodconfigui";
        constexpr char kTemplateScreenName[] = "prompt_performance_test";
        constexpr char kTemplatePanelWidgetName[] = "perfGrp";
        constexpr char kTemplateLabelWidgetName[] = "minimumResults";
        constexpr char kRootWidgetName[] = "cohmodconfigui_root";
        constexpr char kPanelButtonName[] = "cohmodconfigui_panel";
        constexpr char kTitleLabelName[] = "cohmodconfigui_title";
        constexpr char kPanelScrollBarName[] = "scrlBar_cohmodconfigui_panel";
        constexpr char kPanelScrollBarTrackVisualName[] = "cohmodconfigui_panelscrolltrack";
        constexpr char kPanelScrollBarThumbVisualName[] = "cohmodconfigui_panelscrollthumb";
        constexpr char kModSelectorComboBoxName[] = "drop_cohmodconfigui_mod";
        constexpr char kSummaryLabelName[] = "cohmodconfigui_summary";
        constexpr char kRowLabelNamePrefix[] = "cohmodconfigui_rowlabel_";
        constexpr char kRowButtonNamePrefix[] = "cohmodconfigui_row_";
        constexpr char kRowCheckButtonNamePrefix[] = "cohmodconfigui_rowcheck_";
        constexpr char kRowSliderNamePrefix[] = "cohmodconfigui_rowslider_";
        // Keep the options-menu radio-button donor recorded for later investigation.
        constexpr char kReferenceRadioButtonDonorScreenName[] = "optionsmenu";
        constexpr char kReferenceRadioButtonDonorWidgetName[] = "rdo_graphics_custom";
        constexpr char kCheckButtonDonorScreenName[] = "messageboxpopup2";
        constexpr char kCheckButtonDonorWidgetName[] = "checkbutton_ShowOnce";
        // Keep the skirmish ready donor recorded as a fallback reference.
        constexpr char kReferenceCheckButtonDonorScreenName[] = "skirmishmissionsetup";
        constexpr char kReferenceCheckButtonDonorWidgetName[] = "btnReady";
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
        constexpr float kModSelectorPositionX = 0.05f;
        constexpr float kModSelectorPositionY = 0.095f;
        constexpr float kModSelectorSizeX = 0.35f;
        constexpr float kModSelectorSizeY = 0.035f;
        constexpr float kModSelectorButtonSizeX = 0.025f;
        constexpr float kModSelectorButtonOffsetX = kModSelectorSizeX - kModSelectorButtonSizeX;
        constexpr float kModSelectorLabelSizeX = kModSelectorSizeX - kModSelectorButtonSizeX;
        constexpr float kModSelectorListBoxPositionX = 0.0f;
        constexpr float kModSelectorListBoxPositionY = kModSelectorSizeY;
        constexpr float kModSelectorListBoxSizeX = kModSelectorSizeX;
        constexpr float kModSelectorListBoxSizeY = kModSelectorSizeY * 4.0f;
        constexpr float kModSelectorListBoxScrollBarSizeX = 0.015f;
        constexpr float kModSelectorListBoxContentSizeX = kModSelectorListBoxSizeX - kModSelectorListBoxScrollBarSizeX;
        constexpr float kSummaryPositionX = 0.05f;
        constexpr float kSummaryPositionY = 0.145f;
        constexpr float kSummarySizeX = 0.90f;
        constexpr float kSummarySizeY = 0.06f;
        constexpr float kFirstRowPositionX = 0.03f;
        constexpr float kFirstRowPositionY = 0.165f;
        constexpr float kRowSpacingY = 0.06f;
        constexpr float kRowLabelSizeX = 0.20f;
        constexpr float kRowLabelSizeY = 0.035f;
        constexpr float kRowControlPositionX = 0.24f;
        constexpr float kPanelScrollBarPositionX = 0.910f;
        constexpr float kPanelScrollBarPositionY = 0.12f;
        constexpr float kPanelScrollBarSizeX = 0.020f;
        constexpr float kPanelScrollBarSizeY = 0.74f;
        constexpr float kPanelScrollBarMinThumbSizeY = 0.04f;
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
        constexpr float kRowCheckButtonSizeX = 0.18750f;
        constexpr float kRowCheckButtonSizeY = 0.04167f;
        constexpr float kRowSliderSizeX = 0.14858f;
        constexpr float kRowSliderSizeY = 0.05416f;
        constexpr float kRowSliderOffsetY = (kRowLabelSizeY - kRowSliderSizeY) * 0.5f;
        constexpr float kRowSliderButtonSizeX = 0.01635f;
        constexpr float kRowSliderButtonSizeY = 0.04524f;
        constexpr float kRowSliderButtonPositionY = 0.00519f;
        constexpr float kRowSliderButtonMinPositionX = 0.0f;
        constexpr float kRowSliderButtonMaxPositionX = kRowSliderSizeX - kRowSliderButtonSizeX;
        constexpr std::uintptr_t kNativeSliderBindInputRva = 0x0056D1C0u;
        constexpr std::size_t kNativeSliderKnobProxyOffset = 0x110u;
        constexpr std::size_t kNativeSliderCallbackOffset = 0x1E0u;
        constexpr std::size_t kNativeSliderCurrentValueOffset = 0x200u;
        constexpr std::size_t kNativeSliderEnabledOffset = 0x204u;
        constexpr std::size_t kNativeSliderMinValueOffset = 0x208u;
        constexpr std::size_t kNativeSliderMaxValueOffset = 0x20Cu;
        constexpr float kSliderProgressEpsilon = 0.0005f;

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
        using CustomWidgetCtorFn = void(__thiscall*)(void* customWidget);
        using CustomWidgetDtorFn = void(__thiscall*)(void* customWidget);
        using ArtLabelCtorFn = void(__thiscall*)(void* artLabel);
        using ArtLabelDtorFn = void(__thiscall*)(void* artLabel);
        using ArtLabelSetAllArtVisibleFn = void(__thiscall*)(void* artLabel, bool visible);
        using ButtonCtorFn = void(__thiscall*)(void* button);
        using ButtonDtorFn = void(__thiscall*)(void* button);
        using ButtonSetTextFn = void(__thiscall*)(void* button, const void* locString);
        using CheckButtonCtorFn = void(__thiscall*)(void* checkButton);
        using CheckButtonDtorFn = void(__thiscall*)(void* checkButton);
        using CheckButtonSetCheckedFn = void(__thiscall*)(void* checkButton, bool checked);
        using CheckButtonGetCheckedFn = bool(__thiscall*)(const void* checkButton);
        using CheckButtonSetTextFn = void(__thiscall*)(void* checkButton, const void* locString);
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
        using CustomListBoxGetScrollPositionFn = float(__thiscall*)(const void* customListBox);
        using CustomListBoxGetScrollRangeFn = const void* (__thiscall*)(const void* customListBox);
        using CustomListBoxSetScrollPositionFn = void(__thiscall*)(void* customListBox, float scrollPosition);
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

        struct OpaqueTextLabel {
            alignas(16) std::array<std::byte, kOpaqueTextLabelStorageSize> storage = {};
            void* Get() { return storage.data(); }
            const void* Get() const { return storage.data(); }
        };

        struct OpaqueArtLabel {
            alignas(16) std::array<std::byte, kOpaqueButtonStorageSize> storage = {};
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

        struct NativeSliderCallback {
            void* object = nullptr;
            void* function = nullptr;
        };

        struct OpaqueNativeSlider {
            alignas(16) std::array<std::byte, kOpaqueNativeSliderStorageSize> storage = {};

            void* Get() { return storage.data(); }
            const void* Get() const { return storage.data(); }

            void* GetKnobProxy() {
                return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(storage.data()) + kNativeSliderKnobProxyOffset);
            }

            const void* GetKnobProxy() const {
                return reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(storage.data()) + kNativeSliderKnobProxyOffset);
            }

            NativeSliderCallback& Callback() {
                return *reinterpret_cast<NativeSliderCallback*>(reinterpret_cast<std::uintptr_t>(storage.data()) + kNativeSliderCallbackOffset);
            }

            float& CurrentValue() {
                return *reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(storage.data()) + kNativeSliderCurrentValueOffset);
            }

            std::uint8_t& EnabledFlag() {
                return *reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uintptr_t>(storage.data()) + kNativeSliderEnabledOffset);
            }

            float& MinValue() {
                return *reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(storage.data()) + kNativeSliderMinValueOffset);
            }

            float& MaxValue() {
                return *reinterpret_cast<float*>(reinterpret_cast<std::uintptr_t>(storage.data()) + kNativeSliderMaxValueOffset);
            }
        };

        struct State {
            Catalog* catalog = nullptr;
            bool installed = false;
            bool overlayBuilt = false;
            bool overlayVisible = false;
            std::size_t selectedModIndex = 0u;
            std::size_t selectedOptionIndex = 0u;
            std::size_t topVisibleOptionIndex = 0u;
            bool modSelectorDropDownOpen = false;
            long activeEnumDropDownRowIndex = -1;
            int toggleKey = 0;
            void* screenManagerUpdateTarget = nullptr;
            HWND gameWindowHandle = nullptr;
            WNDPROC originalGameWindowProc = nullptr;
            LONG pendingMouseWheelDelta = 0;
            bool hasPendingLeftClick = false;
            POINT pendingLeftClickClientPosition = {};
            bool hasPendingMouseMove = false;
            POINT pendingMouseMoveClientPosition = {};
            bool panelScrollBarDragging = false;
            float panelScrollBarDragOffsetY = 0.0f;
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
            CustomWidgetCtorFn customWidgetCtor = nullptr;
            CustomWidgetDtorFn customWidgetDtor = nullptr;
            ArtLabelCtorFn artLabelCtor = nullptr;
            ArtLabelDtorFn artLabelDtor = nullptr;
            ArtLabelSetAllArtVisibleFn artLabelSetAllArtVisible = nullptr;
            ButtonCtorFn buttonCtor = nullptr;
            ButtonDtorFn buttonDtor = nullptr;
            ButtonSetTextFn buttonSetText = nullptr;
            CheckButtonCtorFn checkButtonCtor = nullptr;
            CheckButtonDtorFn checkButtonDtor = nullptr;
            CheckButtonSetCheckedFn checkButtonSetChecked = nullptr;
            CheckButtonGetCheckedFn checkButtonGetChecked = nullptr;
            CheckButtonSetTextFn checkButtonSetText = nullptr;
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
            CustomListBoxGetScrollPositionFn customListBoxGetScrollPosition = nullptr;
            CustomListBoxGetScrollRangeFn customListBoxGetScrollRange = nullptr;
            CustomListBoxSetScrollPositionFn customListBoxSetScrollPosition = nullptr;
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
            void* nativeSliderBindInputAddress = nullptr;
            bool fileOverrideRegistered = false;
            bool updateHookObserved = false;
            bool toggleKeyWasDown = false;
            bool toggleInputObserved = false;
            void* screen = nullptr;
            void* rootWidgetRaw = nullptr;
            void* panelWidgetRaw = nullptr;
            void* titleLabelRaw = nullptr;
            void* panelScrollBarWidget = nullptr;
            void* panelScrollBarTrackVisualWidget = nullptr;
            void* panelScrollBarThumbVisualWidget = nullptr;
            void* panelScrollBarDecButtonWidget = nullptr;
            void* panelScrollBarIncButtonWidget = nullptr;
            void* panelScrollBarTrackButtonWidget = nullptr;
            void* panelScrollBarPageDownButtonWidget = nullptr;
            void* panelScrollBarPageUpButtonWidget = nullptr;
            void* modSelectorComboBoxWidget = nullptr;
            void* modSelectorListBoxWidget = nullptr;
            void* modSelectorValueLabelWidget = nullptr;
            void* modSelectorArrowButtonWidget = nullptr;
            void* summaryLabelRaw = nullptr;
            std::array<void*, kVisibleRowCount> rowLabelWidgets = {};
            // Enum widgets
            std::array<void*, kVisibleRowCount> rowComboBoxWidgets = {};
            std::array<void*, kVisibleRowCount> rowListBoxWidgets = {};
            std::array<void*, kVisibleRowCount> rowValueLabelWidgets = {};
            std::array<void*, kVisibleRowCount> rowArrowButtonWidgets = {};
            // Bool widgets
            std::array<void*, kVisibleRowCount> rowCheckButtonWidgets = {};
            // Int/Float slider widgets
            std::array<void*, kVisibleRowCount> rowSliderWidgets = {};
            std::array<void*, kVisibleRowCount> rowSliderButtonWidgets = {};
            std::array<void*, kVisibleRowCount> rowSliderBarWidgets = {};
            OpaqueTextLabel titleLabel = {};
            OpaqueTextLabel modSelectorValueLabel = {};
            OpaqueTextLabel summaryLabel = {};
            OpaqueButton modSelectorButton = {};
            std::array<OpaqueTextLabel, kVisibleRowCount> rowLabels = {};
            // Enum proxies
            std::array<OpaqueTextLabel, kVisibleRowCount> rowValueLabels = {};
            std::array<OpaqueButton, kVisibleRowCount> rowArrowButtons = {};
            // Bool proxies
            std::array<OpaqueCheckButton, kVisibleRowCount> rowCheckButtons = {};
            // Int/Float slider proxies
            std::array<OpaqueGenericWidget, kVisibleRowCount> rowSliders = {};
            std::array<OpaqueNativeSlider, kVisibleRowCount> rowNativeSliders = {};
            // State tracking
            std::array<bool, kVisibleRowCount> rowComboBoxWasActive = {};
            std::array<bool, kVisibleRowCount> rowValueLabelWasActive = {};
            std::array<bool, kVisibleRowCount> rowArrowButtonWasActive = {};
            std::array<bool, kVisibleRowCount> rowCheckButtonWasActive = {};
            std::array<bool, kVisibleRowCount> rowNativeSliderInitialized = {};
            std::array<float, kVisibleRowCount> rowObservedSliderProgress = {};
            std::array<bool, kVisibleRowCount> rowHasObservedSliderProgress = {};
            std::array<long, kVisibleRowCount> rowObservedListBoxSelection = {};
            std::array<bool, kVisibleRowCount> rowHasObservedListBoxSelection = {};
            bool modSelectorComboBoxWasActive = false;
            bool modSelectorValueLabelWasActive = false;
            bool modSelectorArrowButtonWasActive = false;
            bool panelScrollBarPageDownWasActive = false;
            bool panelScrollBarPageUpWasActive = false;
            long observedModListSelection = -1;
            bool hasObservedModListSelection = false;
            std::array<CoHModSDKConfigType, kVisibleRowCount> rowActiveControlType = {};
        };

        struct FindProcessWindowContext {
            DWORD processId = 0u;
            HWND window = nullptr;
        };

        struct SelectedOptionRef {
            std::size_t modIndex = 0u;
            std::size_t flatIndex = 0u;
            std::size_t optionCount = 0u;
            ModEntry* modEntry = nullptr;
            OptionEntry* optionEntry = nullptr;
        };

        State& GetState() {
            static State state;
            return state;
        }

        BOOL CALLBACK FindProcessWindowProc(HWND hwnd, LPARAM lParam) {
            if ((lParam == 0) || !IsWindowVisible(hwnd) || (GetWindow(hwnd, GW_OWNER) != nullptr)) {
                return TRUE;
            }

            auto* context = reinterpret_cast<FindProcessWindowContext*>(lParam);
            DWORD windowProcessId = 0u;
            GetWindowThreadProcessId(hwnd, &windowProcessId);
            if (windowProcessId != context->processId) {
                return TRUE;
            }

            context->window = hwnd;
            return FALSE;
        }

        HWND FindGameWindowHandle() {
            const DWORD currentProcessId = GetCurrentProcessId();
            HWND foregroundWindow = GetForegroundWindow();
            if (foregroundWindow != nullptr) {
                DWORD foregroundProcessId = 0u;
                GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);
                if (foregroundProcessId == currentProcessId) {
                    return foregroundWindow;
                }
            }

            FindProcessWindowContext context = {};
            context.processId = currentProcessId;
            EnumWindows(&FindProcessWindowProc, reinterpret_cast<LPARAM>(&context));
            return context.window;
        }

        bool IsPointInsideOpenDropDown(State& state, HWND hwnd, const POINT& clientPoint);
        bool TryMarkDropDownOpenFromClick(State& state, HWND hwnd, const POINT& clientPoint);
        bool TryScrollOpenDropDown(State& state, int direction);

        LRESULT CALLBACK HookedGameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
            State& state = GetState();

            if ((message == WM_MOUSEWHEEL) && state.overlayVisible) {
                const SHORT wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (wheelDelta != 0) {
                    state.pendingMouseWheelDelta += static_cast<LONG>(wheelDelta);
                    return 0;
                }
            }
            else if ((message == WM_LBUTTONDOWN) && state.overlayVisible) {
                POINT clientPoint = {};
                clientPoint.x = static_cast<LONG>(static_cast<SHORT>(LOWORD(lParam)));
                clientPoint.y = static_cast<LONG>(static_cast<SHORT>(HIWORD(lParam)));
                if (!TryMarkDropDownOpenFromClick(state, hwnd, clientPoint) &&
                    (state.modSelectorDropDownOpen || (state.activeEnumDropDownRowIndex >= 0)) &&
                    !IsPointInsideOpenDropDown(state, hwnd, clientPoint)) {
                    state.modSelectorDropDownOpen = false;
                    state.activeEnumDropDownRowIndex = -1;
                }

                state.pendingLeftClickClientPosition.x = static_cast<LONG>(static_cast<SHORT>(LOWORD(lParam)));
                state.pendingLeftClickClientPosition.y = static_cast<LONG>(static_cast<SHORT>(HIWORD(lParam)));
                state.hasPendingLeftClick = true;
            }
            else if ((message == WM_MOUSEMOVE) && state.overlayVisible && state.panelScrollBarDragging) {
                state.pendingMouseMoveClientPosition.x = static_cast<LONG>(static_cast<SHORT>(LOWORD(lParam)));
                state.pendingMouseMoveClientPosition.y = static_cast<LONG>(static_cast<SHORT>(HIWORD(lParam)));
                state.hasPendingMouseMove = true;
            }
            else if ((message == WM_LBUTTONUP) && state.overlayVisible) {
                state.panelScrollBarDragging = false;
                state.hasPendingMouseMove = false;
            }

            if (state.originalGameWindowProc != nullptr) {
                return CallWindowProc(state.originalGameWindowProc, hwnd, message, wParam, lParam);
            }

            return DefWindowProc(hwnd, message, wParam, lParam);
        }

        bool InstallGameWindowHook(State& state) {
            if ((state.gameWindowHandle != nullptr) && (state.originalGameWindowProc != nullptr)) {
                return true;
            }

            HWND gameWindowHandle = FindGameWindowHandle();
            if (gameWindowHandle == nullptr) {
                return false;
            }

            SetLastError(0);
            auto previousWindowProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtr(gameWindowHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HookedGameWindowProc))
            );
            if ((previousWindowProc == nullptr) && (GetLastError() != 0)) {
                return false;
            }

            state.gameWindowHandle = gameWindowHandle;
            state.originalGameWindowProc = previousWindowProc;
            state.pendingMouseWheelDelta = 0;
            return true;
        }

        bool RefreshVisibleMenu(State& state);
        std::size_t ComputeFirstVisibleIndex(State& state);
        bool TryGetVisibleRowOption(State& state, std::size_t rowIndex, SelectedOptionRef& outSelectedOption);
        float ComputeListBoxHeightFromItemCount(std::size_t itemCount, float itemHeight);
        float ComputeRowListBoxHeight(const OptionEntry& optionEntry);
#if defined(_M_IX86)
        void __cdecl CallWithEaxContext0(void* eaxContext, void* targetFn);
#endif

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

        bool TryGetSelectedMod(State& state, ModEntry*& outModEntry, std::size_t& outModIndex) {
            outModEntry = nullptr;
            outModIndex = 0u;
            if (state.catalog == nullptr) {
                return false;
            }

            std::vector<ModEntry>& mods = state.catalog->GetMods();
            if (mods.empty()) {
                state.selectedModIndex = 0u;
                state.selectedOptionIndex = 0u;
                state.topVisibleOptionIndex = 0u;
                return false;
            }

            if (state.selectedModIndex >= mods.size()) {
                state.selectedModIndex = mods.size() - 1u;
            }

            outModIndex = state.selectedModIndex;
            outModEntry = &mods[outModIndex];
            return true;
        }

        bool TryGetOptionByIndexInMod(ModEntry& modEntry, std::size_t modIndex, std::size_t optionIndex, SelectedOptionRef& outSelectedOption) {
            outSelectedOption = {};
            outSelectedOption.modIndex = modIndex;
            outSelectedOption.optionCount = modEntry.options.size();
            if (optionIndex >= outSelectedOption.optionCount) {
                return false;
            }

            outSelectedOption.flatIndex = optionIndex;
            outSelectedOption.modEntry = &modEntry;
            outSelectedOption.optionEntry = &modEntry.options[optionIndex];
            return true;
        }

        bool TryGetSelectedOption(State& state, SelectedOptionRef& outSelectedOption) {
            ModEntry* modEntry = nullptr;
            std::size_t modIndex = 0u;
            if (!TryGetSelectedMod(state, modEntry, modIndex) || (modEntry == nullptr)) {
                return false;
            }

            outSelectedOption = {};
            outSelectedOption.modIndex = modIndex;
            outSelectedOption.optionCount = modEntry->options.size();
            if (outSelectedOption.optionCount == 0u) {
                state.selectedOptionIndex = 0u;
                state.topVisibleOptionIndex = 0u;
                outSelectedOption.modEntry = modEntry;
                return false;
            }

            if (state.selectedOptionIndex >= outSelectedOption.optionCount) {
                state.selectedOptionIndex = outSelectedOption.optionCount - 1u;
            }

            return TryGetOptionByIndexInMod(*modEntry, modIndex, state.selectedOptionIndex, outSelectedOption);
        }

        std::size_t ComputeMaxFirstVisibleIndex(State& state) {
            SelectedOptionRef selectedOption = {};
            if (!TryGetSelectedOption(state, selectedOption) || (selectedOption.optionCount <= kVisibleRowCount)) {
                return 0u;
            }

            return selectedOption.optionCount - kVisibleRowCount;
        }

        void ClampTopVisibleOptionIndex(State& state) {
            const std::size_t maxFirstVisibleIndex = ComputeMaxFirstVisibleIndex(state);
            if (state.topVisibleOptionIndex > maxFirstVisibleIndex) {
                state.topVisibleOptionIndex = maxFirstVisibleIndex;
            }
        }

        void EnsureSelectedOptionVisible(State& state) {
            SelectedOptionRef selectedOption = {};
            if (!TryGetSelectedOption(state, selectedOption) || (selectedOption.optionCount <= kVisibleRowCount)) {
                state.topVisibleOptionIndex = 0u;
                return;
            }

            ClampTopVisibleOptionIndex(state);
            if (state.selectedOptionIndex < state.topVisibleOptionIndex) {
                state.topVisibleOptionIndex = state.selectedOptionIndex;
            }
            else if (state.selectedOptionIndex >= (state.topVisibleOptionIndex + kVisibleRowCount)) {
                state.topVisibleOptionIndex = state.selectedOptionIndex - kVisibleRowCount + 1u;
            }

            ClampTopVisibleOptionIndex(state);
        }

        bool TrySetOptionWindowTop(State& state, std::size_t newTopVisibleIndex) {
            SelectedOptionRef selectedOption = {};
            if (!TryGetSelectedOption(state, selectedOption) || (selectedOption.optionCount <= kVisibleRowCount)) {
                return false;
            }

            const std::size_t maxFirstVisibleIndex = selectedOption.optionCount - kVisibleRowCount;
            newTopVisibleIndex = (std::min)(newTopVisibleIndex, maxFirstVisibleIndex);
            if (newTopVisibleIndex == state.topVisibleOptionIndex) {
                return false;
            }

            state.topVisibleOptionIndex = newTopVisibleIndex;
            state.selectedOptionIndex = newTopVisibleIndex;
            LogInfo(
                "CoH Mod Config UI: Scrolled option window to first visible index " +
                std::to_string(state.topVisibleOptionIndex) +
                "."
            );
            RefreshVisibleMenu(state);
            return true;
        }

        bool TryScrollOptionWindow(State& state, int deltaRows) {
            if (deltaRows == 0) {
                return false;
            }

            SelectedOptionRef selectedOption = {};
            if (!TryGetSelectedOption(state, selectedOption) || (selectedOption.optionCount <= kVisibleRowCount)) {
                LogInfo(
                    "CoH Mod Config UI: Ignored scroll request with delta " +
                    std::to_string(deltaRows) +
                    " because the current mod does not overflow the visible rows."
                );
                return false;
            }

            const long maxFirstVisibleIndex = static_cast<long>(selectedOption.optionCount - kVisibleRowCount);
            const long currentTopVisibleIndex = static_cast<long>(state.topVisibleOptionIndex);
            const long nextTopVisibleIndex = std::clamp(currentTopVisibleIndex + static_cast<long>(deltaRows), 0l, maxFirstVisibleIndex);
            if (nextTopVisibleIndex == currentTopVisibleIndex) {
                LogInfo(
                    "CoH Mod Config UI: Ignored scroll request with delta " +
                    std::to_string(deltaRows) +
                    " because the option window is already at the boundary (top=" +
                    std::to_string(state.topVisibleOptionIndex) +
                    ")."
                );
                return false;
            }

            return TrySetOptionWindowTop(state, static_cast<std::size_t>(nextTopVisibleIndex));
        }

        bool IsPointInsideRect(float x, float y, float rectX, float rectY, float rectWidth, float rectHeight) {
            return
                (x >= rectX) &&
                (x <= (rectX + rectWidth)) &&
                (y >= rectY) &&
                (y <= (rectY + rectHeight));
        }

        bool IsPointInsideOpenDropDown(State& state, HWND hwnd, const POINT& clientPoint) {
            if ((hwnd == nullptr) || !IsWindow(hwnd)) {
                return false;
            }

            if (!state.modSelectorDropDownOpen && (state.activeEnumDropDownRowIndex < 0)) {
                return false;
            }

            RECT clientRect = {};
            if (!GetClientRect(hwnd, &clientRect)) {
                return false;
            }

            const LONG clientWidth = clientRect.right - clientRect.left;
            const LONG clientHeight = clientRect.bottom - clientRect.top;
            if ((clientWidth <= 0) || (clientHeight <= 0)) {
                return false;
            }

            const float mouseX = static_cast<float>(clientPoint.x) / static_cast<float>(clientWidth);
            const float mouseY = static_cast<float>(clientPoint.y) / static_cast<float>(clientHeight);

            if (state.modSelectorDropDownOpen && (state.catalog != nullptr)) {
                const std::size_t modCount = state.catalog->GetModCount();
                const bool needsScrollBar = modCount > 4u;
                const float listBoxSizeX = needsScrollBar ? kModSelectorListBoxSizeX : kModSelectorListBoxContentSizeX;
                const float listBoxSizeY = ComputeListBoxHeightFromItemCount(modCount, kModSelectorSizeY);
                const float rectX = kPanelPositionX + (kModSelectorPositionX * kPanelSizeX);
                const float rectY = kPanelPositionY + ((kModSelectorPositionY + kModSelectorSizeY) * kPanelSizeY);
                const float rectWidth = listBoxSizeX * kPanelSizeX;
                const float rectHeight = listBoxSizeY * kPanelSizeY;
                if (IsPointInsideRect(mouseX, mouseY, rectX, rectY, rectWidth, rectHeight)) {
                    return true;
                }
            }

            if (state.activeEnumDropDownRowIndex >= 0) {
                SelectedOptionRef rowOption = {};
                if (TryGetVisibleRowOption(state, static_cast<std::size_t>(state.activeEnumDropDownRowIndex), rowOption) &&
                    (rowOption.optionEntry != nullptr) &&
                    (rowOption.optionEntry->type == CoHModSDKConfigType_Enum)) {
                    const std::size_t choiceCount = rowOption.optionEntry->choices.empty() ? 1u : rowOption.optionEntry->choices.size();
                    const bool needsScrollBar = choiceCount > 4u;
                    const float listBoxSizeX = needsScrollBar ? kRowListBoxSizeX : kRowListBoxContentSizeX;
                    const float listBoxSizeY = ComputeRowListBoxHeight(*rowOption.optionEntry);
                    const float rowTopY = kFirstRowPositionY + (static_cast<float>(state.activeEnumDropDownRowIndex) * kRowSpacingY);
                    const float rectX = kPanelPositionX + ((kRowControlPositionX + kRowListBoxPositionX) * kPanelSizeX);
                    const float rectY = kPanelPositionY + ((rowTopY + kRowListBoxPositionY) * kPanelSizeY);
                    const float rectWidth = listBoxSizeX * kPanelSizeX;
                    const float rectHeight = listBoxSizeY * kPanelSizeY;
                    if (IsPointInsideRect(mouseX, mouseY, rectX, rectY, rectWidth, rectHeight)) {
                        return true;
                    }
                }
            }

            return false;
        }

        bool TryMarkDropDownOpenFromClick(State& state, HWND hwnd, const POINT& clientPoint) {
            if ((hwnd == nullptr) || !IsWindow(hwnd)) {
                return false;
            }

            RECT clientRect = {};
            if (!GetClientRect(hwnd, &clientRect)) {
                return false;
            }

            const LONG clientWidth = clientRect.right - clientRect.left;
            const LONG clientHeight = clientRect.bottom - clientRect.top;
            if ((clientWidth <= 0) || (clientHeight <= 0)) {
                return false;
            }

            const float mouseX = static_cast<float>(clientPoint.x) / static_cast<float>(clientWidth);
            const float mouseY = static_cast<float>(clientPoint.y) / static_cast<float>(clientHeight);

            const float modSelectorRectX = kPanelPositionX + (kModSelectorPositionX * kPanelSizeX);
            const float modSelectorRectY = kPanelPositionY + (kModSelectorPositionY * kPanelSizeY);
            const float modSelectorRectWidth = kModSelectorSizeX * kPanelSizeX;
            const float modSelectorRectHeight = kModSelectorSizeY * kPanelSizeY;
            if (IsPointInsideRect(mouseX, mouseY, modSelectorRectX, modSelectorRectY, modSelectorRectWidth, modSelectorRectHeight)) {
                state.modSelectorDropDownOpen = true;
                state.activeEnumDropDownRowIndex = -1;
                LogInfo("CoH Mod Config UI: Mod selector ComboBox body clicked.");
                return true;
            }

            for (std::size_t i = 0u; i < kVisibleRowCount; ++i) {
                if (state.rowActiveControlType[i] != CoHModSDKConfigType_Enum) {
                    continue;
                }

                const float rowRectX = kPanelPositionX + (kRowControlPositionX * kPanelSizeX);
                const float rowRectY = kPanelPositionY + ((kFirstRowPositionY + (static_cast<float>(i) * kRowSpacingY)) * kPanelSizeY);
                const float rowRectWidth = kRowComboBoxSizeX * kPanelSizeX;
                const float rowRectHeight = kRowComboBoxSizeY * kPanelSizeY;
                if (IsPointInsideRect(mouseX, mouseY, rowRectX, rowRectY, rowRectWidth, rowRectHeight)) {
                    state.modSelectorDropDownOpen = false;
                    state.activeEnumDropDownRowIndex = static_cast<long>(i);
                    LogInfo("CoH Mod Config UI: Native ComboBox body clicked for row " + std::to_string(i) + ".");
                    return true;
                }
            }

            return false;
        }

        struct CustomListBoxScrollRange {
            float minValue = 0.0f;
            float maxValue = 0.0f;
        };

        bool TryAdjustCustomListBoxScroll(State& state, void* listBoxWidget, std::size_t itemCount, int direction) {
            if ((direction == 0) ||
                (itemCount <= 4u) ||
                (listBoxWidget == nullptr) ||
                (state.widgetProxyBind == nullptr) ||
                (state.customListBoxCtor == nullptr) ||
                (state.customListBoxDtor == nullptr) ||
                (state.customListBoxGetScrollPosition == nullptr) ||
                (state.customListBoxGetScrollRange == nullptr) ||
                (state.customListBoxSetScrollPosition == nullptr)) {
                return false;
            }

            OpaqueCustomListBox listBoxProxy = {};
            state.customListBoxCtor(listBoxProxy.Get());
            state.widgetProxyBind(listBoxProxy.Get(), listBoxWidget);

            const float currentPosition = state.customListBoxGetScrollPosition(listBoxProxy.Get());
            const auto* range = reinterpret_cast<const CustomListBoxScrollRange*>(state.customListBoxGetScrollRange(listBoxProxy.Get()));
            if (range == nullptr) {
                state.customListBoxDtor(listBoxProxy.Get());
                return false;
            }

            float rangeMin = range->minValue;
            float rangeMax = range->maxValue;
            if (rangeMax < rangeMin) {
                std::swap(rangeMin, rangeMax);
            }

            const std::size_t visibleCount = (std::min)(itemCount, static_cast<std::size_t>(4u));
            const std::size_t scrollStepCount = itemCount > visibleCount ? (itemCount - visibleCount) : 0u;
            if (scrollStepCount == 0u) {
                state.customListBoxDtor(listBoxProxy.Get());
                return false;
            }

            const float rangeSpan = rangeMax - rangeMin;
            const float scrollStep = (rangeSpan > 0.0f) ? (rangeSpan / static_cast<float>(scrollStepCount)) : 1.0f;
            const float newPosition = std::clamp(
                currentPosition + (direction > 0 ? scrollStep : -scrollStep),
                rangeMin,
                rangeMax
            );
            const bool changed = std::fabs(newPosition - currentPosition) > 0.0001f;
            if (changed) {
                state.customListBoxSetScrollPosition(listBoxProxy.Get(), newPosition);
            }

            state.customListBoxDtor(listBoxProxy.Get());
            return changed;
        }

        bool TryScrollOpenDropDown(State& state, int direction) {
            if (direction == 0) {
                return false;
            }

            if (state.modSelectorDropDownOpen && (state.catalog != nullptr) && (state.modSelectorListBoxWidget != nullptr)) {
                return TryAdjustCustomListBoxScroll(
                    state,
                    state.modSelectorListBoxWidget,
                    state.catalog->GetModCount(),
                    direction
                );
            }

            if (state.activeEnumDropDownRowIndex >= 0) {
                SelectedOptionRef rowOption = {};
                if (TryGetVisibleRowOption(state, static_cast<std::size_t>(state.activeEnumDropDownRowIndex), rowOption) &&
                    (rowOption.optionEntry != nullptr) &&
                    (rowOption.optionEntry->type == CoHModSDKConfigType_Enum)) {
                    return TryAdjustCustomListBoxScroll(
                        state,
                        state.rowListBoxWidgets[static_cast<std::size_t>(state.activeEnumDropDownRowIndex)],
                        rowOption.optionEntry->choices.size(),
                        direction
                    );
                }
            }

            return false;
        }

        bool TryHandlePanelScrollBarClick(State& state) {
            if (!state.hasPendingLeftClick) {
                return false;
            }

            state.hasPendingLeftClick = false;

            SelectedOptionRef selectedOption = {};
            if (!TryGetSelectedOption(state, selectedOption) || (selectedOption.optionCount <= kVisibleRowCount)) {
                return false;
            }

            if ((state.gameWindowHandle == nullptr) || !IsWindow(state.gameWindowHandle)) {
                return false;
            }

            RECT clientRect = {};
            if (!GetClientRect(state.gameWindowHandle, &clientRect)) {
                return false;
            }

            const LONG clientWidth = clientRect.right - clientRect.left;
            const LONG clientHeight = clientRect.bottom - clientRect.top;
            if ((clientWidth <= 0) || (clientHeight <= 0)) {
                return false;
            }

            const float clickX = static_cast<float>(state.pendingLeftClickClientPosition.x) / static_cast<float>(clientWidth);
            const float clickY = static_cast<float>(state.pendingLeftClickClientPosition.y) / static_cast<float>(clientHeight);
            const float trackX = kPanelPositionX + (kPanelScrollBarPositionX * kPanelSizeX);
            const float trackY = kPanelPositionY + (kPanelScrollBarPositionY * kPanelSizeY);
            const float trackWidth = kPanelScrollBarSizeX * kPanelSizeX;
            const float trackHeight = kPanelScrollBarSizeY * kPanelSizeY;

            if ((clickX < trackX) || (clickX > (trackX + trackWidth)) ||
                (clickY < trackY) || (clickY > (trackY + trackHeight))) {
                return false;
            }

            const std::size_t maxFirstVisibleIndex = selectedOption.optionCount - kVisibleRowCount;
            const float visibleFraction =
                static_cast<float>(kVisibleRowCount) /
                static_cast<float>(selectedOption.optionCount);
            const float thumbSizeLocalY = (std::min)(
                (std::max)(kPanelScrollBarMinThumbSizeY, kPanelScrollBarSizeY * visibleFraction),
                kPanelScrollBarSizeY
            );
            const float thumbSizeScreenY = thumbSizeLocalY * kPanelSizeY;
            const float thumbTravelScreenY = (std::max)(0.0f, trackHeight - thumbSizeScreenY);
            const float currentThumbProgress =
                (maxFirstVisibleIndex == 0u) ?
                0.0f :
                (static_cast<float>(state.topVisibleOptionIndex) / static_cast<float>(maxFirstVisibleIndex));
            const float currentThumbTop = thumbTravelScreenY * currentThumbProgress;

            if ((clickY >= (trackY + currentThumbTop)) && (clickY <= (trackY + currentThumbTop + thumbSizeScreenY))) {
                state.panelScrollBarDragOffsetY = clickY - (trackY + currentThumbTop);
            }
            else {
                state.panelScrollBarDragOffsetY = thumbSizeScreenY * 0.5f;
            }
            state.panelScrollBarDragging = true;

            const float targetThumbTop = clickY - trackY - state.panelScrollBarDragOffsetY;
            const float progress =
                (thumbTravelScreenY <= 0.0f) ?
                0.0f :
                std::clamp(targetThumbTop / thumbTravelScreenY, 0.0f, 1.0f);
            const std::size_t newTopVisibleIndex = static_cast<std::size_t>(
                std::lround(progress * static_cast<float>(maxFirstVisibleIndex))
            );

            LogInfo(
                "CoH Mod Config UI: Panel scrollbar click observed at normalized position (" +
                std::to_string(clickX) +
                ", " +
                std::to_string(clickY) +
                ")."
            );
            return TrySetOptionWindowTop(state, newTopVisibleIndex);
        }

        bool TryHandlePanelScrollBarDrag(State& state) {
            if (!state.panelScrollBarDragging || !state.hasPendingMouseMove) {
                return false;
            }

            state.hasPendingMouseMove = false;

            SelectedOptionRef selectedOption = {};
            if (!TryGetSelectedOption(state, selectedOption) || (selectedOption.optionCount <= kVisibleRowCount)) {
                state.panelScrollBarDragging = false;
                return false;
            }

            if ((state.gameWindowHandle == nullptr) || !IsWindow(state.gameWindowHandle)) {
                state.panelScrollBarDragging = false;
                return false;
            }

            RECT clientRect = {};
            if (!GetClientRect(state.gameWindowHandle, &clientRect)) {
                return false;
            }

            const LONG clientHeight = clientRect.bottom - clientRect.top;
            if (clientHeight <= 0) {
                return false;
            }

            const std::size_t maxFirstVisibleIndex = selectedOption.optionCount - kVisibleRowCount;
            const float trackY = kPanelPositionY + (kPanelScrollBarPositionY * kPanelSizeY);
            const float trackHeight = kPanelScrollBarSizeY * kPanelSizeY;
            const float visibleFraction =
                static_cast<float>(kVisibleRowCount) /
                static_cast<float>(selectedOption.optionCount);
            const float thumbSizeLocalY = (std::min)(
                (std::max)(kPanelScrollBarMinThumbSizeY, kPanelScrollBarSizeY * visibleFraction),
                kPanelScrollBarSizeY
            );
            const float thumbSizeScreenY = thumbSizeLocalY * kPanelSizeY;
            const float thumbTravelScreenY = (std::max)(0.0f, trackHeight - thumbSizeScreenY);
            const float mouseY = static_cast<float>(state.pendingMouseMoveClientPosition.y) / static_cast<float>(clientHeight);
            const float targetThumbTop = mouseY - trackY - state.panelScrollBarDragOffsetY;
            const float progress =
                (thumbTravelScreenY <= 0.0f) ?
                0.0f :
                std::clamp(targetThumbTop / thumbTravelScreenY, 0.0f, 1.0f);
            const std::size_t newTopVisibleIndex = static_cast<std::size_t>(
                std::lround(progress * static_cast<float>(maxFirstVisibleIndex))
            );
            return TrySetOptionWindowTop(state, newTopVisibleIndex);
        }

        bool TryGetVisibleRowOption(State& state, std::size_t rowIndex, SelectedOptionRef& outSelectedOption) {
            outSelectedOption = {};
            if ((state.catalog == nullptr) || (rowIndex >= kVisibleRowCount)) {
                return false;
            }

            ModEntry* modEntry = nullptr;
            std::size_t modIndex = 0u;
            if (!TryGetSelectedMod(state, modEntry, modIndex) || (modEntry == nullptr)) {
                return false;
            }

            const std::size_t firstVisibleIndex = ComputeFirstVisibleIndex(state);
            const std::size_t optionIndex = firstVisibleIndex + rowIndex;
            return TryGetOptionByIndexInMod(*modEntry, modIndex, optionIndex, outSelectedOption);
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

        bool TryMapVisualIndexToNativeListIndex(std::size_t itemCount, long visualIndex, long& outNativeIndex) {
            outNativeIndex = -1;
            if ((itemCount == 0u) ||
                (visualIndex < 0) ||
                (visualIndex >= static_cast<long>(itemCount))) {
                return false;
            }

            outNativeIndex = static_cast<long>(itemCount - 1u) - visualIndex;
            return true;
        }

        bool TryMapNativeListIndexToVisualIndex(std::size_t itemCount, long nativeIndex, long& outVisualIndex) {
            outVisualIndex = -1;
            if ((itemCount == 0u) ||
                (nativeIndex < 0) ||
                (nativeIndex >= static_cast<long>(itemCount))) {
                return false;
            }

            outVisualIndex = static_cast<long>(itemCount - 1u) - nativeIndex;
            return true;
        }

        bool TryMapEnumChoiceIndexToNativeListIndex(const OptionEntry& optionEntry, long choiceIndex, long& outNativeIndex) {
            if (optionEntry.type != CoHModSDKConfigType_Enum) {
                outNativeIndex = -1;
                return false;
            }

            return TryMapVisualIndexToNativeListIndex(optionEntry.choices.size(), choiceIndex, outNativeIndex);
        }

        bool TryMapNativeListIndexToEnumChoiceIndex(const OptionEntry& optionEntry, long nativeIndex, long& outChoiceIndex) {
            if (optionEntry.type != CoHModSDKConfigType_Enum) {
                outChoiceIndex = -1;
                return false;
            }

            return TryMapNativeListIndexToVisualIndex(optionEntry.choices.size(), nativeIndex, outChoiceIndex);
        }

        std::string BuildModDisplayText(const ModEntry& modEntry) {
            const std::string& sourceText = modEntry.displayName.empty() ? modEntry.modId : modEntry.displayName;
            return TruncateText(sourceText, 32u);
        }

        std::string BuildTitleText(const ModEntry* selectedMod, const SelectedOptionRef* selectedOption) {
            if (selectedMod == nullptr) {
                return "Mod Options";
            }

            const std::size_t optionCount = selectedMod->options.size();
            const std::size_t currentIndex = selectedOption == nullptr ? 0u : (selectedOption->flatIndex + 1u);
            return "Mod Options  " +
                std::to_string(currentIndex) +
                "/" +
                std::to_string(optionCount);
        }

        std::string BuildEmptyModSummaryText(const ModEntry& modEntry) {
            return "No registered options for " + TruncateText(modEntry.modId, 48u) + ".";
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
                28u
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
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??0CustomWidget@UI@@QAE@XZ", state.customWidgetCtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??1CustomWidget@UI@@UAE@XZ", state.customWidgetDtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??0ArtLabel@UI@@QAE@XZ", state.artLabelCtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??1ArtLabel@UI@@UAE@XZ", state.artLabelDtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetAllArtVisible@ArtLabel@UI@@QAEX_N@Z", state.artLabelSetAllArtVisible) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??0Button@UI@@QAE@XZ", state.buttonCtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??1Button@UI@@UAE@XZ", state.buttonDtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetText@Button@UI@@QAEXABVLocString@@@Z", state.buttonSetText) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??0CheckButton@UI@@QAE@XZ", state.checkButtonCtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "??1CheckButton@UI@@UAE@XZ", state.checkButtonDtor) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?SetChecked@CheckButton@UI@@QAEX_N@Z", state.checkButtonSetChecked) &&
                ResolveRequiredExport(userInterfaceModule, kUserInterfaceModuleName, "?GetChecked@CheckButton@UI@@QBE_NXZ", state.checkButtonGetChecked) &&
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
            HMODULE gameModule = GetModuleHandleA(kGameExecutableModuleName);
            if (gameModule == nullptr) {
                gameModule = GetModuleHandleA(nullptr);
            }
            state.nativeSliderBindInputAddress =
                gameModule == nullptr
                ? nullptr
                : reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(gameModule) + kNativeSliderBindInputRva);

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
            ResolveOptionalExport(userInterfaceModule, "?GetScrollPosition@CustomListBox@UI@@QBEMXZ", state.customListBoxGetScrollPosition);
            ResolveOptionalExport(userInterfaceModule, "?GetScrollRange@CustomListBox@UI@@QBEABVVector2f@Math@@XZ", state.customListBoxGetScrollRange);
            ResolveOptionalExport(userInterfaceModule, "?SetScrollPosition@CustomListBox@UI@@QAEXM@Z", state.customListBoxSetScrollPosition);
            ResolveOptionalExport(userInterfaceModule, "?ScrollToTop@CustomListBox@UI@@QAEXXZ", state.customListBoxScrollToTop);
            ResolveOptionalExport(userInterfaceModule, "?GetOldCustomItem@CustomListBox@UI@@QAEPAVCustomListBoxItemOld@2@XZ", state.customListBoxGetOldCustomItem);
            ResolveOptionalExport(userInterfaceModule, "?Bind@CustomListBoxItemOld@UI@@QAEXABVWidgetProxy@2@PBDJ@Z", state.customListBoxItemOldBind);
            ResolveOptionalExport(userInterfaceModule, "?SetText@CustomListBoxItemOld@UI@@QAEXABVLocString@@@Z", state.customListBoxItemOldSetText);
            ResolveOptionalExport(userInterfaceModule, "?GetStyleManager@ScreenManager@UI@@QAEPAVStyleManager@2@XZ", state.getStyleManager);
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

        std::string BuildEmptyButtonText() {
            return " ";
        }

        std::string MakeRowLabelName(std::size_t rowIndex) {
            return std::string(kRowLabelNamePrefix) + std::to_string(rowIndex);
        }

        std::string MakeRowButtonName(std::size_t rowIndex) {
            return std::string(kRowButtonNamePrefix) + std::to_string(rowIndex);
        }

        std::string MakeModSelectorComboBoxName() {
            return kModSelectorComboBoxName;
        }

        std::string MakeModSelectorLabelName() {
            return std::string("lbl_") + MakeModSelectorComboBoxName();
        }

        std::string MakeModSelectorButtonName() {
            return std::string("btn_") + MakeModSelectorComboBoxName();
        }

        std::string MakeModSelectorListBoxName() {
            return std::string("lstBox_") + MakeModSelectorComboBoxName();
        }

        std::string MakeModSelectorListBoxItemsName() {
            return std::string("items_") + MakeModSelectorListBoxName();
        }

        std::string MakeModSelectorListBoxScrollBarName() {
            return std::string("scrlBar_") + MakeModSelectorListBoxName();
        }

        std::string MakeModSelectorListBoxItemTemplateName() {
            return std::string("itemTmplt_") + MakeModSelectorListBoxName();
        }

        std::string MakeModSelectorListItemName(std::size_t modIndex) {
            return std::string("cohmodconfigui_moditem_") + std::to_string(modIndex);
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

        std::string MakeRowSliderName(std::size_t rowIndex) {
            return std::string(kRowSliderNamePrefix) + std::to_string(rowIndex);
        }

        std::string MakeRowSliderButtonName(std::size_t rowIndex) {
            return std::string("slider_bttn_") + MakeRowSliderName(rowIndex);
        }

        std::string MakeRowSliderBarName(std::size_t rowIndex) {
            return std::string("slider_bar_") + MakeRowSliderName(rowIndex);
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
            state.panelScrollBarWidget = nullptr;
            state.panelScrollBarTrackVisualWidget = nullptr;
            state.panelScrollBarThumbVisualWidget = nullptr;
            state.panelScrollBarDecButtonWidget = nullptr;
            state.panelScrollBarIncButtonWidget = nullptr;
            state.panelScrollBarTrackButtonWidget = nullptr;
            state.panelScrollBarPageDownButtonWidget = nullptr;
            state.panelScrollBarPageUpButtonWidget = nullptr;
            state.modSelectorComboBoxWidget = nullptr;
            state.modSelectorListBoxWidget = nullptr;
            state.modSelectorValueLabelWidget = nullptr;
            state.modSelectorArrowButtonWidget = nullptr;
            state.summaryLabelRaw = nullptr;
            state.rowLabelWidgets.fill(nullptr);
            state.rowComboBoxWidgets.fill(nullptr);
            state.rowListBoxWidgets.fill(nullptr);
            state.rowValueLabelWidgets.fill(nullptr);
            state.rowArrowButtonWidgets.fill(nullptr);
            state.rowCheckButtonWidgets.fill(nullptr);
            state.rowSliderWidgets.fill(nullptr);
            state.rowSliderButtonWidgets.fill(nullptr);
            state.rowSliderBarWidgets.fill(nullptr);
            state.rowComboBoxWasActive.fill(false);
            state.rowValueLabelWasActive.fill(false);
            state.rowNativeSliderInitialized.fill(false);
            state.rowObservedSliderProgress.fill(0.0f);
            state.rowHasObservedSliderProgress.fill(false);
            state.rowObservedListBoxSelection.fill(-1);
            state.rowHasObservedListBoxSelection.fill(false);
            state.rowArrowButtonWasActive.fill(false);
            state.rowCheckButtonWasActive.fill(false);
            state.modSelectorComboBoxWasActive = false;
            state.modSelectorValueLabelWasActive = false;
            state.modSelectorArrowButtonWasActive = false;
            state.modSelectorDropDownOpen = false;
            state.activeEnumDropDownRowIndex = -1;
            state.panelScrollBarPageDownWasActive = false;
            state.panelScrollBarPageUpWasActive = false;
            state.observedModListSelection = -1;
            state.hasObservedModListSelection = false;
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

        bool BindGenericWidgetProxy(State& state, OpaqueGenericWidget& genericWidget, void* rawWidget) {
            if ((rawWidget == nullptr) || (state.genericWidgetCtor == nullptr) || (state.widgetProxyBind == nullptr)) {
                return false;
            }

            state.genericWidgetCtor(genericWidget.Get());
            state.widgetProxyBind(genericWidget.Get(), rawWidget);
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

        bool BindArtLabelProxy(State& state, OpaqueArtLabel& artLabel, void* rawWidget) {
            if ((rawWidget == nullptr) || (state.artLabelCtor == nullptr) || (state.widgetProxyBind == nullptr)) {
                return false;
            }

            state.artLabelCtor(artLabel.Get());
            state.widgetProxyBind(artLabel.Get(), rawWidget);
            return true;
        }

        constexpr float PanelLocalToScreenX(float localX) {
            return kPanelPositionX + (localX * kPanelSizeX);
        }

        constexpr float PanelLocalToScreenY(float localY) {
            return kPanelPositionY + (localY * kPanelSizeY);
        }

        constexpr float PanelLocalToScreenWidth(float localWidth) {
            return localWidth * kPanelSizeX;
        }

        constexpr float PanelLocalToScreenHeight(float localHeight) {
            return localHeight * kPanelSizeY;
        }

        void ApplyWidgetProxyState(State& state, void* widgetProxy);

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

        bool ApplyRawWidgetState(State& state, void* rawWidget) {
            if ((rawWidget == nullptr) ||
                (state.genericWidgetCtor == nullptr) ||
                (state.genericWidgetDtor == nullptr) ||
                (state.widgetProxyBind == nullptr)) {
                return false;
            }

            OpaqueGenericWidget widgetProxy = {};
            state.genericWidgetCtor(widgetProxy.Get());
            state.widgetProxyBind(widgetProxy.Get(), rawWidget);
            ApplyWidgetProxyState(state, widgetProxy.Get());
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

        bool EnsureListItemDirectText(
            State& state,
            void* itemWidget,
            const std::string& contextLabel,
            const std::string& itemName,
            const std::string& displayText
        ) {
            constexpr int kTextLabelExtensionId = 7;

            if ((itemWidget == nullptr) || (state.textLabelCtor == nullptr) || (state.widgetProxyBind == nullptr)) {
                return false;
            }

            if (FindWidgetExtensionObject(state, itemWidget, kTextLabelExtensionId) == nullptr) {
                LogWarning(
                    "CoH Mod Config UI: " + contextLabel +
                    " list item '" +
                    itemName +
                    "' has no text extension for direct fallback text binding."
                );
                return false;
            }

            OpaqueTextLabel textLabel = {};
            if (!BindTextLabelProxy(state, textLabel, itemWidget)) {
                LogWarning(
                    "CoH Mod Config UI: Failed to bind direct fallback text proxy for " + contextLabel +
                    " list item '" +
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
                    "CoH Mod Config UI: Set direct fallback text on " + contextLabel +
                    " list item '" +
                    itemName +
                    "' to '" +
                    displayText +
                    "'."
                );
            } else {
                LogWarning(
                    "CoH Mod Config UI: Failed to set direct fallback text on " + contextLabel +
                    " list item '" +
                    itemName +
                    "'."
                );
            }

            return textSet;
        }

        bool TryResolveListItemTextSubItemIndex(
            State& state,
            void* itemWidget,
            const std::string& contextLabel,
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
                    "CoH Mod Config UI: " + contextLabel +
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
                "CoH Mod Config UI: " + contextLabel +
                " list item '" + itemName +
                "' exposes " + std::to_string(childCount) +
                " subitems via extension 17."
            );

            if ((childWidgets == nullptr) || (childCount <= 0) || (childCount > kMaxExpectedSubItemCount)) {
                LogWarning(
                    "CoH Mod Config UI: " + contextLabel +
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
                    "CoH Mod Config UI: " + contextLabel +
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
                "CoH Mod Config UI: " + contextLabel +
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
            const std::string contextLabel = "row " + std::to_string(rowIndex);
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
                            contextLabel,
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
                    const bool usedFallbackLabel = EnsureListItemDirectText(
                        state,
                        itemWidget,
                        contextLabel,
                        itemName,
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

        bool PopulateModListBox(State& state) {
            if ((state.catalog == nullptr) ||
                (state.modSelectorListBoxWidget == nullptr) ||
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
            state.widgetProxyBind(listBoxProxy.Get(), state.modSelectorListBoxWidget);
            LogInfo("CoH Mod Config UI: Bound CustomListBox proxy for mod selector.");

            void* const oldItemProxy = state.customListBoxGetOldCustomItem(listBoxProxy.Get());
            if (oldItemProxy == nullptr) {
                LogWarning("CoH Mod Config UI: CustomListBox returned null old-item proxy for mod selector.");
                state.customListBoxDtor(listBoxProxy.Get());
                return false;
            }

            state.customListBoxDeleteAllItems(listBoxProxy.Get());
            LogInfo("CoH Mod Config UI: Cleared existing mod selector CustomListBox items.");

            const std::vector<ModEntry>& mods = state.catalog->GetMods();
            if (!mods.empty() && (state.selectedModIndex >= mods.size())) {
                state.selectedModIndex = mods.size() - 1u;
            }
            long listItemTextSubItemIndex = 0;
            bool listItemTextSubItemIndexResolved = false;
            for (std::size_t modIndex = 0u; modIndex < mods.size(); ++modIndex) {
                const std::string itemName = MakeModSelectorListItemName(modIndex);
                const std::string displayText = BuildModDisplayText(mods[modIndex]);
                const std::wstring wideText = ToWide(displayText);
                OpaqueLocString locString = {};
                state.locStringCtor(locString.Get(), wideText.c_str());
                const long addResult = state.customListBoxAddItem(listBoxProxy.Get(), itemName.c_str(), true);
                void* const itemWidget = FindNamedWidget(state, state.modSelectorListBoxWidget, itemName.c_str());
                if (!listItemTextSubItemIndexResolved && (itemWidget != nullptr)) {
                    listItemTextSubItemIndexResolved = TryResolveListItemTextSubItemIndex(
                        state,
                        itemWidget,
                        "mod selector",
                        itemName,
                        listItemTextSubItemIndex
                    );
                }

                LogInfo(
                    "CoH Mod Config UI: Mod selector CustomListBox::AddItem added '" + itemName +
                    "' with result " + std::to_string(addResult) +
                    ", text subitem index " + std::to_string(listItemTextSubItemIndex) +
                    ", display text '" + displayText + "'."
                );

                if ((addResult >= 0) && listItemTextSubItemIndexResolved) {
                    state.customListBoxItemOldBind(
                        oldItemProxy,
                        listBoxProxy.Get(),
                        itemName.c_str(),
                        listItemTextSubItemIndex
                    );
                    state.customListBoxItemOldSetText(oldItemProxy, locString.Get());
                } else if ((addResult >= 0) && (itemWidget != nullptr)) {
                    EnsureListItemDirectText(
                        state,
                        itemWidget,
                        "mod selector",
                        itemName,
                        displayText
                    );
                }

                state.locStringDtor(locString.Get());
            }

            long selectedNativeIndex = -1;
            if (!mods.empty() &&
                !TryMapVisualIndexToNativeListIndex(mods.size(), static_cast<long>(state.selectedModIndex), selectedNativeIndex)) {
                selectedNativeIndex = static_cast<long>(state.selectedModIndex);
            }
            if (selectedNativeIndex >= 0) {
                state.customListBoxSelectItem(listBoxProxy.Get(), selectedNativeIndex);
            }
            if (state.customListBoxScrollToTop != nullptr) {
                state.customListBoxScrollToTop(listBoxProxy.Get());
            }
            state.customListBoxDtor(listBoxProxy.Get());

            LogInfo(
                "CoH Mod Config UI: Populated mod selector list box with " +
                std::to_string(mods.size()) +
                " mods; selected mod index=" +
                std::to_string(state.selectedModIndex) +
                ", native selected index=" +
                std::to_string(selectedNativeIndex) +
                "."
            );
            return true;
        }

        float ComputeListBoxHeightFromItemCount(std::size_t itemCount, float itemHeight) {
            const std::size_t effectiveItemCount = itemCount == 0u ? 1u : itemCount;
            const std::size_t visibleRowCount = (std::min)(effectiveItemCount, static_cast<std::size_t>(4u));
            return itemHeight * static_cast<float>(visibleRowCount);
        }

        float ComputeRowListBoxHeight(const OptionEntry& optionEntry) {
            return ComputeListBoxHeightFromItemCount(optionEntry.choices.size(), kRowComboBoxSizeY);
        }

        void ConfigureModSelectorListBoxGeometry(State& state) {
            if ((state.catalog == nullptr) ||
                (state.modSelectorComboBoxWidget == nullptr) ||
                (state.modSelectorListBoxWidget == nullptr)) {
                return;
            }

            const std::size_t modCount = state.catalog->GetModCount();
            const bool needsScrollBar = modCount > 4u;
            const float listBoxSizeY = ComputeListBoxHeightFromItemCount(modCount, kModSelectorSizeY);
            const float listBoxSizeX = needsScrollBar ? kModSelectorListBoxSizeX : kModSelectorListBoxContentSizeX;
            const float listContentSizeX = kModSelectorListBoxContentSizeX;
            const float scrollBarPositionX = kModSelectorListBoxContentSizeX;
            const float scrollBarSizeX = needsScrollBar ? kModSelectorListBoxScrollBarSizeX : 0.0f;

            const std::string listBoxName = MakeModSelectorListBoxName();
            const std::string itemsName = MakeModSelectorListBoxItemsName();
            const std::string scrollBarName = MakeModSelectorListBoxScrollBarName();
            const std::string itemTemplateName = MakeModSelectorListBoxItemTemplateName();

            ConfigureRawWidget(
                state,
                state.modSelectorListBoxWidget,
                listBoxName.c_str(),
                kModSelectorListBoxPositionX,
                kModSelectorListBoxPositionY,
                listBoxSizeX,
                listBoxSizeY,
                state.modSelectorComboBoxWidget
            );

            void* listItemsWidget = nullptr;
            void* listScrollBarWidget = nullptr;
            void* listItemTemplateWidget = nullptr;
            if (!ResolveListBoxChildWidgets(
                state,
                state.modSelectorListBoxWidget,
                listBoxName,
                listItemsWidget,
                listScrollBarWidget,
                listItemTemplateWidget
            )) {
                LogWarning("CoH Mod Config UI: Failed to reconfigure mod selector list box geometry.");
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
                state.modSelectorListBoxWidget
            );
            ConfigureRawWidget(
                state,
                listScrollBarWidget,
                scrollBarName.c_str(),
                scrollBarPositionX,
                0.0f,
                scrollBarSizeX,
                listBoxSizeY,
                state.modSelectorListBoxWidget
            );
            ConfigureRawWidget(
                state,
                listItemTemplateWidget,
                itemTemplateName.c_str(),
                0.0f,
                0.0f,
                listContentSizeX,
                kModSelectorSizeY,
                state.modSelectorListBoxWidget
            );

            SetRawWidgetVisible(state, listScrollBarWidget, needsScrollBar);
            LogInfo(
                "CoH Mod Config UI: Configured mod selector list box height for " +
                std::to_string(modCount) +
                " mods to " +
                std::to_string(listBoxSizeY) +
                " with scrollbar " +
                (needsScrollBar ? std::string("visible") : std::string("hidden")) +
                "."
            );
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

        void ConfigurePanelScrollBarGeometry(State& state) {
            if ((state.panelScrollBarWidget == nullptr) ||
                (state.panelScrollBarTrackButtonWidget == nullptr) ||
                (state.panelScrollBarPageDownButtonWidget == nullptr) ||
                (state.panelScrollBarPageUpButtonWidget == nullptr) ||
                (state.panelWidgetRaw == nullptr)) {
                return;
            }

            const std::string scrollBarName = kPanelScrollBarName;
            const std::string decName = std::string("btnDec_") + scrollBarName;
            const std::string incName = std::string("btnInc_") + scrollBarName;
            const std::string trackName = std::string("btnTrk_") + scrollBarName;
            const std::string pageDownName = std::string("btnPgDn_") + scrollBarName;
            const std::string pageUpName = std::string("btnPgUp_") + scrollBarName;

            ConfigureRawWidget(
                state,
                state.panelScrollBarWidget,
                scrollBarName.c_str(),
                kPanelScrollBarPositionX,
                kPanelScrollBarPositionY,
                kPanelScrollBarSizeX,
                kPanelScrollBarSizeY,
                state.panelWidgetRaw
            );

            SelectedOptionRef selectedOption = {};
            const bool needsScrollBar =
                TryGetSelectedOption(state, selectedOption) &&
                (selectedOption.optionCount > kVisibleRowCount);

            if (!needsScrollBar) {
                ConfigureRawWidget(state, state.panelScrollBarDecButtonWidget, decName.c_str(), 0.0f, 0.0f, 0.0f, 0.0f, state.panelScrollBarWidget);
                ConfigureRawWidget(state, state.panelScrollBarIncButtonWidget, incName.c_str(), 0.0f, 0.0f, 0.0f, 0.0f, state.panelScrollBarWidget);
                ConfigureRawWidget(state, state.panelScrollBarTrackButtonWidget, trackName.c_str(), 0.0f, 0.0f, 0.0f, 0.0f, state.panelScrollBarWidget);
                ConfigureRawWidget(state, state.panelScrollBarPageDownButtonWidget, pageDownName.c_str(), 0.0f, 0.0f, 0.0f, 0.0f, state.panelScrollBarWidget);
                ConfigureRawWidget(state, state.panelScrollBarPageUpButtonWidget, pageUpName.c_str(), 0.0f, 0.0f, 0.0f, 0.0f, state.panelScrollBarWidget);
                state.panelScrollBarPageDownWasActive = false;
                state.panelScrollBarPageUpWasActive = false;
                SetRawWidgetVisible(state, state.panelScrollBarWidget, false);
                if (state.panelScrollBarTrackVisualWidget != nullptr) {
                    SetRawWidgetVisible(state, state.panelScrollBarTrackVisualWidget, false);
                }
                if (state.panelScrollBarThumbVisualWidget != nullptr) {
                    SetRawWidgetVisible(state, state.panelScrollBarThumbVisualWidget, false);
                }
                return;
            }

            ClampTopVisibleOptionIndex(state);
            const std::size_t maxFirstVisibleIndex = selectedOption.optionCount - kVisibleRowCount;
            const float visibleFraction =
                static_cast<float>(kVisibleRowCount) /
                static_cast<float>(selectedOption.optionCount);
            float thumbSizeY = (std::max)(kPanelScrollBarMinThumbSizeY, kPanelScrollBarSizeY * visibleFraction);
            thumbSizeY = (std::min)(thumbSizeY, kPanelScrollBarSizeY);

            const float thumbTravelY = (std::max)(0.0f, kPanelScrollBarSizeY - thumbSizeY);
            const float thumbProgress =
                (maxFirstVisibleIndex == 0u) ?
                0.0f :
                (static_cast<float>(state.topVisibleOptionIndex) / static_cast<float>(maxFirstVisibleIndex));
            const float thumbPositionY = thumbTravelY * thumbProgress;
            const float pageUpSizeY = thumbPositionY;
            const float pageDownPositionY = thumbPositionY + thumbSizeY;
            const float pageDownSizeY = (std::max)(0.0f, kPanelScrollBarSizeY - pageDownPositionY);

            ConfigureRawWidget(state, state.panelScrollBarDecButtonWidget, decName.c_str(), 0.0f, 0.0f, 0.0f, 0.0f, state.panelScrollBarWidget);
            ConfigureRawWidget(state, state.panelScrollBarIncButtonWidget, incName.c_str(), 0.0f, 0.0f, 0.0f, 0.0f, state.panelScrollBarWidget);
            ConfigureRawWidget(
                state,
                state.panelScrollBarTrackButtonWidget,
                trackName.c_str(),
                0.0f,
                thumbPositionY,
                kPanelScrollBarSizeX,
                thumbSizeY,
                state.panelScrollBarWidget
            );
            ConfigureRawWidget(
                state,
                state.panelScrollBarPageUpButtonWidget,
                pageUpName.c_str(),
                0.0f,
                0.0f,
                kPanelScrollBarSizeX,
                pageUpSizeY,
                state.panelScrollBarWidget
            );
            ConfigureRawWidget(
                state,
                state.panelScrollBarPageDownButtonWidget,
                pageDownName.c_str(),
                0.0f,
                pageDownPositionY,
                kPanelScrollBarSizeX,
                pageDownSizeY,
                state.panelScrollBarWidget
            );

            ApplyRawWidgetState(state, state.panelScrollBarWidget);
            ApplyRawWidgetState(state, state.panelScrollBarTrackButtonWidget);
            ApplyRawWidgetState(state, state.panelScrollBarPageUpButtonWidget);
            ApplyRawWidgetState(state, state.panelScrollBarPageDownButtonWidget);
            SetRawWidgetVisible(state, state.panelScrollBarWidget, true);
            SetRawWidgetVisible(state, state.panelScrollBarDecButtonWidget, false);
            SetRawWidgetVisible(state, state.panelScrollBarIncButtonWidget, false);
            SetRawWidgetVisible(state, state.panelScrollBarTrackButtonWidget, true);
            SetRawWidgetVisible(state, state.panelScrollBarPageUpButtonWidget, true);
            SetRawWidgetVisible(state, state.panelScrollBarPageDownButtonWidget, true);
            if (state.panelScrollBarTrackVisualWidget != nullptr) {
                ConfigureRawWidget(
                    state,
                    state.panelScrollBarTrackVisualWidget,
                    kPanelScrollBarTrackVisualName,
                    PanelLocalToScreenX(kPanelScrollBarPositionX),
                    PanelLocalToScreenY(kPanelScrollBarPositionY),
                    PanelLocalToScreenWidth(kPanelScrollBarSizeX),
                    PanelLocalToScreenHeight(kPanelScrollBarSizeY),
                    state.rootWidgetRaw
                );
                ApplyRawWidgetState(state, state.panelScrollBarTrackVisualWidget);
                SetRawWidgetVisible(state, state.panelScrollBarTrackVisualWidget, true);
            }
            if (state.panelScrollBarThumbVisualWidget != nullptr) {
                ConfigureRawWidget(
                    state,
                    state.panelScrollBarThumbVisualWidget,
                    kPanelScrollBarThumbVisualName,
                    PanelLocalToScreenX(kPanelScrollBarPositionX),
                    PanelLocalToScreenY(kPanelScrollBarPositionY + thumbPositionY),
                    PanelLocalToScreenWidth(kPanelScrollBarSizeX),
                    PanelLocalToScreenHeight(thumbSizeY),
                    state.rootWidgetRaw
                );
                ApplyRawWidgetState(state, state.panelScrollBarThumbVisualWidget);
                SetRawWidgetVisible(state, state.panelScrollBarThumbVisualWidget, true);
            }
        }

        float ComputeNumericOptionProgress(const OptionEntry& optionEntry) {
            if (optionEntry.minValue >= optionEntry.maxValue) {
                return 0.0f;
            }

            switch (optionEntry.type) {
            case CoHModSDKConfigType_Int: {
                const float minValue = static_cast<float>(static_cast<std::int32_t>(std::lround(optionEntry.minValue)));
                const float maxValue = static_cast<float>(static_cast<std::int32_t>(std::lround(optionEntry.maxValue)));
                if (maxValue <= minValue) {
                    return 0.0f;
                }

                return std::clamp(
                    (static_cast<float>(optionEntry.currentValue.intValue) - minValue) / (maxValue - minValue),
                    0.0f,
                    1.0f
                );
            }

            case CoHModSDKConfigType_Float:
                return std::clamp(
                    (optionEntry.currentValue.floatValue - optionEntry.minValue) / (optionEntry.maxValue - optionEntry.minValue),
                    0.0f,
                    1.0f
                );

            default:
                return 0.0f;
            }
        }

        void ConfigureRowSliderProgress(State& state, std::size_t rowIndex, float progress) {
            if ((rowIndex >= kVisibleRowCount) ||
                (state.rowSliderWidgets[rowIndex] == nullptr) ||
                (state.rowSliderButtonWidgets[rowIndex] == nullptr) ||
                (state.widgetSetPosition == nullptr)) {
                return;
            }

            const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
            const float buttonPositionX =
                kRowSliderButtonMinPositionX +
                ((kRowSliderButtonMaxPositionX - kRowSliderButtonMinPositionX) * clampedProgress);

            state.widgetSetPosition(
                state.rowSliderButtonWidgets[rowIndex],
                buttonPositionX,
                kRowSliderButtonPositionY
            );
        }

        void SetNativeRowSliderProgress(State& state, std::size_t rowIndex, float progress) {
            if ((rowIndex >= kVisibleRowCount) || !state.rowNativeSliderInitialized[rowIndex]) {
                return;
            }

            const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
            state.rowNativeSliders[rowIndex].CurrentValue() = clampedProgress;
            ConfigureRowSliderProgress(state, rowIndex, clampedProgress);
            state.rowObservedSliderProgress[rowIndex] = clampedProgress;
            state.rowHasObservedSliderProgress[rowIndex] = true;
        }

        bool BindNativeRowSliderInput(State& state, OpaqueNativeSlider& nativeSlider) {
            if (state.nativeSliderBindInputAddress == nullptr) {
                return false;
            }

#if defined(_M_IX86)
            CallWithEaxContext0(nativeSlider.Get(), state.nativeSliderBindInputAddress);
            return true;
#else
            (void)state;
            (void)nativeSlider;
            return false;
#endif
        }

        bool InitializeNativeRowSlider(State& state, std::size_t rowIndex) {
            if ((rowIndex >= kVisibleRowCount) ||
                (state.customWidgetCtor == nullptr) ||
                (state.customWidgetDtor == nullptr) ||
                (state.artLabelCtor == nullptr) ||
                (state.artLabelDtor == nullptr) ||
                (state.widgetProxyBind == nullptr) ||
                (state.rowSliderBarWidgets[rowIndex] == nullptr) ||
                (state.rowSliderButtonWidgets[rowIndex] == nullptr)) {
                return false;
            }

            OpaqueNativeSlider& nativeSlider = state.rowNativeSliders[rowIndex];
            nativeSlider.storage.fill(std::byte { 0 });
            state.customWidgetCtor(nativeSlider.Get());
            state.artLabelCtor(nativeSlider.GetKnobProxy());
            state.widgetProxyBind(nativeSlider.Get(), state.rowSliderBarWidgets[rowIndex]);
            state.widgetProxyBind(nativeSlider.GetKnobProxy(), state.rowSliderButtonWidgets[rowIndex]);
            nativeSlider.Callback() = {};
            nativeSlider.CurrentValue() = 0.0f;
            nativeSlider.EnabledFlag() = 1u;
            nativeSlider.MinValue() = 0.0f;
            nativeSlider.MaxValue() = 1.0f;

            if (!BindNativeRowSliderInput(state, nativeSlider)) {
                state.artLabelDtor(nativeSlider.GetKnobProxy());
                state.customWidgetDtor(nativeSlider.Get());
                nativeSlider.storage.fill(std::byte { 0 });
                return false;
            }

            state.rowNativeSliderInitialized[rowIndex] = true;
            state.rowObservedSliderProgress[rowIndex] = 0.0f;
            state.rowHasObservedSliderProgress[rowIndex] = false;
            LogInfo("CoH Mod Config UI: Initialized native slider controller for row " + std::to_string(rowIndex) + ".");
            return true;
        }

        void DestroyNativeRowSlider(State& state, std::size_t rowIndex) {
            if ((rowIndex >= kVisibleRowCount) ||
                !state.rowNativeSliderInitialized[rowIndex] ||
                (state.customWidgetDtor == nullptr) ||
                (state.artLabelDtor == nullptr)) {
                return;
            }

            state.artLabelDtor(state.rowNativeSliders[rowIndex].GetKnobProxy());
            state.customWidgetDtor(state.rowNativeSliders[rowIndex].Get());
            state.rowNativeSliders[rowIndex].storage.fill(std::byte { 0 });
            state.rowNativeSliderInitialized[rowIndex] = false;
            state.rowObservedSliderProgress[rowIndex] = 0.0f;
            state.rowHasObservedSliderProgress[rowIndex] = false;
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
        __declspec(naked) void __cdecl CallWithEaxContext0(void* /*eaxContext*/, void* /*targetFn*/) {
            __asm {
                push esi
                push edi
                push ebx
                mov eax, [esp + 16]
                mov ecx, [esp + 20]
                call ecx
                pop ebx
                pop edi
                pop esi
                ret
            }
        }

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

            // Step 8: Create row name TextLabels and attach them to the panel.
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

            // Step 9: Resolve native CheckButton widgets preloaded by cohmodconfigui.screen.
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

            // Step 10: Resolve native slider widgets preloaded by cohmodconfigui.screen for int/float rows.
            for (std::size_t i = 0u; i < kVisibleRowCount; ++i) {
                const std::string rowSliderName = MakeRowSliderName(i);
                const std::string rowSliderButtonName = MakeRowSliderButtonName(i);
                const std::string rowSliderBarName = MakeRowSliderBarName(i);

                state.rowSliderWidgets[i] = state.findWidgetByName(state.rootWidgetRaw, rowSliderName.c_str(), 0);
                if (state.rowSliderWidgets[i] == nullptr) {
                    state.rowSliderWidgets[i] = state.findWidgetByName(state.rootWidgetRaw, rowSliderName.c_str(), 1);
                    if (state.rowSliderWidgets[i] == nullptr) {
                        LogError("CoH Mod Config UI: Failed to resolve preloaded slider widget '" + rowSliderName + "' for row " + std::to_string(i) + ".");
                        return false;
                    }
                }

                state.rowSliderButtonWidgets[i] = FindNamedWidget(state, state.rowSliderWidgets[i], rowSliderButtonName.c_str());
                state.rowSliderBarWidgets[i] = FindNamedWidget(state, state.rowSliderWidgets[i], rowSliderBarName.c_str());
                if ((state.rowSliderButtonWidgets[i] == nullptr) || (state.rowSliderBarWidgets[i] == nullptr)) {
                    LogError(
                        "CoH Mod Config UI: Failed to resolve slider child widgets for row " +
                        std::to_string(i) +
                        " (button='" + rowSliderButtonName +
                        "', bar='" + rowSliderBarName + "')."
                    );
                    return false;
                }

                if (!RemoveRenderChild(state, state.rootWidgetRaw, state.rowSliderWidgets[i])) {
                    LogError("CoH Mod Config UI: Failed to remove preloaded slider widget '" + rowSliderName + "' from the root render tree for row " + std::to_string(i) + ".");
                    return false;
                }

                ConfigureRawWidget(
                    state,
                    state.rowSliderWidgets[i],
                    rowSliderName.c_str(),
                    kRowControlPositionX,
                    kFirstRowPositionY + (static_cast<float>(i) * kRowSpacingY) + kRowSliderOffsetY,
                    kRowSliderSizeX,
                    kRowSliderSizeY,
                    state.panelWidgetRaw
                );
                ConfigureRawWidget(
                    state,
                    state.rowSliderButtonWidgets[i],
                    rowSliderButtonName.c_str(),
                    kRowSliderButtonMinPositionX,
                    kRowSliderButtonPositionY,
                    kRowSliderButtonSizeX,
                    kRowSliderButtonSizeY,
                    state.rowSliderWidgets[i]
                );
                ConfigureRawWidget(
                    state,
                    state.rowSliderBarWidgets[i],
                    rowSliderBarName.c_str(),
                    0.0f,
                    0.0f,
                    kRowSliderSizeX,
                    kRowSliderSizeY,
                    state.rowSliderWidgets[i]
                );

                if (!AttachRenderChild(state, state.panelWidgetRaw, state.rowSliderWidgets[i])) {
                    LogError("CoH Mod Config UI: Failed to attach preloaded slider widget '" + rowSliderName + "' to the panel render tree for row " + std::to_string(i) + ".");
                    return false;
                }

                SetRawWidgetVisible(state, state.rowSliderWidgets[i], false);
                if (!InitializeNativeRowSlider(state, i)) {
                    LogError("CoH Mod Config UI: Failed to initialize native slider controller for row " + std::to_string(i) + ".");
                    return false;
                }
                LogInfo(
                    "CoH Mod Config UI: Resolved, moved, and attached preloaded slider widget '" +
                    rowSliderName +
                    "' for row " +
                    std::to_string(i) +
                    "."
                );
            }
            LogInfo("CoH Mod Config UI: Row numeric slider widgets resolved from the active screen.");

            void* optionsMenuDonorScreen = nullptr;
            if (!EnsureDonorScreenLoaded(state, optionsMenuDonorScreen, kOptionsmenuDonorScreenName)) {
                LogError("CoH Mod Config UI: Failed to load donor screen '" + std::string(kOptionsmenuDonorScreenName) + "' for enum row widgets.");
                return false;
            }

            state.panelScrollBarWidget = CreateRawWidgetByType(state, kScrollBarWidgetTypeName);
            if (state.panelScrollBarWidget == nullptr) {
                LogError("CoH Mod Config UI: Failed to create the panel ScrollBar widget.");
                return false;
            }
            if (!TransferDonorPresentationDirect(state, state.panelScrollBarWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarDonorWidgetName, true)) {
                LogWarning("CoH Mod Config UI: Failed to transfer panel ScrollBar Presentation from donor.");
            }
            ConfigureRawWidget(
                state,
                state.panelScrollBarWidget,
                kPanelScrollBarName,
                kPanelScrollBarPositionX,
                kPanelScrollBarPositionY,
                kPanelScrollBarSizeX,
                kPanelScrollBarSizeY,
                state.panelWidgetRaw
            );
            if (!AttachRenderChild(state, state.panelWidgetRaw, state.panelScrollBarWidget)) {
                LogError("CoH Mod Config UI: Failed to attach the panel ScrollBar widget to the panel render tree.");
                return false;
            }
            if (!ResolveScrollBarChildWidgets(
                state,
                state.panelScrollBarWidget,
                kPanelScrollBarName,
                state.panelScrollBarDecButtonWidget,
                state.panelScrollBarIncButtonWidget,
                state.panelScrollBarTrackButtonWidget,
                state.panelScrollBarPageDownButtonWidget,
                state.panelScrollBarPageUpButtonWidget
            )) {
                LogError("CoH Mod Config UI: Failed to resolve the panel ScrollBar subtree widgets.");
                return false;
            }
            LogInfo(
                "CoH Mod Config UI: Panel ScrollBar child widgets resolved as dec='" +
                ReadWidgetNameForLog(state.panelScrollBarDecButtonWidget) +
                "', inc='" +
                ReadWidgetNameForLog(state.panelScrollBarIncButtonWidget) +
                "', track='" +
                ReadWidgetNameForLog(state.panelScrollBarTrackButtonWidget) +
                "', pgDn='" +
                ReadWidgetNameForLog(state.panelScrollBarPageDownButtonWidget) +
                "', pgUp='" +
                ReadWidgetNameForLog(state.panelScrollBarPageUpButtonWidget) +
                "'."
            );
            if (!TransferDonorPresentationDirect(state, state.panelScrollBarDecButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarDecDonorWidgetName, true)) {
                LogWarning("CoH Mod Config UI: Failed to transfer panel ScrollBar decrement button Presentation from donor.");
            }
            if (!TransferDonorPresentationDirect(state, state.panelScrollBarIncButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarIncDonorWidgetName, true)) {
                LogWarning("CoH Mod Config UI: Failed to transfer panel ScrollBar increment button Presentation from donor.");
            }
            if (!TransferDonorPresentationDirect(state, state.panelScrollBarTrackButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarTrackDonorWidgetName, true)) {
                LogWarning("CoH Mod Config UI: Failed to transfer panel ScrollBar track Presentation from donor.");
            }
            if (!TransferDonorPresentationDirect(state, state.panelScrollBarPageDownButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarPageDownDonorWidgetName, true)) {
                LogWarning("CoH Mod Config UI: Failed to transfer panel ScrollBar page-down Presentation from donor.");
            }
            if (!TransferDonorPresentationDirect(state, state.panelScrollBarPageUpButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarPageUpDonorWidgetName, true)) {
                LogWarning("CoH Mod Config UI: Failed to transfer panel ScrollBar page-up Presentation from donor.");
            }
            SetRawWidgetVisible(state, state.panelScrollBarWidget, false);
            state.panelScrollBarTrackVisualWidget = FindNamedWidget(state, state.rootWidgetRaw, kPanelScrollBarTrackVisualName);
            state.panelScrollBarThumbVisualWidget = FindNamedWidget(state, state.rootWidgetRaw, kPanelScrollBarThumbVisualName);
            if ((state.panelScrollBarTrackVisualWidget != nullptr) &&
                (state.panelScrollBarThumbVisualWidget != nullptr)) {
                ConfigureRawWidget(
                    state,
                    state.panelScrollBarTrackVisualWidget,
                    kPanelScrollBarTrackVisualName,
                    PanelLocalToScreenX(kPanelScrollBarPositionX),
                    PanelLocalToScreenY(kPanelScrollBarPositionY),
                    PanelLocalToScreenWidth(kPanelScrollBarSizeX),
                    PanelLocalToScreenHeight(kPanelScrollBarSizeY),
                    state.rootWidgetRaw
                );
                ConfigureRawWidget(
                    state,
                    state.panelScrollBarThumbVisualWidget,
                    kPanelScrollBarThumbVisualName,
                    PanelLocalToScreenX(kPanelScrollBarPositionX),
                    PanelLocalToScreenY(kPanelScrollBarPositionY),
                    PanelLocalToScreenWidth(kPanelScrollBarSizeX),
                    PanelLocalToScreenHeight(kPanelScrollBarMinThumbSizeY),
                    state.rootWidgetRaw
                );

                if (state.widgetSetHitArea != nullptr) {
                    // These are display-only overlays. Let the underlying native scrollbar subtree receive input.
                    state.widgetSetHitArea(state.panelScrollBarTrackVisualWidget, nullptr);
                    state.widgetSetHitArea(state.panelScrollBarThumbVisualWidget, nullptr);
                }

                OpaqueArtLabel panelScrollTrackArtLabel = {};
                OpaqueArtLabel panelScrollThumbArtLabel = {};
                const bool boundTrackArtLabel = BindArtLabelProxy(state, panelScrollTrackArtLabel, state.panelScrollBarTrackVisualWidget);
                const bool boundThumbArtLabel = BindArtLabelProxy(state, panelScrollThumbArtLabel, state.panelScrollBarThumbVisualWidget);
                if (boundTrackArtLabel && (state.artLabelSetAllArtVisible != nullptr)) {
                    state.artLabelSetAllArtVisible(panelScrollTrackArtLabel.Get(), true);
                }
                if (boundThumbArtLabel && (state.artLabelSetAllArtVisible != nullptr)) {
                    state.artLabelSetAllArtVisible(panelScrollThumbArtLabel.Get(), true);
                }
                ApplyRawWidgetState(state, state.panelScrollBarTrackVisualWidget);
                ApplyRawWidgetState(state, state.panelScrollBarThumbVisualWidget);
                SetRawWidgetVisible(state, state.panelScrollBarTrackVisualWidget, false);
                SetRawWidgetVisible(state, state.panelScrollBarThumbVisualWidget, false);
                if (boundTrackArtLabel && (state.artLabelDtor != nullptr)) {
                    state.artLabelDtor(panelScrollTrackArtLabel.Get());
                }
                if (boundThumbArtLabel && (state.artLabelDtor != nullptr)) {
                    state.artLabelDtor(panelScrollThumbArtLabel.Get());
                }
                LogInfo("CoH Mod Config UI: Resolved panel scrollbar visuals from the active screen.");
            }
            else {
                LogWarning("CoH Mod Config UI: Failed to resolve panel scrollbar visuals from the active screen.");
            }

            // Step 12: Create native ComboBox widgets for enum rows and resolve their child widgets.
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

            // Step 13: Create a native ComboBox widget for the header-level mod selector.
            state.modSelectorComboBoxWidget = CreateRawWidgetByType(state, kComboBoxWidgetTypeName);
            if (state.modSelectorComboBoxWidget == nullptr) {
                LogError("CoH Mod Config UI: Failed to create native ComboBox widget for the mod selector.");
                return false;
            }

            if (!TransferDonorPresentationDirect(state, state.modSelectorComboBoxWidget, optionsMenuDonorScreen, kDropdownDonorWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer mod selector ComboBox root Presentation from donor.");
            }

            const std::string modSelectorComboBoxName = MakeModSelectorComboBoxName();
            ConfigureRawWidget(
                state,
                state.modSelectorComboBoxWidget,
                modSelectorComboBoxName.c_str(),
                kModSelectorPositionX,
                kModSelectorPositionY,
                kModSelectorSizeX,
                kModSelectorSizeY,
                state.panelWidgetRaw
            );
            if (!AttachRenderChild(state, state.panelWidgetRaw, state.modSelectorComboBoxWidget)) {
                LogError("CoH Mod Config UI: Failed to attach the mod selector ComboBox to the panel render tree.");
                return false;
            }

            if (!ResolveComboBoxChildWidgets(
                state,
                state.modSelectorComboBoxWidget,
                modSelectorComboBoxName,
                state.modSelectorValueLabelWidget,
                state.modSelectorArrowButtonWidget,
                state.modSelectorListBoxWidget
            )) {
                LogError("CoH Mod Config UI: Failed to resolve DropDownExt child widgets for the mod selector.");
                return false;
            }

            const std::string expectedModSelectorLabelName = MakeModSelectorLabelName();
            const std::string expectedModSelectorButtonName = MakeModSelectorButtonName();
            const std::string expectedModSelectorListBoxName = MakeModSelectorListBoxName();
            LogInfo(
                "CoH Mod Config UI: Mod selector ComboBox child widgets resolved as label='" +
                ReadWidgetNameForLog(state.modSelectorValueLabelWidget) +
                "', button='" +
                ReadWidgetNameForLog(state.modSelectorArrowButtonWidget) +
                "', listBox='" +
                ReadWidgetNameForLog(state.modSelectorListBoxWidget) +
                "'."
            );

            ConfigureRawWidget(
                state,
                state.modSelectorValueLabelWidget,
                expectedModSelectorLabelName.c_str(),
                0.0f,
                0.0f,
                kModSelectorLabelSizeX,
                kModSelectorSizeY,
                state.modSelectorComboBoxWidget
            );
            ConfigureRawWidget(
                state,
                state.modSelectorArrowButtonWidget,
                expectedModSelectorButtonName.c_str(),
                kModSelectorButtonOffsetX,
                0.0f,
                kModSelectorButtonSizeX,
                kModSelectorSizeY,
                state.modSelectorComboBoxWidget
            );

            if (!TransferDonorPresentationDirect(state, state.modSelectorValueLabelWidget, optionsMenuDonorScreen, kDropdownLabelDonorWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer mod selector ComboBox label Presentation from donor.");
            }
            if (!TransferDonorPresentationDirect(state, state.modSelectorArrowButtonWidget, optionsMenuDonorScreen, kDropdownButtonDonorWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer mod selector ComboBox button Presentation from donor.");
            }
            if (!TransferDonorPresentationDirect(state, state.modSelectorListBoxWidget, optionsMenuDonorScreen, kDropdownListBoxDonorWidgetName, true)) {
                LogWarning("CoH Mod Config UI: Failed to transfer mod selector ComboBox list box Presentation from donor.");
            }

            void* modSelectorItemsWidget = nullptr;
            void* modSelectorScrollBarWidget = nullptr;
            void* modSelectorItemTemplateWidget = nullptr;
            if (!ResolveListBoxChildWidgets(
                state,
                state.modSelectorListBoxWidget,
                expectedModSelectorListBoxName,
                modSelectorItemsWidget,
                modSelectorScrollBarWidget,
                modSelectorItemTemplateWidget
            )) {
                LogError("CoH Mod Config UI: Failed to resolve mod selector list box subtree widgets.");
                return false;
            }

            ConfigureModSelectorListBoxGeometry(state);

            if (!TransferDonorPresentationDirect(state, modSelectorItemsWidget, optionsMenuDonorScreen, kDropdownListBoxItemsDonorWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer mod selector list items Presentation from donor.");
            }
            if (!TransferDonorPresentationDirect(state, modSelectorScrollBarWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarDonorWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer mod selector list scrollbar Presentation from donor.");
            }
            if (!TransferDonorPresentationDirect(state, modSelectorItemTemplateWidget, optionsMenuDonorScreen, kDropdownListBoxItemTemplateDonorWidgetName)) {
                LogWarning("CoH Mod Config UI: Failed to transfer mod selector list item template Presentation from donor.");
            }

            void* modSelectorScrollBarDecButtonWidget = nullptr;
            void* modSelectorScrollBarIncButtonWidget = nullptr;
            void* modSelectorScrollBarTrackButtonWidget = nullptr;
            void* modSelectorScrollBarPageDownButtonWidget = nullptr;
            void* modSelectorScrollBarPageUpButtonWidget = nullptr;
            const std::string modSelectorScrollBarName = MakeModSelectorListBoxScrollBarName();
            if (ResolveScrollBarChildWidgets(
                state,
                modSelectorScrollBarWidget,
                modSelectorScrollBarName,
                modSelectorScrollBarDecButtonWidget,
                modSelectorScrollBarIncButtonWidget,
                modSelectorScrollBarTrackButtonWidget,
                modSelectorScrollBarPageDownButtonWidget,
                modSelectorScrollBarPageUpButtonWidget
            )) {
                LogInfo(
                    "CoH Mod Config UI: Mod selector scrollbar child widgets resolved as dec='" +
                    ReadWidgetNameForLog(modSelectorScrollBarDecButtonWidget) +
                    "', inc='" +
                    ReadWidgetNameForLog(modSelectorScrollBarIncButtonWidget) +
                    "', track='" +
                    ReadWidgetNameForLog(modSelectorScrollBarTrackButtonWidget) +
                    "', pgDn='" +
                    ReadWidgetNameForLog(modSelectorScrollBarPageDownButtonWidget) +
                    "', pgUp='" +
                    ReadWidgetNameForLog(modSelectorScrollBarPageUpButtonWidget) +
                    "'."
                );

                if (!TransferDonorPresentationDirect(state, modSelectorScrollBarDecButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarDecDonorWidgetName)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer mod selector scrollbar decrement button Presentation from donor.");
                }
                if (!TransferDonorPresentationDirect(state, modSelectorScrollBarIncButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarIncDonorWidgetName)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer mod selector scrollbar increment button Presentation from donor.");
                }
                if (!TransferDonorPresentationDirect(state, modSelectorScrollBarTrackButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarTrackDonorWidgetName)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer mod selector scrollbar track Presentation from donor.");
                }
                if (!TransferDonorPresentationDirect(state, modSelectorScrollBarPageDownButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarPageDownDonorWidgetName, true)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer mod selector scrollbar page-down Presentation from donor.");
                }
                if (!TransferDonorPresentationDirect(state, modSelectorScrollBarPageUpButtonWidget, optionsMenuDonorScreen, kDropdownListBoxScrollBarPageUpDonorWidgetName, true)) {
                    LogWarning("CoH Mod Config UI: Failed to transfer mod selector scrollbar page-up Presentation from donor.");
                }
            } else {
                LogWarning("CoH Mod Config UI: Failed to resolve mod selector scrollbar subtree widgets.");
            }

            LogInfo("CoH Mod Config UI: Native mod selector ComboBox created, attached, and child widgets resolved.");

            // Step 14: Construct and bind title, row label, bool, numeric, and enum proxies.
            state.textLabelCtor(state.titleLabel.Get());
            state.widgetProxyBind(state.titleLabel.Get(), state.titleLabelRaw);
            ApplyWidgetProxyState(state, state.titleLabel.Get());
            TrySetTextLabel(state, state.titleLabel, "Mod Options", false);

            state.textLabelCtor(state.modSelectorValueLabel.Get());
            state.widgetProxyBind(state.modSelectorValueLabel.Get(), state.modSelectorValueLabelWidget);
            ApplyWidgetProxyState(state, state.modSelectorValueLabel.Get());
            TrySetTextLabel(state, state.modSelectorValueLabel, BuildEmptyButtonText(), false);

            if (!BindButtonProxy(state, state.modSelectorButton, state.modSelectorArrowButtonWidget)) {
                LogError("CoH Mod Config UI: Failed to bind the mod selector button proxy.");
                return false;
            }
            if (state.widgetProxySetVisible != nullptr) {
                state.widgetProxySetVisible(state.modSelectorButton.Get(), true);
            }
            if (state.widgetProxySetEnabled != nullptr) {
                state.widgetProxySetEnabled(state.modSelectorButton.Get(), true);
            }

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

                if (!BindGenericWidgetProxy(state, state.rowSliders[i], state.rowSliderWidgets[i])) {
                    LogError("CoH Mod Config UI: Failed to bind slider proxy for row " + std::to_string(i) + ".");
                    return false;
                }
                ApplyWidgetProxyState(state, state.rowSliders[i].Get());
            }
            LogInfo("CoH Mod Config UI: All proxy objects constructed.");

            state.overlayBuilt = true;
            LogInfo("CoH Mod Config UI: Overlay built successfully (panel + title + mod selector + row labels + row CheckButtons + row Sliders + native enum ComboBox child binding milestone).");
            return true;
        }

        void DestroyOverlay(State& state) {
            if (!state.overlayBuilt && (state.screen == nullptr)) {
                return;
            }

            if (state.overlayBuilt) {
                for (std::size_t i = kVisibleRowCount; i > 0u; --i) {
                    DestroyNativeRowSlider(state, i - 1u);
                    state.genericWidgetDtor(state.rowSliders[i - 1u].Get());
                    state.checkButtonDtor(state.rowCheckButtons[i - 1u].Get());
                    state.buttonDtor(state.rowArrowButtons[i - 1u].Get());
                    state.textLabelDtor(state.rowValueLabels[i - 1u].Get());
                    state.textLabelDtor(state.rowLabels[i - 1u].Get());
                }

                state.buttonDtor(state.modSelectorButton.Get());
                state.textLabelDtor(state.modSelectorValueLabel.Get());
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
                state.widgetProxySetVisible(state.rowSliders[rowIndex].Get(), false);
            }
            SetRawWidgetVisible(state, state.rowComboBoxWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowListBoxWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowCheckButtonWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowSliderWidgets[rowIndex], false);
            state.rowObservedListBoxSelection[rowIndex] = -1;
            state.rowHasObservedListBoxSelection[rowIndex] = false;
            state.rowObservedSliderProgress[rowIndex] = 0.0f;
            state.rowHasObservedSliderProgress[rowIndex] = false;
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
                state.widgetProxySetVisible(state.rowSliders[rowIndex].Get(), false);
            }
            SetRawWidgetVisible(state, state.rowComboBoxWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowListBoxWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowCheckButtonWidgets[rowIndex], false);
            SetRawWidgetVisible(state, state.rowSliderWidgets[rowIndex], false);

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
                // Show native slider-style control for numeric values.
                const float progress = ComputeNumericOptionProgress(opt);
                SetNativeRowSliderProgress(state, rowIndex, progress);
                SetRawWidgetVisible(state, state.rowSliderWidgets[rowIndex], true);
                if (state.widgetProxySetVisible != nullptr) {
                    state.widgetProxySetVisible(state.rowSliders[rowIndex].Get(), true);
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

            ModEntry* selectedMod = nullptr;
            std::size_t selectedModIndex = 0u;
            const bool hasSelectedMod = TryGetSelectedMod(state, selectedMod, selectedModIndex);
            SelectedOptionRef selectedOption = {};
            const bool hasSelectedOption = TryGetSelectedOption(state, selectedOption);
            EnsureSelectedOptionVisible(state);

            bool updatedAllWidgets =
                TrySetTextLabel(state, state.titleLabel, BuildTitleText(selectedMod, hasSelectedOption ? &selectedOption : nullptr), false);

            if (hasSelectedMod) {
                ConfigureModSelectorListBoxGeometry(state);
                SetRawWidgetVisible(state, state.modSelectorComboBoxWidget, true);
                SetRawWidgetVisible(state, state.modSelectorListBoxWidget, true);
                updatedAllWidgets =
                    TrySetTextLabel(state, state.modSelectorValueLabel, BuildModDisplayText(*selectedMod), false) &&
                    updatedAllWidgets;
                if (!PopulateModListBox(state)) {
                    LogWarning("CoH Mod Config UI: Failed to populate the mod selector list box.");
                    updatedAllWidgets = false;
                }

                long selectedNativeIndex = -1;
                if (!TryMapVisualIndexToNativeListIndex(
                    state.catalog->GetModCount(),
                    static_cast<long>(state.selectedModIndex),
                    selectedNativeIndex)) {
                    selectedNativeIndex = -1;
                }
                state.observedModListSelection = selectedNativeIndex;
                state.hasObservedModListSelection = selectedNativeIndex >= 0;

                if (state.widgetProxySetVisible != nullptr) {
                    state.widgetProxySetVisible(state.modSelectorValueLabel.Get(), true);
                    state.widgetProxySetVisible(state.modSelectorButton.Get(), true);
                }
            } else {
                SetRawWidgetVisible(state, state.modSelectorComboBoxWidget, false);
                SetRawWidgetVisible(state, state.modSelectorListBoxWidget, false);
                state.observedModListSelection = -1;
                state.hasObservedModListSelection = false;
                if (state.widgetProxySetVisible != nullptr) {
                    state.widgetProxySetVisible(state.modSelectorValueLabel.Get(), false);
                    state.widgetProxySetVisible(state.modSelectorButton.Get(), false);
                }
            }

            for (std::size_t rowIndex = 0u; rowIndex < kVisibleRowCount; ++rowIndex) {
                if (!hasSelectedOption) {
                    HideAllRowControls(state, rowIndex);
                    continue;
                }

                SelectedOptionRef rowOption = {};
                if (TryGetVisibleRowOption(state, rowIndex, rowOption)) {
                    UpdateRowForOption(state, rowIndex, rowOption);
                } else {
                    HideAllRowControls(state, rowIndex);
                }
            }

            ConfigurePanelScrollBarGeometry(state);
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

            if (state.originalGameWindowProc == nullptr) {
                InstallGameWindowHook(state);
            }

            if (!RefreshVisibleMenu(state)) {
                LogError("CoH Mod Config UI failed to build or refresh the overlay.");
                return;
            }

            state.pendingMouseWheelDelta = 0;
            state.hasPendingLeftClick = false;
            state.hasPendingMouseMove = false;
            state.panelScrollBarDragging = false;
            state.panelScrollBarDragOffsetY = 0.0f;
            state.modSelectorDropDownOpen = false;
            state.activeEnumDropDownRowIndex = -1;
            state.panelScrollBarPageUpWasActive = false;
            state.panelScrollBarPageDownWasActive = false;
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
            state.pendingMouseWheelDelta = 0;
            state.hasPendingLeftClick = false;
            state.hasPendingMouseMove = false;
            state.panelScrollBarDragging = false;
            state.panelScrollBarDragOffsetY = 0.0f;
            state.modSelectorDropDownOpen = false;
            state.activeEnumDropDownRowIndex = -1;
            state.panelScrollBarPageUpWasActive = false;
            state.panelScrollBarPageDownWasActive = false;
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
                state.modSelectorDropDownOpen = false;
                state.activeEnumDropDownRowIndex = -1;
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
            state.modSelectorDropDownOpen = false;
            state.activeEnumDropDownRowIndex = -1;
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

        bool TryApplyModSelectorSelectionChange(State& state, long nativeSelectedIndex) {
            if ((nativeSelectedIndex < 0) || (state.catalog == nullptr)) {
                return false;
            }

            const std::vector<ModEntry>& mods = state.catalog->GetMods();
            long selectedModIndex = -1;
            if (!TryMapNativeListIndexToVisualIndex(mods.size(), nativeSelectedIndex, selectedModIndex) ||
                (selectedModIndex < 0) ||
                (selectedModIndex >= static_cast<long>(mods.size()))) {
                return false;
            }

            const std::size_t newSelectedModIndex = static_cast<std::size_t>(selectedModIndex);
            if (newSelectedModIndex == state.selectedModIndex) {
                state.modSelectorDropDownOpen = false;
                state.activeEnumDropDownRowIndex = -1;
                return false;
            }

            state.selectedModIndex = newSelectedModIndex;
            state.selectedOptionIndex = 0u;
            state.topVisibleOptionIndex = 0u;
            state.modSelectorDropDownOpen = false;
            state.activeEnumDropDownRowIndex = -1;
            state.observedModListSelection = nativeSelectedIndex;
            state.hasObservedModListSelection = true;
            LogInfo(
                "CoH Mod Config UI: Mod selector changed to mod " +
                std::to_string(newSelectedModIndex) +
                " ('" +
                BuildModDisplayText(mods[newSelectedModIndex]) +
                "')."
            );
            RefreshVisibleMenu(state);
            return true;
        }

        bool TryApplyNumericRowSliderChange(State& state, std::size_t rowIndex, float normalizedProgress) {
            if (state.catalog == nullptr) {
                return false;
            }

            SelectedOptionRef rowOption = {};
            if (!TryGetVisibleRowOption(state, rowIndex, rowOption) ||
                (rowOption.modEntry == nullptr) ||
                (rowOption.optionEntry == nullptr)) {
                return false;
            }

            OptionEntry& optionEntry = *rowOption.optionEntry;
            if ((optionEntry.type != CoHModSDKConfigType_Int) && (optionEntry.type != CoHModSDKConfigType_Float)) {
                return false;
            }

            if (optionEntry.minValue >= optionEntry.maxValue) {
                return false;
            }

            const float clampedProgress = std::clamp(normalizedProgress, 0.0f, 1.0f);
            ModSDK::Config::Value newValue = optionEntry.currentValue;
            float appliedProgress = clampedProgress;

            if (optionEntry.type == CoHModSDKConfigType_Int) {
                const std::int32_t minValue = static_cast<std::int32_t>(std::lround(optionEntry.minValue));
                const std::int32_t maxValue = static_cast<std::int32_t>(std::lround(optionEntry.maxValue));
                const std::int32_t stepValue = (std::max)(1, static_cast<std::int32_t>(std::lround(optionEntry.step > 0.0f ? optionEntry.step : 1.0f)));
                const float scaledValue = static_cast<float>(minValue) + (clampedProgress * static_cast<float>(maxValue - minValue));
                std::int32_t candidateValue = minValue + (static_cast<std::int32_t>(std::lround((scaledValue - static_cast<float>(minValue)) / static_cast<float>(stepValue))) * stepValue);
                candidateValue = std::clamp(candidateValue, minValue, maxValue);
                newValue = ModSDK::Config::MakeIntValue(candidateValue);
                if (maxValue > minValue) {
                    appliedProgress = std::clamp(
                        (static_cast<float>(candidateValue) - static_cast<float>(minValue)) /
                        static_cast<float>(maxValue - minValue),
                        0.0f,
                        1.0f
                    );
                }
            }
            else {
                const float stepValue = optionEntry.step > 0.0f ? optionEntry.step : 0.0f;
                float candidateValue = optionEntry.minValue + (clampedProgress * (optionEntry.maxValue - optionEntry.minValue));
                if (stepValue > 0.0f) {
                    candidateValue = optionEntry.minValue + (std::round((candidateValue - optionEntry.minValue) / stepValue) * stepValue);
                }
                candidateValue = std::clamp(candidateValue, optionEntry.minValue, optionEntry.maxValue);
                newValue = ModSDK::Config::MakeFloatValue(candidateValue);
                appliedProgress = std::clamp(
                    (candidateValue - optionEntry.minValue) / (optionEntry.maxValue - optionEntry.minValue),
                    0.0f,
                    1.0f
                );
            }

            const bool valueChanged =
                ((optionEntry.type == CoHModSDKConfigType_Int) && (newValue.intValue != optionEntry.currentValue.intValue)) ||
                ((optionEntry.type == CoHModSDKConfigType_Float) && (std::fabs(newValue.floatValue - optionEntry.currentValue.floatValue) > kSliderProgressEpsilon));

            SetNativeRowSliderProgress(state, rowIndex, appliedProgress);
            state.selectedOptionIndex = rowOption.flatIndex;
            TrySetTextLabel(state, state.titleLabel, BuildTitleText(rowOption.modEntry, &rowOption), false);

            if (!valueChanged) {
                return false;
            }

            if (!ModSDK::Config::SetValue(rowOption.modEntry->modId.c_str(), optionEntry.optionId.c_str(), newValue)) {
                LogWarning(
                    "CoH Mod Config UI failed to update numeric value for " +
                    rowOption.modEntry->modId +
                    "." +
                    optionEntry.optionId +
                    " via native slider."
                );
                return false;
            }

            optionEntry.currentValue = newValue;
            LogInfo(
                "CoH Mod Config UI: Native slider value changed for row " +
                std::to_string(rowIndex) +
                " to normalized progress " +
                std::to_string(appliedProgress) +
                " ('" +
                FormatCurrentValue(optionEntry) +
                "')."
            );
            return true;
        }

        std::size_t ComputeFirstVisibleIndex(State& state) {
            ClampTopVisibleOptionIndex(state);
            return state.topVisibleOptionIndex;
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
                state.modSelectorDropDownOpen = false;
                state.activeEnumDropDownRowIndex = static_cast<long>(rowIndex);
                LogInfo("CoH Mod Config UI: Native ComboBox button clicked for row " + std::to_string(rowIndex) + ".");
                return;
            }

            state.modSelectorDropDownOpen = false;
            state.activeEnumDropDownRowIndex = -1;
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

        bool PollRawWidgetActiveEdge(State& state, void* rawWidget, bool& wasActive) {
            if ((state.widgetGetState == nullptr) || (rawWidget == nullptr)) {
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

            if (state.pendingMouseWheelDelta >= WHEEL_DELTA) {
                state.pendingMouseWheelDelta -= WHEEL_DELTA;
                LogInfo("CoH Mod Config UI: Mouse wheel up input observed.");
                if (state.modSelectorDropDownOpen || (state.activeEnumDropDownRowIndex >= 0)) {
                    TryScrollOpenDropDown(state, -1);
                    return;
                }
                if (TryScrollOptionWindow(state, -1)) {
                    return;
                }
            }
            else if (state.pendingMouseWheelDelta <= -WHEEL_DELTA) {
                state.pendingMouseWheelDelta += WHEEL_DELTA;
                LogInfo("CoH Mod Config UI: Mouse wheel down input observed.");
                if (state.modSelectorDropDownOpen || (state.activeEnumDropDownRowIndex >= 0)) {
                    TryScrollOpenDropDown(state, 1);
                    return;
                }
                if (TryScrollOptionWindow(state, 1)) {
                    return;
                }
            }
            if (TryHandlePanelScrollBarClick(state)) {
                return;
            }
            if (TryHandlePanelScrollBarDrag(state)) {
                return;
            }
            if (PollRawWidgetActiveEdge(state, state.panelScrollBarPageUpButtonWidget, state.panelScrollBarPageUpWasActive)) {
                LogInfo("CoH Mod Config UI: Panel scrollbar page-up click observed.");
                if (TryScrollOptionWindow(state, -(static_cast<int>(kVisibleRowCount) - 1))) {
                    return;
                }
            }
            if (PollRawWidgetActiveEdge(state, state.panelScrollBarPageDownButtonWidget, state.panelScrollBarPageDownWasActive)) {
                LogInfo("CoH Mod Config UI: Panel scrollbar page-down click observed.");
                if (TryScrollOptionWindow(state, static_cast<int>(kVisibleRowCount) - 1)) {
                    return;
                }
            }

            if (PollRawWidgetActiveEdge(state, state.modSelectorComboBoxWidget, state.modSelectorComboBoxWasActive) ||
                PollWidgetActiveEdge(state, state.modSelectorValueLabel.Get(), state.modSelectorValueLabelWasActive) ||
                PollWidgetActiveEdge(state, state.modSelectorButton.Get(), state.modSelectorArrowButtonWasActive)) {
                state.modSelectorDropDownOpen = true;
                state.activeEnumDropDownRowIndex = -1;
                LogInfo("CoH Mod Config UI: Mod selector ComboBox button clicked.");
            }
            if (state.modSelectorListBoxWidget != nullptr) {
                long selectedIndex = -1;
                if (TryGetCustomListBoxSelectedIndex(state, state.modSelectorListBoxWidget, selectedIndex)) {
                    if (!state.hasObservedModListSelection) {
                        state.observedModListSelection = selectedIndex;
                        state.hasObservedModListSelection = true;
                    }
                    else if (selectedIndex != state.observedModListSelection) {
                        state.observedModListSelection = selectedIndex;
                        if (TryApplyModSelectorSelectionChange(state, selectedIndex)) {
                            return;
                        }
                    }
                }
            }

            for (std::size_t i = 0u; i < kVisibleRowCount; ++i) {
                // Poll all widget types — only the visible one will have Active state.
                if ((state.rowActiveControlType[i] == CoHModSDKConfigType_Enum) &&
                    (PollRawWidgetActiveEdge(state, state.rowComboBoxWidgets[i], state.rowComboBoxWasActive[i]) ||
                        PollWidgetActiveEdge(state, state.rowValueLabels[i].Get(), state.rowValueLabelWasActive[i]))) {
                    state.modSelectorDropDownOpen = false;
                    state.activeEnumDropDownRowIndex = static_cast<long>(i);
                    LogInfo("CoH Mod Config UI: Native ComboBox body clicked for row " + std::to_string(i) + ".");
                }
                if (PollWidgetActiveEdge(state, state.rowArrowButtons[i].Get(), state.rowArrowButtonWasActive[i])) {
                    OnRowControlClicked(state, i);
                }
                if (PollWidgetActiveEdge(state, state.rowCheckButtons[i].Get(), state.rowCheckButtonWasActive[i])) {
                    OnRowControlClicked(state, i);
                }
                if (((state.rowActiveControlType[i] == CoHModSDKConfigType_Int) ||
                    (state.rowActiveControlType[i] == CoHModSDKConfigType_Float)) &&
                    state.rowNativeSliderInitialized[i]) {
                    const float currentProgress = std::clamp(state.rowNativeSliders[i].CurrentValue(), 0.0f, 1.0f);
                    if (!state.rowHasObservedSliderProgress[i]) {
                        state.rowObservedSliderProgress[i] = currentProgress;
                        state.rowHasObservedSliderProgress[i] = true;
                    }
                    else if (std::fabs(currentProgress - state.rowObservedSliderProgress[i]) > kSliderProgressEpsilon) {
                        if (TryApplyNumericRowSliderChange(state, i, currentProgress)) {
                            return;
                        }
                    }
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
        state.selectedModIndex = 0u;
        state.selectedOptionIndex = 0u;
        state.topVisibleOptionIndex = 0u;
        state.modSelectorDropDownOpen = false;
        state.activeEnumDropDownRowIndex = -1;
        state.updateHookObserved = false;
        state.toggleKeyWasDown = false;
        state.pendingMouseWheelDelta = 0;
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

        InstallGameWindowHook(state);


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

        if ((state.gameWindowHandle != nullptr) && (state.originalGameWindowProc != nullptr) && IsWindow(state.gameWindowHandle)) {
            SetWindowLongPtr(state.gameWindowHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(state.originalGameWindowProc));
        }
        state.gameWindowHandle = nullptr;
        state.originalGameWindowProc = nullptr;
        state.pendingMouseWheelDelta = 0;
        state.modSelectorDropDownOpen = false;
        state.activeEnumDropDownRowIndex = -1;

        state.catalog = nullptr;
        state.installed = false;
        state.overlayBuilt = false;
        state.overlayVisible = false;
    }
}
