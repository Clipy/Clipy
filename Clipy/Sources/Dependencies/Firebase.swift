//
//  Firebase.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/06/25.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import DependenciesMacros
import Firebase

@DependencyClient
struct Firebase {
    var configure: () -> Void
    var logEvent: (_ event: Event) -> Void

    enum Event {
        case popUpMenu(MenuType)
        case popUpSnippetFolderMenu

        var name: String {
            switch self {
            case .popUpMenu:
                return "pop_up_menu"
            case .popUpSnippetFolderMenu:
                return "pop_up_snippet_folder_menu"
            }
        }
        var parameters: [String: Any]? {
            switch self {
            case .popUpMenu(let type):
                return ["type": type.rawValue]
            case .popUpSnippetFolderMenu:
                return nil
            }
        }
    }
}

extension DependencyValues {
    var firebase: Firebase {
        get { self[FirebaseKey.self] }
        set { self[FirebaseKey.self] = newValue }
    }

    private enum FirebaseKey: DependencyKey {
        static let liveValue: Firebase = .init(
            configure: {
                @Dependency(\.defaultAppStorage) var appStorage

                appStorage.register(defaults: ["NSApplicationCrashOnExceptions": true])
                guard
                    appStorage.bool(forKey: Constants.UserDefaults.collectCrashReport),
                    let path = Bundle.main.path(forResource: "GoogleService-Info", ofType: "plist"),
                    let options = FirebaseOptions(contentsOfFile: path),
                    FirebaseApp.app() == nil
                else { return }

                FirebaseApp.configure(options: options)
            },
            logEvent: { event in
                @Dependency(\.defaultAppStorage) var appStorage

                guard appStorage.bool(forKey: Constants.UserDefaults.collectCrashReport) else { return }

                Analytics.logEvent(event.name, parameters: event.parameters)
            }
        )

        static var previewValue: Firebase = Firebase(
            configure: {},
            logEvent: { _ in }
        )
        static var testValue: Firebase = Firebase(
            configure: {},
            logEvent: { _ in }
        )
    }
}
