//
//  MenuType.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/06/26.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

enum MenuType: String {
    case Main       = "ClipMenu"
    case History    = "HistoryMenu"
    case Snippet    = "SnippetMenu"

    var userDefaultsKey: String {
        switch self {
        case .Main:
            return Constants.HotKey.mainKeyCombo
        case .History:
            return Constants.HotKey.historyKeyCombo
        case .Snippet:
            return Constants.HotKey.snippetKeyCombo
        }
    }

    var hotKeySelector: Selector {
        switch self {
        case .Main:
            return #selector(HotKeyManager.popUpClipMenu)
        case .History:
            return #selector(HotKeyManager.popUpHistoryMenu)
        case .Snippet:
            return #selector(HotKeyManager.popUpSnippetMenu)
        }
    }

}
