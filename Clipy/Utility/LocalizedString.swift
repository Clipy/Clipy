//
//  LocalizedString.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/06.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

enum LocalizedString: String {
    case ClearHistory           = "Clear History"
    case ConfirmClearHistory    = "Are you sure you want to clear your clipboard history?"
    case EditSnippets           = "Edit Snippets..."
    case Preference             = "Preferences..."
    case QuitClipy              = "Quit Clipy"
    case History                = "History"
    case Snippet                = "Snippet"
    case Cancel                 = "Cancel"
    case LaunchClipy            = "Launch Clipy on system startup?"
    case LaunchSettingInfo      = "You can change this setting in the Preferences if you want."
    case LaunchOnStartup        = "Launch on system startup"
    case DontLaunch             = "Don't Launch"
    case DeleteItem             = "Delete Item"
    case ConfirmDeleteItem      = "Are you sure want to delete this item?"

    case TabGeneral             = "General"
    case TabMenu                = "Menu"
    case TabType                = "Type"
    case TabShortcuts           = "Shortcuts"
    case TabUpdates             = "Updates"

    var value: String {
        return NSLocalizedString(rawValue, comment: "")
    }
}
