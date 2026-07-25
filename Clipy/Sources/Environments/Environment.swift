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

struct Environment {

    // MARK: - Properties
    let clipService: ClipService
    let hotKeyService: HotKeyService
    let pasteService: PasteService
    let menuManager: MenuManager

    // MARK: - Initialize
    init(clipService: ClipService = ClipService(),
         hotKeyService: HotKeyService = HotKeyService(),
         pasteService: PasteService = PasteService(),
         menuManager: MenuManager = MenuManager()) {
        self.clipService = clipService
        self.hotKeyService = hotKeyService
        self.pasteService = pasteService
        self.menuManager = menuManager
    }

}
