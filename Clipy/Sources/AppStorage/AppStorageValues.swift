//
//  AppStorageValues.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/09.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation
import Magnet
import Sharing

enum AppStorageValues {
    /// Registers default values for settings accessed through Cocoa Bindings in XIB files.
    static func register(in defaults: UserDefaults = .standard) {
        defaults.register(
            defaults: [
                AppStorageKeys.loginItem.rawValue: AppStorageKey<Bool>.Default.isLaunchAtLogin.initialValue,
                AppStorageKeys.suppressAlertForLoginItem.rawValue: AppStorageKey<Bool>.Default.suppressesLoginItemAlert.initialValue,
                AppStorageKeys.inputPasteCommand.rawValue: AppStorageKey<Bool>.Default.pastesAutomatically.initialValue,
                AppStorageKeys.reorderClipsAfterPasting.rawValue: AppStorageKey<Bool>.Default.reordersClipsAfterPasting.initialValue,
                AppStorageKeys.collectCrashReport.rawValue: AppStorageKey<Bool>.Default.collectsCrashReports.initialValue,
                AppStorageKeys.menuItemsTitleStartWithZero.rawValue: AppStorageKey<Bool>.Default.startsMenuItemTitlesAtZero.initialValue,
                AppStorageKeys.showAlertBeforeClearHistory.rawValue: AppStorageKey<Bool>.Default.showsClearHistoryAlert.initialValue,
                AppStorageKeys.addClearHistoryMenuItem.rawValue: AppStorageKey<Bool>.Default.showsClearHistoryMenuItem.initialValue,
                AppStorageKeys.showIconInTheMenu.rawValue: AppStorageKey<Bool>.Default.showsIconsInMenu.initialValue,
                AppStorageKeys.menuItemsAreMarkedWithNumbers.rawValue: AppStorageKey<Bool>.Default.marksMenuItemsWithNumbers.initialValue,
                AppStorageKeys.showToolTipOnMenuItem.rawValue: AppStorageKey<Bool>.Default.showsToolTipsOnMenuItems.initialValue,
                AppStorageKeys.showImageInTheMenu.rawValue: AppStorageKey<Bool>.Default.showsImagesInMenu.initialValue,
                AppStorageKeys.addNumericKeyEquivalents.rawValue: AppStorageKey<Bool>.Default.addsNumericKeyEquivalents.initialValue,
                AppStorageKeys.overwriteSameHistory.rawValue: AppStorageKey<Bool>.Default.overwritesDuplicateHistory.initialValue,
                AppStorageKeys.copySameHistory.rawValue: AppStorageKey<Bool>.Default.allowsDuplicateHistory.initialValue,
                AppStorageKeys.showColorPreviewInTheMenu.rawValue: AppStorageKey<Bool>.Default.showsColorPreviewInMenu.initialValue,
                AppStorageKeys.ignoreConcealedPasteboardType.rawValue: AppStorageKey<Bool>.Default.ignoresConcealedPasteboardTypes.initialValue,
                AppStorageKeys.enableAutomaticCheck.rawValue: AppStorageKey<Bool>.Default.checksForUpdatesAutomatically.initialValue,
                AppStorageKeys.pastePlainText.rawValue: AppStorageKey<Bool>.Default.pastesPlainTextWithModifier.initialValue,
                AppStorageKeys.deleteHistory.rawValue: AppStorageKey<Bool>.Default.deletesHistoryWithModifier.initialValue,
                AppStorageKeys.pasteAndDeleteHistory.rawValue: AppStorageKey<Bool>.Default.pastesAndDeletesHistoryWithModifier.initialValue,
                AppStorageKeys.observerScreenshot.rawValue: AppStorageKey<Bool>.Default.observesScreenshots.initialValue,
                AppStorageKeys.maxHistorySize.rawValue: AppStorageKey<Int>.Default.maximumHistoryCount.initialValue,
                AppStorageKeys.showStatusItem.rawValue: AppStorageKey<Int>.Default.statusItemDisplayMode.initialValue,
                AppStorageKeys.menuIconSize.rawValue: AppStorageKey<Int>.Default.menuIconSize.initialValue,
                AppStorageKeys.maxMenuItemTitleLength.rawValue: AppStorageKey<Int>.Default.maximumMenuItemTitleLength.initialValue,
                AppStorageKeys.numberOfItemsPlaceInline.rawValue: AppStorageKey<Int>.Default.inlineMenuItemLimit.initialValue,
                AppStorageKeys.numberOfItemsPlaceInsideFolder.rawValue: AppStorageKey<Int>.Default.folderMenuItemLimit.initialValue,
                AppStorageKeys.maxLengthOfToolTip.rawValue: AppStorageKey<Int>.Default.maximumToolTipLength.initialValue,
                AppStorageKeys.thumbnailWidth.rawValue: AppStorageKey<Int>.Default.thumbnailWidth.initialValue,
                AppStorageKeys.thumbnailHeight.rawValue: AppStorageKey<Int>.Default.thumbnailHeight.initialValue,
                AppStorageKeys.checkInterval.rawValue: AppStorageKey<Int>.Default.updateCheckInterval.initialValue,
                AppStorageKeys.pastePlainTextModifier.rawValue: AppStorageKey<Int>.Default.plainTextPasteModifier.initialValue,
                AppStorageKeys.deleteHistoryModifier.rawValue: AppStorageKey<Int>.Default.historyDeletionModifier.initialValue,
                AppStorageKeys.pasteAndDeleteHistoryModifier.rawValue: AppStorageKey<Int>.Default.pasteAndDeleteHistoryModifier.initialValue
            ]
        )
    }
}

