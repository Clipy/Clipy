//
//  Environment.swift
//  Clipy
//
//  Created by 古林俊佑 on 2017/08/10.
//  Copyright © 2017年 Shunsuke Furubayashi. All rights reserved.
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

    let defaults: KeyValueStorable

    // MARK: - Initialize
    init(clipService: ClipService = ClipService(),
         hotKeyService: HotKeyService = HotKeyService(),
         dataCleanService: DataCleanService = DataCleanService(),
         pasteService: PasteService = PasteService(),
         excludeAppService: ExcludeAppService = ExcludeAppService(applications: []),
         menuManager: MenuManager = MenuManager(),
         defaults: KeyValueStorable = UserDefaults.standard) {

        self.clipService = clipService
        self.hotKeyService = hotKeyService
        self.dataCleanService = dataCleanService
        self.pasteService = pasteService
        self.excludeAppService = excludeAppService
        self.menuManager = menuManager
        self.defaults = defaults
    }

}
