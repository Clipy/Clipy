//
//  SnippetsEditorWindowController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/05/18.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Cocoa
import SwiftUI

final class SnippetsEditorWindowController: NSWindowController {

    // MARK: - Initialize
    init(frameAutosaveName: NSWindow.FrameAutosaveName = "SnippetEditorWindow") {
        let minimumContentSize = SnippetsEditorView.minimumContentSize
        let window = NSWindow(
            contentRect: NSRect(origin: .zero, size: minimumContentSize),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        let hostingController = NSHostingController(rootView: SnippetsEditorView())
        // Update the minimum constraint without resizing to GeometryReader's ideal size.
        hostingController.sizingOptions = [.minSize]
        window.title = String(localized: .SnippetEditor.snippetsEditor)
        window.isReleasedWhenClosed = false
        window.contentMinSize = minimumContentSize
        window.contentViewController = hostingController
        window.setContentSize(minimumContentSize)
        window.center()
        window.setFrameUsingName(frameAutosaveName)
        window.setFrameAutosaveName(frameAutosaveName)
        // Repair frames saved while the hosting view allowed the window to collapse.
        let restoredContentSize = window.contentRect(forFrameRect: window.frame).size
        if restoredContentSize.width < minimumContentSize.width || restoredContentSize.height < minimumContentSize.height {
            window.setContentSize(NSSize(
                width: max(minimumContentSize.width, restoredContentSize.width),
                height: max(minimumContentSize.height, restoredContentSize.height)
            ))
        }
        super.init(window: window)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: - Window Life Cycle
    override func showWindow(_ sender: Any?) {
        super.showWindow(sender)
        window?.toolbar?.displayMode = .iconAndLabel
    }
}
