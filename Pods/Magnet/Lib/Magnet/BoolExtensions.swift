//
//  BoolExtensions.swift
//  Magnet
//
//  Created by 古林俊佑 on 2018/09/22.
//  Copyright © 2018年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension Bool {
    var intValue: Int {
        return NSNumber(value: self).intValue
    }
}
