import AppKit
import Dependencies
import GRDB
import SQLiteData

/// Reads the existing trigram index without loading clipboard assets or thumbnails.
final class HistoryMenuSearchStore {
    @Dependency(\.defaultDatabase) private var database

    enum Sort: String, CaseIterable, Sendable {
        case bestMatch, original, newest, oldest, alphabetical, type

        var title: String {
            switch self {
            case .bestMatch: return String(localized: "Best Match")
            case .original: return String(localized: "Follow History Preferences")
            case .newest: return String(localized: "Newest First")
            case .oldest: return String(localized: "Oldest First")
            case .alphabetical: return String(localized: "Alphabetical")
            case .type: return String(localized: "Content Type")
            }
        }
    }

    private static func escaped(_ text: String) -> String {
        text.replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "%", with: "\\%")
            .replacingOccurrences(of: "_", with: "\\_")
    }

    struct Entry {
        let id: PasteboardHistory.ID
        let label: String
    }

    func search(query: String, sortsByCreatedAt: Bool, limit: Int, sort: Sort = .bestMatch) throws -> [Entry] {
        let words = query.split(whereSeparator: \.isWhitespace).map(String.init)
        guard !words.isEmpty else { return [] }
        var predicates = [String]()
        var arguments = [String]()
        for word in words {
            let pattern = "%" + Self.escaped(word) + "%"
            // ESCAPE disables SQLite's trigram LIKE optimization, so use it only
            // when the user's literal query includes wildcard/escape characters.
            let escape = word.contains(where: { "%_\\".contains($0) }) ? " ESCAPE '\\'" : ""
            predicates.append("h.id IN (SELECT id FROM pasteboardHistorySearches WHERE title LIKE ?\(escape) OR ocrText LIKE ?\(escape))")
            arguments.append(contentsOf: [pattern, pattern])
        }
        let order: String
        switch sort {
        case .bestMatch:
            let titleWords = words.map { _ in "h.title LIKE ? ESCAPE '\\'" }.joined(separator: " AND ")
            order = """
                CASE WHEN h.title = ? COLLATE NOCASE THEN 0
                WHEN h.title LIKE ? ESCAPE '\\' THEN 1
                WHEN h.title LIKE ? ESCAPE '\\' THEN 2
                WHEN \(titleWords) THEN 3 ELSE 4 END,
                length(h.title), h.createdAt DESC
                """
            let phrase = words.joined(separator: " ")
            arguments.append(contentsOf: [phrase, Self.escaped(phrase) + "%", "%" + Self.escaped(phrase) + "%"])
            arguments.append(contentsOf: words.map { "%" + Self.escaped($0) + "%" })
        case .original: order = (sortsByCreatedAt ? "h.createdAt" : "h.updateAt") + " DESC"
        case .newest: order = "h.createdAt DESC"
        case .oldest: order = "h.createdAt ASC"
        case .alphabetical: order = "h.title COLLATE NOCASE, h.createdAt DESC"
        case .type: order = "h.pasteboardTypes, h.createdAt DESC"
        }
        return try database.read { db in
            try Row.fetchAll(db, sql: """
                SELECT h.id, h.title FROM pasteboardHistories h
                WHERE \(predicates.joined(separator: " AND "))
                ORDER BY \(order), h.id LIMIT \(max(1, min(limit, 100)))
                """, arguments: StatementArguments(arguments)).map { row in
                    Entry(id: .init(rawValue: row["id"]), label: (row["title"] as String).isEmpty ? "(Image)" : row["title"])
                }
        }
    }
}
