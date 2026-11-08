//
//  AlternativeMenu.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

public struct AlternativeMenu {
    public let items: [AlternativeMenuItem]

    public init(items: [AlternativeMenuItem]) {
        self.items = items
    }
}

public struct AlternativeMenuItem {
    enum Title {
        case plain(String)
        case attributed(NSAttributedString)

        var string: String {
            switch self {
            case .plain(let title): title
            case .attributed(let title): title.string
            }
        }
    }

    enum Kind {
        case action(isEnabled: Bool, perform: @MainActor (NSEvent.ModifierFlags) -> Void)
        case submenu(AlternativeMenu, isEnabled: Bool)
        case sectionHeader
        case separator
    }

    /// The plain text of the title, including titles supplied as attributed strings.
    public var title: String { titleContent.string }
    public let image: NSImage?
    public let toolTip: String?
    public let keyEquivalent: String
    public let keyEquivalentModifierMask: NSEvent.ModifierFlags
    /// The item's own enabled state. Headers and separators always return false.
    public var isEnabled: Bool {
        switch kind {
        case .action(let isEnabled, _), .submenu(_, let isEnabled): isEnabled
        case .sectionHeader, .separator: false
        }
    }
    public var submenu: AlternativeMenu? {
        if case .submenu(let menu, _) = kind { return menu }
        return nil
    }

    let titleContent: Title
    let kind: Kind

    private init(
        _ title: Title,
        keyEquivalent: String = "",
        keyEquivalentModifierMask: NSEvent.ModifierFlags = .command,
        image: NSImage? = nil,
        toolTip: String? = nil,
        kind: Kind
    ) {
        self.titleContent = switch title {
        case .plain:
            title
        case .attributed(let value):
            .attributed(NSAttributedString(attributedString: value))
        }
        self.keyEquivalent = keyEquivalent
        self.keyEquivalentModifierMask = keyEquivalentModifierMask
        self.image = image
        self.toolTip = toolTip
        self.kind = kind
    }
}

public extension AlternativeMenuItem {
    static var separator: AlternativeMenuItem { AlternativeMenuItem(.plain(""), kind: .separator) }

    static func action(
        _ title: String,
        isEnabled: Bool = true,
        keyEquivalent: String = "",
        keyEquivalentModifierMask: NSEvent.ModifierFlags = .command,
        image: NSImage? = nil,
        toolTip: String? = nil,
        perform: @escaping @MainActor (NSEvent.ModifierFlags) -> Void
    ) -> AlternativeMenuItem {
        AlternativeMenuItem(
            .plain(title),
            keyEquivalent: keyEquivalent,
            keyEquivalentModifierMask: keyEquivalentModifierMask,
            image: image,
            toolTip: toolTip,
            kind: .action(isEnabled: isEnabled, perform: perform)
        )
    }

    static func action(
        attributedTitle: NSAttributedString,
        isEnabled: Bool = true,
        keyEquivalent: String = "",
        keyEquivalentModifierMask: NSEvent.ModifierFlags = .command,
        image: NSImage? = nil,
        toolTip: String? = nil,
        perform: @escaping @MainActor (NSEvent.ModifierFlags) -> Void
    ) -> AlternativeMenuItem {
        AlternativeMenuItem(
            .attributed(attributedTitle),
            keyEquivalent: keyEquivalent,
            keyEquivalentModifierMask: keyEquivalentModifierMask,
            image: image,
            toolTip: toolTip,
            kind: .action(isEnabled: isEnabled, perform: perform)
        )
    }

    static func submenu(
        _ title: String,
        menu: AlternativeMenu,
        isEnabled: Bool = true,
        image: NSImage? = nil,
        toolTip: String? = nil
    ) -> AlternativeMenuItem {
        AlternativeMenuItem(.plain(title), image: image, toolTip: toolTip, kind: .submenu(menu, isEnabled: isEnabled))
    }

    static func submenu(
        attributedTitle: NSAttributedString,
        menu: AlternativeMenu,
        isEnabled: Bool = true,
        image: NSImage? = nil,
        toolTip: String? = nil
    ) -> AlternativeMenuItem {
        AlternativeMenuItem(.attributed(attributedTitle), image: image, toolTip: toolTip, kind: .submenu(menu, isEnabled: isEnabled))
    }

    static func sectionHeader(
        _ title: String,
        image: NSImage? = nil,
        toolTip: String? = nil
    ) -> AlternativeMenuItem {
        AlternativeMenuItem(.plain(title), image: image, toolTip: toolTip, kind: .sectionHeader)
    }

    static func sectionHeader(
        attributedTitle: NSAttributedString,
        image: NSImage? = nil,
        toolTip: String? = nil
    ) -> AlternativeMenuItem {
        AlternativeMenuItem(.attributed(attributedTitle), image: image, toolTip: toolTip, kind: .sectionHeader)
    }
}
