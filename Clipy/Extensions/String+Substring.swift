//
//  String+Substring.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/17.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension String {
    func trimTitle(index: Int) -> String {
        let theString = stringByTrimmingCharactersInSet(NSCharacterSet.whitespaceAndNewlineCharacterSet()) as NSString
        let aRange = NSMakeRange(0, 0)
        var lineStart = 0, lineEnd = 0, contentsEnd = 0
        theString.getLineStart(&lineStart, end: &lineEnd, contentsEnd: &contentsEnd, forRange: aRange)
        
        var titleString = (lineEnd == theString.length) ? theString : theString.substringToIndex(contentsEnd)
        if titleString.length > index {
            titleString = titleString.substringToIndex(index)
        }
        return titleString as String
    }
}
