//
//  Array+Remove.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/07/06.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension Array {
    mutating func removeObject<T: Equatable>(element: T) {
        self = filter { $0 as? T != element }
    }
}

extension Array {
    mutating func removeObjects<T: Equatable>(elements: [T]) {
        elements.forEach { removeObject($0) }
    }
}
