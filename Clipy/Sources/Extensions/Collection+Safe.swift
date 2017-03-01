//
//  Collection+Safe.swift
//  Clipy
//
//  Created by 古林俊佑 on 2017/03/01.
//  Copyright © 2017年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension Collection {
    subscript(safe index: Index) -> _Element? {
        return index >= startIndex && index < endIndex ? self[index] : nil
    }
}
