//
//  Realm+NoCatch.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/11.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import RealmSwift

extension Realm {
    func transaction(@noescape block: (() throws -> Void)) {
        do {
            try write(block)
        } catch {}
    }
}
