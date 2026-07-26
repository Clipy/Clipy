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
import Sharing
import Sparkle

@DependencyClient
struct Sparkle {
    var configure: () -> Void
    var lastUpdateCheckDate: () -> AnyPublisher<Any?, Never> = { Empty().eraseToAnyPublisher() }
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
                    @Dependency(\.defaultAppStorage) var appStorage

                    if appStorage.bool(forKey: Constants.Update.enableAutomaticCheck) {
                        updaterController.startUpdater()
                    }
                    updaterController.updater.updateCheckInterval = TimeInterval(appStorage.integer(forKey: Constants.Update.checkInterval))
                    updaterController.updater.clearFeedURLFromUserDefaults()
                },
                lastUpdateCheckDate: {
                    updaterController.updater.publisher(for: \.lastUpdateCheckDate)
                        .compactMap { $0 }
                        .eraseToAnyPublisher()
                },
                checkForUpdates: { sender in
                    updaterController.checkForUpdates(sender)
                }
            )
        }
    }
}
