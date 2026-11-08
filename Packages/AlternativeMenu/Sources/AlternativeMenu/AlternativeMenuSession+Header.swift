//
//  AlternativeMenuSession+Header.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

public extension AlternativeMenuSession {
    /// The height includes padding supplied by the caller. The minimum width
    /// is for the whole menu and remains subject to its screen bounds.
    /// The symbol column minimum also applies to submenus and must be finite and nonnegative.
    /// The key policy can opt focused header controls into menu navigation and shortcuts.
    convenience init<Header: View>(
        headerHeight: CGFloat,
        minimumWidth: CGFloat = 240,
        minimumSymbolColumnWidth: CGFloat = 0,
        passesScrollWheelToHeader: Bool = true,
        shouldHandleMenuKey: ((NSEvent) -> Bool)? = nil,
        onDismiss: (() -> Void)? = nil,
        @ViewBuilder header: () -> Header
    ) {
        let header = AlternativeMenuHeader(height: headerHeight, minimumWidth: minimumWidth, content: header)
        self.init(
            root: AlternativeMenuPanel(header: header, minimumSymbolColumnWidth: minimumSymbolColumnWidth),
            passesScrollWheelToHeader: passesScrollWheelToHeader,
            shouldHandleMenuKey: shouldHandleMenuKey,
            onDismiss: onDismiss
        )
    }
}
