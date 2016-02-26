//
//  OptionalType.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/02/26.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

protocol OptionalType {
    typealias Wrapped
    var value: Wrapped? { get }
}

extension Optional: OptionalType {
    var value: Wrapped? {
        return self
    }
}