extension SharedKey where Self == AppStorageKey<Bool>.Default {
    static var isLaunchAtLogin: Self {
        Self[.appStorage(AppStorageKeys.loginItem.rawValue), default: false]
    }

    static var suppressesLoginItemAlert: Self {
        Self[.appStorage(AppStorageKeys.suppressAlertForLoginItem.rawValue), default: false]
    }

    static var pastesAutomatically: Self {
        Self[.appStorage(AppStorageKeys.inputPasteCommand.rawValue), default: true]
    }

    static var reordersClipsAfterPasting: Self {
        Self[.appStorage(AppStorageKeys.reorderClipsAfterPasting.rawValue), default: true]
    }

    static var collectsCrashReports: Self {
        Self[.appStorage(AppStorageKeys.collectCrashReport.rawValue), default: true]
    }

    static var startsMenuItemTitlesAtZero: Self {
        Self[.appStorage(AppStorageKeys.menuItemsTitleStartWithZero.rawValue), default: false]
    }

    static var showsClearHistoryAlert: Self {
        Self[.appStorage(AppStorageKeys.showAlertBeforeClearHistory.rawValue), default: true]
    }

    static var showsClearHistoryMenuItem: Self {
        Self[.appStorage(AppStorageKeys.addClearHistoryMenuItem.rawValue), default: true]
    }

    static var showsIconsInMenu: Self {
        Self[.appStorage(AppStorageKeys.showIconInTheMenu.rawValue), default: true]
    }

    static var marksMenuItemsWithNumbers: Self {
        Self[.appStorage(AppStorageKeys.menuItemsAreMarkedWithNumbers.rawValue), default: true]
    }

    static var showsToolTipsOnMenuItems: Self {
        Self[.appStorage(AppStorageKeys.showToolTipOnMenuItem.rawValue), default: true]
    }

    static var showsImagesInMenu: Self {
        Self[.appStorage(AppStorageKeys.showImageInTheMenu.rawValue), default: true]
    }

    static var addsNumericKeyEquivalents: Self {
        Self[.appStorage(AppStorageKeys.addNumericKeyEquivalents.rawValue), default: false]
    }

    static var overwritesDuplicateHistory: Self {
        Self[.appStorage(AppStorageKeys.overwriteSameHistory.rawValue), default: true]
    }

    static var allowsDuplicateHistory: Self {
        Self[.appStorage(AppStorageKeys.copySameHistory.rawValue), default: true]
    }

    static var showsColorPreviewInMenu: Self {
        Self[.appStorage(AppStorageKeys.showColorPreviewInTheMenu.rawValue), default: true]
    }

    static var ignoresConcealedPasteboardTypes: Self {
        Self[.appStorage(AppStorageKeys.ignoreConcealedPasteboardType.rawValue), default: false]
    }

    static var checksForUpdatesAutomatically: Self {
        Self[.appStorage(AppStorageKeys.enableAutomaticCheck.rawValue), default: true]
    }

    static var pastesPlainTextWithModifier: Self {
        Self[.appStorage(AppStorageKeys.pastePlainText.rawValue), default: true]
    }

    static var deletesHistoryWithModifier: Self {
        Self[.appStorage(AppStorageKeys.deleteHistory.rawValue), default: false]
    }

