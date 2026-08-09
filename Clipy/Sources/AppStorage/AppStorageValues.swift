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

import Magnet
import Sharing

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
