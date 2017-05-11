//
//  NSImage+AssetCatalog.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/10/23.
//  Copyright © 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa

extension NSImage {

    // MARK: - Enum Value
    enum AssetIdentifier: String {
        // Icons
        case iconFolder = "icon_folder"
        case iconText   = "icon_text"

        // MenuIcons
        case menuBlack = "statusbar_menu_black"
        case menuWhite = "statusbar_menu_white"

        // Snippets Editor
        case snippetsIconFolder         = "snippets_icon_folder_blue"
        case snippetsIconFolderWhite    = "snippets_icon_folder_white"

        // Preferences
        case generalOff     = "pref_general"
        case menuOff        = "pref_menu"
        case typeOff        = "pref_type"
        case excludedOff    = "pref_excluded"
        case shortcutsOff   = "pref_shortcut"
        case updatesOff     = "pref_update"
        case betaOff        = "pref_beta"

        case generalOn      = "pref_general_on"
        case menuOn         = "pref_menu_on"
        case typeOn         = "pref_type_on"
        case excludedOn     = "pref_excluded_on"
        case shortcutsOn    = "pref_shortcut_on"
        case updatesOn      = "pref_update_on"
        case betaOn         = "pref_beta_on"
    }

    // MARK: - Initialize
    convenience init(assetIdentifier: AssetIdentifier) {
        self.init(named: assetIdentifier.rawValue)!
    }

}
