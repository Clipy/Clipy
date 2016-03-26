//
//  String+Substring.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/17.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension String {
    subscript (r: Range<Int>) -> String {
        let startIndex = self.startIndex.advancedBy(r.startIndex, limit:  self.endIndex)
        let endIndex = self.startIndex.advancedBy(r.endIndex, limit: self.endIndex)
        
        return self[Range(start: startIndex, end: endIndex)]
    }
}
