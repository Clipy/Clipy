//
//  NSImage+AssetCatalog.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/10/23.
//  Copyright © 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension NSImage {
    
    // MARK: - Enum Value
    enum AssetIdentifier: String {
        // Icons
        case IconFolder = "icon_folder"
        case IconText   = "icon_text"
        
        // MenuIcons
        case MenuBlack = "statusbar_menu_black"
        case MenuWhite = "statusbar_menu_white"
        
        // SettingTabIcons
        case Menu               = "Menu"
        case IconApplication    = "icon_application"
        case IconKeyboard       = "PTKeyboardIcon"
        case IconSparkle        = "SparkleIcon"
    }
    
    // MARK: - Initialize
    convenience init!(assetIdentifier: AssetIdentifier) {
        self.init(named: assetIdentifier.rawValue)
    }
    
}
