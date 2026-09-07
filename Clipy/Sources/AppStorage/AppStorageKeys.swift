//
//  AppStorageKeys.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/08.
//
//  Copyright © 2015-2026 Clipy Project.
//

/// These case names are persisted as UserDefaults keys. Do not rename or reuse them without a migration.
enum AppStorageKeys: String {
    case menuIconSize = "kCPYPrefMenuIconSizeKey"
    case maxHistorySize = "kCPYPrefMaxHistorySizeKey"
    case inputPasteCommand = "kCPYPrefInputPasteCommandKey"
    case showIconInTheMenu = "kCPYPrefShowIconInTheMenuKey"
    case numberOfItemsPlaceInline = "kCPYPrefNumberOfItemsPlaceInlineKey"
    case numberOfItemsPlaceInsideFolder = "kCPYPrefNumberOfItemsPlaceInsideFolderKey"
    case maxMenuItemTitleLength = "kCPYPrefMaxMenuItemTitleLengthKey"
    case menuItemsTitleStartWithZero = "kCPYPrefMenuItemsTitleStartWithZeroKey"
    case reorderClipsAfterPasting = "kCPYPrefReorderClipsAfterPasting"
    case addClearHistoryMenuItem = "kCPYPrefAddClearHistoryMenuItemKey"
    case showAlertBeforeClearHistory = "kCPYPrefShowAlertBeforeClearHistoryKey"
    case menuItemsAreMarkedWithNumbers
    case showToolTipOnMenuItem
    case showImageInTheMenu
    case addNumericKeyEquivalents
    case maxLengthOfToolTip = "maxLengthOfToolTipKey"
    case loginItem
    case suppressAlertForLoginItem
    case showStatusItem = "kCPYPrefShowStatusItemKey"
    case thumbnailWidth
    case thumbnailHeight
    case overwriteSameHistory = "kCPYPrefOverwriteSameHistroy"
    case copySameHistory = "kCPYPrefCopySameHistroy"
    case collectCrashReport = "kCPYCollectCrashReport"
    case showColorPreviewInTheMenu = "kCPYPrefShowColorPreviewInTheMenu"
    case ignoreConcealedPasteboardType = "kCPYPrefIgnoreConcealedPasteboardType"
    case enableAutomaticCheck = "kCPYEnableAutomaticCheckKey"
    case checkInterval = "kCPYUpdateCheckIntervalKey"
    case pastePlainText = "kCPYBetaPastePlainText"
    case pastePlainTextModifier = "kCPYBetaPastePlainTextModifier"
    case deleteHistory = "kCPYBetaDeleteHistory"
    case deleteHistoryModifier = "kCPYBetaDeleteHistoryModifier"
    case pasteAndDeleteHistory = "kCPYBetaPasteAndDeleteHistory"
    case pasteAndDeleteHistoryModifier = "kCPYBetapasteAndDeleteHistoryModifier"
    case observerScreenshot = "kCPYBetaObserveScreenshot"
    case pasteboardTypeSettings
    case excludedApplications
    case mainKeyCombo
    case historyKeyCombo
    case snippetKeyCombo
    case clearHistoryKeyCombo
    case pasteSecondHistoryItemKeyCombo
    case folderKeyCombos
}
