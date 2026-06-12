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

import SwiftUI

@main
struct ClipyApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    var body: some Scene {
        MenuBarExtra("", isInserted: .constant(false)) {
            EmptyView()
        }
    }
}
