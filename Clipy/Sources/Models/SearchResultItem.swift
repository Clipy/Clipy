//
//  SearchResultItem.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/25.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

/// A single row in the search panel, which mixes clipboard history and snippets.
///
/// Modelled as an enum rather than a protocol because pasting branches on the kind — history pastes
/// its stored pasteboard content, a snippet pastes its text — and the exhaustiveness check keeps that
/// branch honest as the cases grow.
enum SearchResultItem: Equatable, Identifiable {
    case history(PasteboardHistoryDetail)
    case snippet(SnippetSearchMatch)

    var id: String {
        switch self {
        case .history(let detail):
            return "history:\(detail.history.id.rawValue)"
        case .snippet(let match):
            return "snippet:\(match.snippet.id.rawValue.uuidString)"
        }
    }

    var title: String {
        switch self {
        case .history(let detail):
            return detail.history.typedTitle
        case .snippet(let match):
            return match.snippet.title.trimmedMenuTitle
        }
    }

    /// The folder name for snippets, and the copied date for history entries.
    var subtitle: String {
        switch self {
        case .history(let detail):
            return Self.subtitleFormatter.localizedString(
                for: Date(timeIntervalSince1970: TimeInterval(detail.history.updateAt)),
                relativeTo: Date()
            )
        case .snippet(let match):
            return match.folder.title
        }
    }

    var toolTip: String? {
        switch self {
        case .history(let detail):
            return detail.history.toolTip
        case .snippet(let match):
            return match.snippet.toolTip
        }
    }

    var thumbnailImage: NSImage? {
        switch self {
        case .history(let detail):
            return detail.thumbnailAsset.flatMap { NSImage(data: $0.data) }
        case .snippet:
            return nil
        }
    }

    private static let subtitleFormatter: RelativeDateTimeFormatter = {
        let formatter = RelativeDateTimeFormatter()
        formatter.unitsStyle = .abbreviated
        return formatter
    }()
}
