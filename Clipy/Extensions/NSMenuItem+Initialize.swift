//
//  NSMenuItem+Initialize.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/06.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension NSMenuItem {
    convenience init(title: String, action: Selector) {
        self.init(title: title, action: action, keyEquivalent: kEmptyString)
    }
}
