//
//  AlternativeMenuSession+Header.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

public extension AlternativeMenuSession {
    /// Creates a session with a custom SwiftUI header above the scrolling menu rows.
    ///
    /// Creating the session does not show a menu. The header remains outside the
    /// scrolling content and is retained when items are replaced or the menu reopens.
    /// A menu with no items can remain open to display just this header.
    ///
    /// The caller owns header state and keyboard focus, including initial focus;
    /// the session does not select a first responder for the header. Focus routing
    /// recognizes AppKit controls and text views, and marked-text detection uses
    /// the active text view. The key policy can opt focused controls into menu
    /// navigation and shortcuts without replacing their normal text input handling.
    ///
    /// - Parameters:
    ///   - headerHeight: Fixed header height, in points, including any padding added
    ///     by the caller. Must be finite and greater than zero. Header content is
    ///     clipped to this height.
    ///   - minimumWidth: Preferred minimum width of the whole root menu, in points.
    ///     Must be finite and nonnegative. Defaults to 240; zero sizes the menu to
    ///     its rows. Screen bounds and the menu's width limit can override this
    ///     minimum. Submenus size themselves independently.
    ///   - minimumSymbolColumnWidth: Minimum width, in points, reserved for SF Symbol
    ///     columns in the root and its submenus. Must be finite and nonnegative.
    ///     Zero sizes columns to their contents; a larger value can keep icons and
    ///     titles aligned across item updates.
    ///   - passesScrollWheelToHeader: When `true`, wheel events over the header
    ///     continue through the normal responder chain. When `false`, the session
    ///     handles them as scrolling of the menu rows. Defaults to `true`.
    ///   - shouldHandleMenuKey: An optional policy called on the main actor for
    ///     key events routed to the menu. Returning `true` allows menu navigation
    ///     and shortcut matching even when a header control has focus; an event
    ///     the menu does not handle still continues through the responder chain.
    ///     Returning `false` leaves the event to the responder chain. With `nil`,
    ///     a focused header receives events instead of the root menu;
    ///     an open submenu may still receive navigation, but shortcuts remain
    ///     disabled while a header control has focus. Typing selection also stays
    ///     disabled during header focus, even when the policy returns `true`.
    ///     Marked text bypasses this policy so the input method handles composition.
    ///     Otherwise, Escape and Command-period dismiss before consulting it.
    ///   - onDismiss: A callback retained by the session and called synchronously
    ///     on the main actor after the presentation's windows are hidden and its
    ///     event monitors removed. It runs for activation, cancellation, replacement,
    ///     and a presentation that cannot be shown. For activation, it runs before
    ///     the item's action. Calling `dismiss()` while inactive does not call it again.
    ///   - header: Builds the header content once during initialization. Its state
    ///     and focus remain under the caller's control.
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
