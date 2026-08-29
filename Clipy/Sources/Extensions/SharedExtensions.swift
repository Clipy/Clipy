//
//  SharedExtensions.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/29.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Combine
import Sharing

extension Shared where Value: Equatable {
    func changes(includingInitialValue: Bool = false) -> AnyPublisher<Value, Never> {
        let changes = publisher.removeDuplicates()
        guard !includingInitialValue else { return changes.eraseToAnyPublisher() }
        return changes.dropFirst().eraseToAnyPublisher()
    }

    func changeEvents(includingInitialValue: Bool = false) -> AnyPublisher<Void, Never> {
        changes(includingInitialValue: includingInitialValue)
            .map { _ in }
            .eraseToAnyPublisher()
    }
}