    static var pastesAndDeletesHistoryWithModifier: Self {
        Self[.appStorage(AppStorageKeys.pasteAndDeleteHistory.rawValue), default: false]
    }

    static var observesScreenshots: Self {
        Self[.appStorage(AppStorageKeys.observerScreenshot.rawValue), default: false]
    }
}

extension SharedKey where Self == AppStorageKey<Int>.Default {
    static var maximumHistoryCount: Self {
        Self[.appStorage(AppStorageKeys.maxHistorySize.rawValue), default: 30]
    }

    static var statusItemDisplayMode: Self {
        Self[.appStorage(AppStorageKeys.showStatusItem.rawValue), default: 1]
    }

    static var menuIconSize: Self {
        Self[.appStorage(AppStorageKeys.menuIconSize.rawValue), default: 16]
    }

    static var maximumMenuItemTitleLength: Self {
        Self[.appStorage(AppStorageKeys.maxMenuItemTitleLength.rawValue), default: 20]
    }

    static var inlineMenuItemLimit: Self {
        Self[.appStorage(AppStorageKeys.numberOfItemsPlaceInline.rawValue), default: 0]
    }

    static var folderMenuItemLimit: Self {
        Self[.appStorage(AppStorageKeys.numberOfItemsPlaceInsideFolder.rawValue), default: 10]
    }

    static var maximumToolTipLength: Self {
        Self[.appStorage(AppStorageKeys.maxLengthOfToolTip.rawValue), default: 200]
    }

    static var thumbnailWidth: Self {
        Self[.appStorage(AppStorageKeys.thumbnailWidth.rawValue), default: 100]
    }

    static var thumbnailHeight: Self {
        Self[.appStorage(AppStorageKeys.thumbnailHeight.rawValue), default: 32]
    }

    static var updateCheckInterval: Self {
        Self[.appStorage(AppStorageKeys.checkInterval.rawValue), default: 86_400]
    }

    static var plainTextPasteModifier: Self {
        Self[.appStorage(AppStorageKeys.pastePlainTextModifier.rawValue), default: 0]
    }

    static var historyDeletionModifier: Self {
        Self[.appStorage(AppStorageKeys.deleteHistoryModifier.rawValue), default: 0]
    }

    static var pasteAndDeleteHistoryModifier: Self {
        Self[.appStorage(AppStorageKeys.pasteAndDeleteHistoryModifier.rawValue), default: 0]
    }
}

extension SharedKey where Self == AppStorageKey<PasteboardTypeSettings>.Default {
    static var pasteboardTypeSettings: Self {
        Self[
            .appStorage(AppStorageKeys.pasteboardTypeSettings.rawValue),
            default: PasteboardTypeSettings()
        ]
    }
}

extension SharedKey where Self == AppStorageKey<KeyCombo?>.Default {
    static var mainKeyCombo: Self {
        Self[
            .appStorage(AppStorageKeys.mainKeyCombo.rawValue),
            // ⌘ + Shift + V
            default: KeyCombo(QWERTYKeyCode: 9, carbonModifiers: 768)
        ]
    }

    static var historyKeyCombo: Self {
        Self[
            .appStorage(AppStorageKeys.historyKeyCombo.rawValue),
            // ⌘ + Control + V
            default: KeyCombo(QWERTYKeyCode: 9, carbonModifiers: 4352)
        ]
    }

    static var snippetKeyCombo: Self {
        Self[
            .appStorage(AppStorageKeys.snippetKeyCombo.rawValue),
            // ⌘ + Shift + B
            default: KeyCombo(QWERTYKeyCode: 11, carbonModifiers: 768)
        ]
    }

    static var editSnippetsKeyCombo: Self {
        Self[.appStorage(AppStorageKeys.editSnippetsKeyCombo.rawValue), default: nil]
    }

    static var clearHistoryKeyCombo: Self {
        Self[.appStorage(AppStorageKeys.clearHistoryKeyCombo.rawValue), default: nil]
    }
}

extension SharedKey where Self == AppStorageKey<[String: KeyCombo]>.Default {
    static var folderKeyCombos: Self {
        Self[.appStorage(AppStorageKeys.folderKeyCombos.rawValue), default: [:]]
    }
}

extension SharedKey where Self == AppStorageKey<[ApplicationInformation]>.Default {
    static var excludedApplications: Self {
        Self[.appStorage(AppStorageKeys.excludedApplications.rawValue), default: []]
    }
}

extension KeyCombo: @retroactive @unchecked Sendable {}
