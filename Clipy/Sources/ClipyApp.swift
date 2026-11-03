//
//  ClipyApp.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2026/06/11.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import SwiftUI

@main
struct ClipyApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    init() {
        prepareDependencies { values in
            try! values.bootstrapDatabase()
        }
    }

    var body: some Scene {
        MenuBarExtra(String(), isInserted: .constant(false)) {
            EmptyView()
        }
    }
}
