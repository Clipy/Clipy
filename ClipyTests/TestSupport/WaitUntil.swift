// 
//  WaitUntil.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
// 
//  Created by Shunsuke Furubayashi on 2026/07/10.
// 
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation
import Testing

func waitUntil(
    interval: TimeInterval = 0.01,
    condition: @escaping @MainActor () async -> Bool
) async throws {
    try await confirmation { confirmation in
        while true {
            if await condition() {
                confirmation()
                return
            } else {
                try await Task.sleep(for: .seconds(interval))
            }
        }
    }
}
