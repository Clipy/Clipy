//
//  AlternativeMenuExtensions.swift
//  Clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import AlternativeMenu
import AppKit

extension AlternativeMenu {
    @MainActor
    init(
        nativeItems: [NSMenuItem],
        perform: @escaping @MainActor @Sendable (NSMenuItem, NSEvent.ModifierFlags) -> Void
    ) {
        self.init(
            items: nativeItems.filter { !$0.isHidden }
                .map { item in
                    guard !item.isSeparatorItem else {
                        return .separator
                    }
                    if item.isSectionHeader {
                        if let title = item.attributedTitle {
                            return .sectionHeader(attributedTitle: title, image: item.image, toolTip: item.toolTip)
                        }
                        return .sectionHeader(item.title, image: item.image, toolTip: item.toolTip)
                    }
                    if let submenu = item.submenu {
                        let menu = AlternativeMenu(nativeItems: submenu.items, perform: perform)
                        if let title = item.attributedTitle {
                            return .submenu(
                                attributedTitle: title,
                                menu: menu,
                                isEnabled: item.isEnabled,
                                image: item.image,
                                toolTip: item.toolTip
                            )
                        }
                        return .submenu(
                            item.title,
                            menu: menu,
                            isEnabled: item.isEnabled,
                            image: item.image,
                            toolTip: item.toolTip
                        )
                    }
                    let action: @MainActor @Sendable (NSEvent.ModifierFlags) -> Void = { modifiers in
                        perform(item, modifiers)
                    }
                    if let title = item.attributedTitle {
                        return .action(
                            attributedTitle: title,
                            isEnabled: item.isEnabled && item.action != nil,
                            keyEquivalent: item.keyEquivalent,
                            keyEquivalentModifierMask: item.keyEquivalentModifierMask,
                            image: item.image,
                            toolTip: item.toolTip,
                            perform: action
                        )
                    }
                    return .action(
                        item.title,
                        isEnabled: item.isEnabled && item.action != nil,
                        keyEquivalent: item.keyEquivalent,
                        keyEquivalentModifierMask: item.keyEquivalentModifierMask,
                        image: item.image,
                        toolTip: item.toolTip,
                        perform: action
                    )
                }
        )
    }
}
