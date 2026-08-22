//
//  FileManagerExtensions.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/22.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation

extension FileManager {
    func removeLegacyHistoryCacheDirectory() throws {
        let url = try url(for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: false)
            .appending(path: Constants.Application.name, directoryHint: .isDirectory)
        try removeItem(at: url)
    }
}
