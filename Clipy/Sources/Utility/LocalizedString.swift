//
//  LocalizedString.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/03/06.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation

enum LocalizedString: String {
    case clearHistory           = "Clear History"
    case confirmClearHistory    = "Are you sure you want to clear your clipboard history?"
    case editSnippets           = "Edit Snippets..."
    case preference             = "Preferences..."
    case quitClipy              = "Quit Clipy"
    case history                = "History"
    case snippet                = "Snippet"
    case cancel                 = "Cancel"
    case launchClipy            = "Launch Clipy on system startup?"
    case launchSettingInfo      = "You can change this setting in the Preferences if you want."
    case launchOnStartup        = "Launch on system startup"
    case dontLaunch             = "Don't Launch"
    case deleteItem             = "Delete Item"
    case confirmDeleteItem      = "Are you sure want to delete this item?"
    case add                    = "Add"

    case tabGeneral             = "General"
    case tabMenu                = "Menu"
    case tabType                = "Type"
    case tabShortcuts           = "Shortcuts"
    case tabUpdates             = "Updates"

    var value: String {
        return NSLocalizedString(rawValue, comment: "")
    }
}
