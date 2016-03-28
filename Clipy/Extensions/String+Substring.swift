//
//  String+Substring.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/17.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension String {
    subscript (range: Range<Int>) -> String {
        let startIndex = self.startIndex.advancedBy(range.startIndex, limit:  self.endIndex)
        let endIndex = self.startIndex.advancedBy(range.endIndex, limit: self.endIndex)

        return self[startIndex..<endIndex]
    }
}
