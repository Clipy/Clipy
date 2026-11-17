//
//  AlternativeMenuBuilder.swift
//  Clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import AlternativeMenu
import AppKit
import Dependencies
import Sharing

/// Builds custom menus using the shared native item presentation.
@MainActor
final class AlternativeMenuBuilder {
    private let nativeBuilder: NSMenuBuilder
    private let menuFont = NSFont.menuFont(ofSize: 0)

    @Dependency(\.pasteService)
    private var pasteService
    @Shared(.showsSnippetsAboveHistory)
    private var showsSnippetsAboveHistory

    nonisolated init(nativeBuilder: NSMenuBuilder) {
        self.nativeBuilder = nativeBuilder
    }

    private(set) lazy var minimumSymbolColumnWidth: CGFloat = searchSymbolColumnWidth()

    func makeSearchItems(
        histories: [PasteboardHistoryDetail],
        snippets: [SnippetSearchResult],
        limit: Int
    ) -> [AlternativeMenuItem] {
        guard !histories.isEmpty || !snippets.isEmpty else {
            return [
                .sectionHeader(
                    attributedTitle: NSAttributedString(
                        string: String(localized: "No Search Results"),
                        attributes: [.font: menuFont, .foregroundColor: NSColor.secondaryLabelColor]
                    )
                )
            ]
        }

        let snippetsFirst = showsSnippetsAboveHistory
        let limit = max(0, limit)
        let historyLimit = snippetsFirst ? max(0, limit - snippets.count) : limit
        let snippetLimit = snippetsFirst ? limit : max(0, limit - histories.count)
        let historyItems = histories.prefix(historyLimit).enumerated().map { index, detail in
            nativeBuilder.makeClipMenuItem(detail, index: index, listNumber: nativeBuilder.firstItemNumber + index, font: menuFont)
        }
        let snippetItems = snippets.prefix(snippetLimit).enumerated().map { index, match in
            nativeBuilder.makeSnippetMenuItem(
                match.snippet, folderTitle: match.folderTitle, listNumber: nativeBuilder.firstItemNumber + index, font: menuFont
            )
        }
        let history = (String(localized: "History"), historyItems)
        let snippet = (String(localized: "Snippet"), snippetItems)
        let sections = snippetsFirst ? [snippet, history] : [history, snippet]
        var items: [AlternativeMenuItem] = []
        for (title, rows) in sections where !rows.isEmpty {
            if !items.isEmpty { items.append(.separator) }
            items.append(.sectionHeader(title))
            items.append(contentsOf: makeItems(from: rows))
        }
        return items
    }

    func makeItems(from items: [NSMenuItem]) -> [AlternativeMenuItem] {
        AlternativeMenu(nativeItems: items) { [weak self] item, modifiers in
            self?.performItem(item, modifiers: modifiers)
        }.items
    }
}

private extension AlternativeMenuBuilder {
    func searchSymbolColumnWidth() -> CGFloat {
        let configuration = NSImage.SymbolConfiguration(pointSize: menuFont.pointSize, weight: .semibold, scale: .small)
        return Set(PasteboardAvailableType.allCases.map(\.symbolName) + ["text.quote"])
            .compactMap { nativeBuilder.symbolImage($0, font: menuFont) }
            .map { image in
                if #available(macOS 26, *) {
                    return (image.withSymbolConfiguration(configuration) ?? image).size.width
                }
                return image.size.width
            }
            .max() ?? 0
    }

    func performItem(_ item: NSMenuItem, modifiers: NSEvent.ModifierFlags) {
        switch item.action {
        case #selector(AppDelegate.selectClipMenuItem(_:)):
            guard let id = item.representedObject as? PasteboardHistory.ID else { return }
            pasteService.paste(id: id, modifierFlags: modifiers)
        case #selector(AppDelegate.selectSnippetMenuItem(_:)):
            guard let id = item.representedObject as? Snippet.ID else { return }
            pasteService.paste(id: id)
        case .some(let action):
            NSApp.sendAction(action, to: item.target, from: item)
        case .none:
            break
        }
    }
}
