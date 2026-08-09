//
//  AppStorageValuesTests.swift
//
//  ClipyTests
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/08.
//
//  Copyright © 2015-2026 Clipy Project.
//

import DependenciesTestSupport
import Sharing
import Testing
@testable import Clipy

@Suite(.dependencies)
struct AppStorageValuesTests {
    @Test
    func loadDefaultPasteboardTypeSettings() {
        @Shared(.pasteboardTypeSettings) var pasteboardTypeSettings

        #expect(PasteboardAvailableType.allCases.allSatisfy { pasteboardTypeSettings[$0] })
    }
}
