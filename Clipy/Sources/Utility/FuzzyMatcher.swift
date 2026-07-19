//
//  FuzzyMatcher.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation

/// Lightweight fzf-style fuzzy matcher.
///
/// Performs a case-insensitive subsequence match of `query` against a candidate
/// string and returns a score plus the matched character positions so callers
/// can highlight them. Higher scores are better matches; `nil` means no match.
enum FuzzyMatcher {
    struct Match: Equatable {
        let score: Int
        let positions: [Int]
    }

    /// - Parameters:
    ///   - query: Lowercased query characters.
    ///   - text: Lowercased candidate characters.
    static func match(query: [Character], in text: [Character]) -> Match? {
        guard !query.isEmpty else { return Match(score: 0, positions: []) }
        guard query.count <= text.count else { return nil }

        var positions = [Int]()
        positions.reserveCapacity(query.count)
        var score = 0
        var queryIndex = 0
        var previousMatch = -2

        for (index, character) in text.enumerated() {
            guard queryIndex < query.count else { break }
            guard character == query[queryIndex] else { continue }

            var bonus = 1
            // Reward consecutive matches (e.g. typing "cli" matching "clipy").
            if index == previousMatch + 1 {
                bonus += 8
            }
            // Reward matches at the start of a word / after a separator.
            if index == 0 || isBoundary(text[index - 1]) {
                bonus += 10
            }
            score += bonus
            positions.append(index)
            previousMatch = index
            queryIndex += 1
        }

        guard queryIndex == query.count, let first = positions.first else { return nil }
        // Prefer matches that start earlier in the string.
        score -= first / 4
        return Match(score: score, positions: positions)
    }

    private static func isBoundary(_ character: Character) -> Bool {
        return character == " " || character == "_" || character == "-" ||
            character == "/" || character == "." || character == ":" ||
            character == "\t" || character == "\n"
    }
}
