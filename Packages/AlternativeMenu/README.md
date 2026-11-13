# AlternativeMenu

A Swift package for macOS popups with the familiar appearance and interaction
of a native menu. Build menus with actions, submenus, section headers, and
separators, and add your own SwiftUI header.

## Why AlternativeMenu?

Embedding a text field in `NSMenu` can provide search, but we encountered a
limitation: users could not switch IMEs or keyboard input sources while the
menu was open. AlternativeMenu uses a custom popup so users can switch input
sources and continue typing without closing the menu.

## Requirements

- macOS 14 or later
- Xcode 26 or later

## Basic usage

Create an `AlternativeMenuSession` on the main actor and retain it while the
menu is in use. Build the items and call `present(menu:)` to open the menu at
the pointer. Actions run on the main actor after the menu closes.

```swift
import AlternativeMenu

@MainActor
final class ClipboardMenu {
    private let session = AlternativeMenuSession()

    func show(canPaste: Bool) {
        session.present(
            menu: .init(
                items: [
                    .sectionHeader("Clipboard"),
                    .action("Copy", keyEquivalent: "c") { _ in print("Copy") },
                    .action("Paste", isEnabled: canPaste, keyEquivalent: "v") { _ in
                        print("Paste")
                    },
                    .separator,
                    .submenu("Format", menu: .init(items: [
                        .action("Plain Text") { _ in print("Plain Text") },
                        .action("Rich Text") { _ in print("Rich Text") }
                    ]))
                ]
            )
        )
    }
}
```

Items can also include an `image`, `toolTip`, or `attributedTitle`. Action shortcuts use
Command by default; specify `keyEquivalentModifierMask` for other modifiers.

Pass `at:` to choose a screen position or `highlightsFirstItem: true` to select
the first enabled item when opening. For a menu opened on mouse-down, pass
the triggering event as `event:` to support dragging into an item to select it.

Use `isVisible` to check whether the menu is open and `dismiss()` to close it.

## Custom headers and updates

Provide a SwiftUI view when creating the session. `headerHeight` includes any
padding supplied by your view.

```swift
import AlternativeMenu
import SwiftUI

@MainActor
func makeMenuWithHeader() -> AlternativeMenuSession {
    AlternativeMenuSession(headerHeight: 40) {
        Text("Choose a format")
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(12)
    }
}
```

For search or other dynamic content, your app builds the new items and calls
`updateItems(_:)` while the menu is open.

```swift
session.updateItems([
    .action("Plain Text") { _ in print("Plain Text") },
    .action("Rich Text", isEnabled: false) { _ in }
])
```

Use SwiftUI's focus management for controls in the header. Focused controls
receive key events by default; use `shouldHandleMenuKey` when selected events,
such as arrow keys, should navigate the menu instead.

## Compatibility

AlternativeMenu implements a subset of `NSMenu` behavior. Some native appearance
details use private AppKit APIs and may vary across macOS releases.
