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

import Sharing

extension SharedKey where Self == AppStorageKey<PasteboardTypeSettings>.Default {
    static var pasteboardTypeSettings: Self {
        Self[
            .appStorage(AppStorageKeys.pasteboardTypeSettings.rawValue),
            default: PasteboardTypeSettings()
        ]
    }
}
