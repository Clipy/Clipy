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
    let menuManager: MenuManager

    let defaults: UserDefaults

    // MARK: - Initialize
    init(clipService: ClipService = ClipService(),
         hotKeyService: HotKeyService = HotKeyService(),
         dataCleanService: DataCleanService = DataCleanService(),
         pasteService: PasteService = PasteService(),
         excludeAppService: ExcludeAppService = ExcludeAppService(applications: []),
         menuManager: MenuManager = MenuManager(),
         defaults: UserDefaults = .standard) {

        self.clipService = clipService
        self.hotKeyService = hotKeyService
        self.dataCleanService = dataCleanService
        self.pasteService = pasteService
        self.excludeAppService = excludeAppService
        self.menuManager = menuManager
        self.defaults = defaults
    }

}
