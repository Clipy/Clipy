//
//  Environment.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2017/08/10.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation

struct Environment {

    // MARK: - Properties
    let clipService: ClipService
    let hotKeyService: HotKeyService
    let dataCleanService: DataCleanService
    let pasteService: PasteService
    let excludeAppService: ExcludeAppService
    let accessibilityService: AccessibilityService
    let menuService: MenuService

    let defaults: UserDefaults

    // MARK: - Initialize
    init(clipService: ClipService = ClipService(),
         hotKeyService: HotKeyService = HotKeyService(),
         dataCleanService: DataCleanService = DataCleanService(),
         pasteService: PasteService = PasteService(),
         excludeAppService: ExcludeAppService = ExcludeAppService(applications: []),
         accessibilityService: AccessibilityService = AccessibilityService(),
         menuService: MenuService = MenuService(),
         defaults: UserDefaults = .standard) {

        self.clipService = clipService
        self.hotKeyService = hotKeyService
        self.dataCleanService = dataCleanService
        self.pasteService = pasteService
        self.excludeAppService = excludeAppService
        self.accessibilityService = accessibilityService
        self.menuService = menuService
        self.defaults = defaults
    }

}

// MARK: Beta
extension Environment {

    var pinHistoryEnabled: Bool {
        return defaults.bool(forKey: Constants.Beta.pinHistory)
    }

    var pinHistoryModifier: Int {
        return defaults.integer(forKey: Constants.Beta.pinHistoryModifier)
    }

    var deleteHistoryEnabled: Bool {
        return defaults.bool(forKey: Constants.Beta.deleteHistory)
    }

    var deleteHistoryModifier: Int {
        return defaults.integer(forKey: Constants.Beta.deleteHistoryModifier)
    }

    var pasteAndDeleteHistoryEnabled: Bool {
        return defaults.bool(forKey: Constants.Beta.pasteAndDeleteHistory)
    }

    var pasteAndDeleteHistoryModifier: Int {
        return defaults.integer(forKey: Constants.Beta.pasteAndDeleteHistoryModifier)
    }

    var pastePlainTextEnabled: Bool {
        return defaults.bool(forKey: Constants.Beta.pastePlainText)
    }

    var pastePlainTextModifier: Int {
        return defaults.integer(forKey: Constants.Beta.pastePlainTextModifier)
    }

    var hidePinnedHistory: Bool {
        return pinHistoryEnabled && defaults.bool(forKey: Constants.Beta.hidePinnedHistory)
    }
}

// MARK: Menu
extension Environment {
    var maxHistorySize: Int {
        return defaults.integer(forKey: Constants.UserDefaults.maxHistorySize)
    }

    var maxLengthOfToolTip: Int {
        return defaults.integer(forKey: Constants.UserDefaults.maxLengthOfToolTip)
    }

    var reorderClipsAfterPasting: Bool {
        return defaults.bool(forKey: Constants.UserDefaults.reorderClipsAfterPasting)
    }

    var showIconInTheMenu: Bool {
        return defaults.bool(forKey: Constants.UserDefaults.showIconInTheMenu)
    }
}
