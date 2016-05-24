//
//  RLMResults+Array.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/07/02.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Realm

// MARK: - Saved Realm Value
extension RLMResults {
    func arrayValue<T: RLMObject>(_: T.Type) -> [T] {
        return flatMap { $0 as? T }
    }
}

extension RLMArray {
    func arrayValue<T: RLMObject>(_: T.Type) -> [T] {
        return flatMap { $0 as? T }
    }
}

// MARK: Remove Object
extension RLMArray {
    func removeObject<T: RLMObject>(element: T) {
        let index = indexOfObject(element)
        if Int(index) == NSNotFound { return }
        removeObjectAtIndex(index)
    }
}
