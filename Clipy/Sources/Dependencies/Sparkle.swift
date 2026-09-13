//
//  Sparkle.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/26.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Combine
import Dependencies
import DependenciesMacros
import Sparkle

@DependencyClient
struct Sparkle {
    var configure: () -> Void
    var setAutomaticallyChecksForUpdates: (_ isEnabled: Bool) -> Void
    var setUpdateCheckInterval: (_ interval: TimeInterval) -> Void
    var automaticallyChecksForUpdates: () -> AnyPublisher<Bool, Never> = { Just(false).eraseToAnyPublisher() }
    var updateCheckInterval: () -> AnyPublisher<TimeInterval, Never> = { Just(TimeInterval(86_400)).eraseToAnyPublisher() }
    var lastUpdateCheckDate: () -> AnyPublisher<Date?, Never> = { Just(nil).eraseToAnyPublisher() }
    var canCheckForUpdates: () -> AnyPublisher<Bool, Never> = { Just(false).eraseToAnyPublisher() }
    var checkForUpdates: (_ sender: Any?) -> Void
}

extension DependencyValues {
    var sparkle: Sparkle {
        get { self[SparkleKey.self] }
        set { self[SparkleKey.self] = newValue }
    }

    private enum SparkleKey: DependencyKey {
        static var liveValue: Sparkle {
            let updaterController = SPUStandardUpdaterController(
                startingUpdater: false,
                updaterDelegate: nil,
                userDriverDelegate: nil
            )

            return Sparkle(
                configure: {
                    updaterController.updater.clearFeedURLFromUserDefaults()
                    updaterController.startUpdater()
                },
                setAutomaticallyChecksForUpdates: { isEnabled in
                    updaterController.updater.automaticallyChecksForUpdates = isEnabled
                },
                setUpdateCheckInterval: { interval in
                    updaterController.updater.updateCheckInterval = interval
                },
                automaticallyChecksForUpdates: {
                    updaterController.updater.publisher(for: \.automaticallyChecksForUpdates)
                        .eraseToAnyPublisher()
                },
                updateCheckInterval: {
                    updaterController.updater.publisher(for: \.updateCheckInterval)
                        .eraseToAnyPublisher()
                },
                lastUpdateCheckDate: {
                    updaterController.updater.publisher(for: \.lastUpdateCheckDate)
                        .eraseToAnyPublisher()
                },
                canCheckForUpdates: {
                    updaterController.updater.publisher(for: \.canCheckForUpdates)
                        .eraseToAnyPublisher()
                },
                checkForUpdates: { sender in
                    updaterController.checkForUpdates(sender)
                }
            )
        }
    }
}
