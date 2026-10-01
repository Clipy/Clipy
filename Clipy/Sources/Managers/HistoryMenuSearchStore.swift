import Dependencies
import GRDB
import SQLiteData

/// Reads the existing trigram index without loading clipboard assets or thumbnails.
final class HistoryMenuSearchStore {
    @Dependency(\.defaultDatabase) private var database

    struct Entry {
        let id: PasteboardHistory.ID
        let label: String
    }

    func search(query: String, sortsByCreatedAt: Bool, limit: Int) throws -> [Entry] {
        let words = query.split(whereSeparator: \.isWhitespace).map(String.init)
        guard !words.isEmpty else { return [] }
        var predicates = [String]()
        var arguments = [String]()
        for word in words {
            let pattern = "%" + word.replacingOccurrences(of: "\\", with: "\\\\")
                .replacingOccurrences(of: "%", with: "\\%")
                .replacingOccurrences(of: "_", with: "\\_") + "%"
            // ESCAPE disables SQLite's trigram LIKE optimization, so use it only
            // when the user's literal query includes wildcard/escape characters.
            let escape = word.contains(where: { "%_\\".contains($0) }) ? " ESCAPE '\\'" : ""
            predicates.append("h.id IN (SELECT id FROM pasteboardHistorySearches WHERE title LIKE ?\(escape) OR ocrText LIKE ?\(escape))")
            arguments.append(contentsOf: [pattern, pattern])
        }
        let order = sortsByCreatedAt ? "h.createdAt" : "h.updateAt"
        return try database.read { db in
            try Row.fetchAll(db, sql: """
                SELECT h.id, h.title FROM pasteboardHistories h
                WHERE \(predicates.joined(separator: " AND "))
                ORDER BY \(order) DESC, h.id LIMIT \(max(1, min(limit, 100)))
                """, arguments: StatementArguments(arguments)).map { row in
                    Entry(id: .init(rawValue: row["id"]), label: (row["title"] as String).isEmpty ? "(Image)" : row["title"])
                }
        }
    }
}
