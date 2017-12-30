//
//  NSColor+Hex.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/11/21.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//
// Rewrote it for NSColor with reference to the following
// https://github.com/yeahdongcn/UIColor-Hex-Swift

import Cocoa

extension NSColor {

    convenience init(hex3: UInt16, alpha: CGFloat = 1) {
        let divisor = CGFloat(16)
        let red     = CGFloat((hex3 & 0xF00) >> 8) / divisor
        let green   = CGFloat((hex3 & 0x0F0) >> 4) / divisor
        let blue    = CGFloat( hex3 & 0x00F      ) / divisor
        self.init(red: red, green: green, blue: blue, alpha: alpha)
    }

    convenience init(hex4: UInt16) {
        let divisor = CGFloat(15)
        let red     = CGFloat((hex4 & 0xF000) >> 12) / divisor
        let green   = CGFloat((hex4 & 0x0F00) >>  8) / divisor
        let blue    = CGFloat((hex4 & 0x00F0) >>  4) / divisor
        let alpha   = CGFloat( hex4 & 0x000F       ) / divisor
        self.init(red: red, green: green, blue: blue, alpha: alpha)
    }

    public convenience init(hex6: UInt32, alpha: CGFloat = 1) {
        let divisor = CGFloat(255)
        let red     = CGFloat((hex6 & 0xFF0000) >> 16) / divisor
        let green   = CGFloat((hex6 & 0x00FF00) >>  8) / divisor
        let blue    = CGFloat( hex6 & 0x0000FF       ) / divisor
        self.init(red: red, green: green, blue: blue, alpha: alpha)
    }

    public convenience init(hex8: UInt32) {
        let divisor = CGFloat(255)
        let red     = CGFloat((hex8 & 0xFF000000) >> 24) / divisor
        let green   = CGFloat((hex8 & 0x00FF0000) >> 16) / divisor
        let blue    = CGFloat((hex8 & 0x0000FF00) >>  8) / divisor
        let alpha   = CGFloat( hex8 & 0x000000FF       ) / divisor
        self.init(red: red, green: green, blue: blue, alpha: alpha)
    }

    public convenience init?(hex rgba: String) {
        let hexString: String
        let hexCount = rgba.count
        if rgba.hasPrefix("#") {
            hexString = rgba.substring(from: rgba.index(rgba.startIndex, offsetBy: 1))
        } else if hexCount == 3 || hexCount == 6 {
            hexString = rgba
        } else {
            return nil
        }
        var hexValue: UInt32 = 0

        guard Scanner(string: hexString).scanHexInt32(&hexValue) else { return nil }

        /**
         *  Images with alpha cannot be previewed and not be created
         */
        switch hexString.count {
        case 3:
            self.init(hex3: UInt16(hexValue))
        case 6:
            self.init(hex6: hexValue)
        default:
            return nil
        }
    }

}
