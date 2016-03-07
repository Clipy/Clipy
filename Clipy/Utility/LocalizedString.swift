//
//  LocalizedString.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/06.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

enum LocalizedString: String {
    case ClearHistory   = "Clear History"
    case EditSnippets   = "Edit Snippets..."
    case Preference     = "Preferences..."
    case QuitClipy      = "Quit Clipy"
    case History        = "History"
    case Snippet        = "Snippet"
 
    var value: String {
        return NSLocalizedString(rawValue, comment: "")
    }
}
