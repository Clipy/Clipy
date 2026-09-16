import Settings
import Sharing
import SwiftUI

struct MenuSettingsView: View {
    @Shared(.inlineMenuItemLimit)
    private var inlineMenuItemLimit
    @Shared(.folderMenuItemLimit)
    private var folderMenuItemLimit
    @Shared(.maximumMenuItemTitleLength)
    private var maximumMenuItemTitleLength
    @Shared(.allowsDuplicateHistory)
    private var allowsDuplicateHistory
    @Shared(.overwritesDuplicateHistory)
    private var overwritesDuplicateHistory
    @Shared(.showsIconsInMenu)
    private var showsIconsInMenu
    @Shared(.marksMenuItemsWithNumbers)
    private var marksMenuItemsWithNumbers
    @Shared(.startsMenuItemTitlesAtZero)
    private var startsMenuItemTitlesAtZero
    @Shared(.addsNumericKeyEquivalents)
    private var addsNumericKeyEquivalents
    @Shared(.showsClearHistoryMenuItem)
    private var showsClearHistoryMenuItem
    @Shared(.showsClearHistoryAlert)
    private var showsClearHistoryAlert
    @Shared(.showsToolTipsOnMenuItems)
    private var showsToolTipsOnMenuItems
    @Shared(.maximumToolTipLength)
    private var maximumToolTipLength
    @Shared(.showsImagesInMenu)
    private var showsImagesInMenu
    @Shared(.thumbnailWidth)
    private var thumbnailWidth
    @Shared(.thumbnailHeight)
    private var thumbnailHeight
    @Shared(.showsColorPreviewInMenu)
    private var showsColorPreviewInMenu

    var body: some View {
        SettingsGrid {
            SettingsSection(title: .Settings.historyItemsShownAtTheTopLevel) {
                SettingsNumberField(
                    value: Binding($inlineMenuItemLimit),
                    in: 0...100
                )
                Text(.Settings.additionalHistoryIsGroupedIntoFoldersSetTo0ToGroupAllHistoryIntoFolders)
                    .settingDescription()
            }

            SettingsSection(title: .Settings.itemsPerFolder) {
                SettingsNumberField(
                    value: Binding($folderMenuItemLimit),
                    in: 1...100
                )
            }

            SettingsSection(title: .Settings.maximumDisplayedTextLength, bottomDivider: true) {
                SettingsNumberField(
                    value: Binding($maximumMenuItemTitleLength),
                    in: 1...1000,
                    unit: .Settings.characters
                )
            }

            SettingsSection(title: .Settings.clipboardHistory, bottomDivider: true) {
                Toggle(
                    .Settings.saveDuplicateClipboardContent,
                    isOn: Binding($allowsDuplicateHistory)
                )
                Toggle(
                    .Settings.replaceThePreviousEntry,
                    isOn: Binding($overwritesDuplicateHistory)
                )
                .padding(.leading, 20)
                .disabled(!allowsDuplicateHistory)
            }

            SettingsSection(title: .Settings.menuItems, bottomDivider: true) {
                Toggle(
                    .Settings.showIconsInMenuItems,
                    isOn: Binding($showsIconsInMenu)
                )
                Toggle(
                    .Settings.markMenuItemsWithNumbers,
                    isOn: Binding($marksMenuItemsWithNumbers)
                )
                Toggle(
                    .Settings.startNumberingAt0,
                    isOn: Binding($startsMenuItemTitlesAtZero)
                )
                .padding(.leading, 20)
                .disabled(!marksMenuItemsWithNumbers)
                Toggle(
                    .Settings.useNumberKeysAsShortcuts,
                    isOn: Binding($addsNumericKeyEquivalents)
                )
            }

            SettingsSection(title: .Settings.clearHistory, bottomDivider: true) {
                Toggle(
                    .Settings.addAMenuItemToClearClipboardHistory,
                    isOn: Binding($showsClearHistoryMenuItem)
                )
                Toggle(
                    .Settings.showAWarningBeforeClearingHistory,
                    isOn: Binding($showsClearHistoryAlert)
                )
                .padding(.leading, 20)
                .disabled(!showsClearHistoryMenuItem)
            }

            SettingsSection(title: .Settings.tooltips, bottomDivider: true) {
                Toggle(
                    .Settings.showTooltipsOnMenuItems,
                    isOn: Binding($showsToolTipsOnMenuItems)
                )
                HStack {
                    Text(.Settings.maximumCharacters)
                    SettingsNumberField(
                        value: Binding($maximumToolTipLength),
                        in: 1...10000,
                        unit: .Settings.characters
                    )
                }
                .padding(.leading, 20)
                .disabled(!showsToolTipsOnMenuItems)
            }

            SettingsSection(title: .Settings.previews) {
                Toggle(
                    .Settings.showImages,
                    isOn: Binding($showsImagesInMenu)
                )
                HStack(alignment: .firstTextBaseline, spacing: 12) {
                    SettingsNumberField(
                        value: Binding($thumbnailWidth),
                        in: 1...512,
                        unit: .Settings.px,
                        caption: .Settings.width
                    )
                    SettingsNumberField(
                        value: Binding($thumbnailHeight),
                        in: 1...512,
                        unit: .Settings.px,
                        caption: .Settings.height
                    )
                }
                .padding(.leading, 20)
                .disabled(!showsImagesInMenu)
                Toggle(
                    .Settings.showColorCodePreview,
                    isOn: Binding($showsColorPreviewInMenu)
                )
            }
        }
    }
}
