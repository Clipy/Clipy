//
//  SharedExtensionsTests.swift
//
//  ClipyTests
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/29.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Combine
import CustomDump
import DependenciesTestSupport
import Sharing
import Testing
@testable import Clipy

@Suite(.dependencies)
struct SharedExtensionsTests {
    @Test
    func changesExcludesInitialValueAndDuplicatesByDefault() {
        @Shared(.inMemory("changesExcludesInitialValue")) var value = 0
        var receivedValues: [Int] = []
        let cancellable = $value.changes().sink { receivedValues.append($0) }

        $value.withLock { $0 = 1 }
        $value.withLock { $0 = 1 }
        $value.withLock { $0 = 2 }

        expectNoDifference(receivedValues, [1, 2])
        _ = cancellable
    }

    @Test
    func changesCanIncludeInitialValue() {
        @Shared(.inMemory("changesIncludesInitialValue")) var value = 0
        var receivedValues: [Int] = []
        let cancellable = $value.changes(includingInitialValue: true)
            .sink { receivedValues.append($0) }

        $value.withLock { $0 = 1 }

        expectNoDifference(receivedValues, [0, 1])
        _ = cancellable
    }
}
