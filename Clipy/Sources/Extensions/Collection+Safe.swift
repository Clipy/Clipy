//
//  Collection+Safe.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2017/03/01.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation

extension Collection {
    subscript(safe index: Index) -> Element? {
        return index >= startIndex && index < endIndex ? self[index] : nil
    }
}
