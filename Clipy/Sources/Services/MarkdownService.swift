//
//  MarkdownService.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation
import Cocoa
import Maaku

/// Service responsible for detecting and converting Markdown content.
///
/// This service provides functionality to:
/// - Detect if text contains Markdown formatting
/// - Convert Markdown to HTML with GitHub-like styling
/// - Convert Markdown to RTF for pasteboard operations
///
/// Uses Maaku library (libcmark_gfm) which supports GitHub Flavored Markdown
/// including tables, strikethrough, and autolinks.
final class MarkdownService {

    // MARK: - Singleton

    static let shared = MarkdownService()

    // MARK: - Private Properties

    /// Regex patterns used to detect common Markdown syntax
    private let markdownPatterns: [String] = [
        "^#{1,6}\\s+.+",           // Headers (# to ######)
        "\\*\\*[^*]+\\*\\*",       // Bold with **
        "__[^_]+__",               // Bold with __
        "```[\\s\\S]*?```",        // Fenced code blocks
        "`[^`]+`",                 // Inline code
        "\\[.+\\]\\(.+\\)",        // Links [text](url)
        "!\\[.*\\]\\(.+\\)",       // Images ![alt](url)
        "^[\\s]*[-*+]\\s+.+",      // Unordered lists
        "^[\\s]*\\d+\\.\\s+.+",    // Ordered lists
        "^>\\s+.+",                // Blockquotes
        "^[-*_]{3,}$",             // Horizontal rules
        "~~[^~]+~~",               // Strikethrough
        "\\|.+\\|"                 // Tables
    ]

    // MARK: - Public Methods

    /// Detects if the given text contains Markdown formatting.
    ///
    /// - Parameter text: The text to analyze.
    /// - Returns: `true` if the text contains Markdown syntax, `false` otherwise.
    func isMarkdown(_ text: String) -> Bool {
        guard !text.isEmpty else { return false }

        for pattern in markdownPatterns {
            if let regex = try? NSRegularExpression(pattern: pattern, options: [.anchorsMatchLines]) {
                let range = NSRange(text.startIndex..., in: text)
                if regex.firstMatch(in: text, options: [], range: range) != nil {
                    return true
                }
            }
        }
        return false
    }

    /// Converts Markdown text to styled HTML.
    ///
    /// Uses GitHub Flavored Markdown (GFM) parser which supports:
    /// - Tables
    /// - Strikethrough
    /// - Autolinks
    /// - Task lists
    ///
    /// - Parameter markdown: The Markdown text to convert.
    /// - Returns: HTML string with embedded CSS styling, or `nil` if conversion fails.
    func markdownToHTML(_ markdown: String) -> String? {
        do {
            let document = try CMDocument(text: markdown)
            let html = try document.renderHtml()
            return wrapInStyledHTML(html)
        } catch {
            return nil
        }
    }

    /// Converts Markdown text to an attributed string.
    ///
    /// - Parameter markdown: The Markdown text to convert.
    /// - Returns: An `NSAttributedString` with formatted content, or `nil` if conversion fails.
    func markdownToAttributedString(_ markdown: String) -> NSAttributedString? {
        guard let html = markdownToHTML(markdown) else { return nil }
        guard let data = html.data(using: .utf8) else { return nil }

        let options: [NSAttributedString.DocumentReadingOptionKey: Any] = [
            .documentType: NSAttributedString.DocumentType.html,
            .characterEncoding: String.Encoding.utf8.rawValue
        ]

        return try? NSAttributedString(data: data, options: options, documentAttributes: nil)
    }

    /// Converts Markdown text to RTF data suitable for pasteboard.
    ///
    /// - Parameter markdown: The Markdown text to convert.
    /// - Returns: RTF data that can be placed on the pasteboard, or `nil` if conversion fails.
    func markdownToRTFData(_ markdown: String) -> Data? {
        guard let attributedString = markdownToAttributedString(markdown) else { return nil }

        let range = NSRange(location: 0, length: attributedString.length)
        return try? attributedString.data(from: range, documentAttributes: [
            .documentType: NSAttributedString.DocumentType.rtf
        ])
    }

    // MARK: - Private Methods

    /// Wraps HTML content in a complete HTML document with GitHub-inspired styling.
    ///
    /// Styling features:
    /// - White background (#ffffff)
    /// - Dark gray text (#333333)
    /// - System font stack
    /// - Styled tables, code blocks, blockquotes
    ///
    /// - Parameter content: The HTML content to wrap.
    /// - Returns: A complete HTML document with embedded CSS.
    private func wrapInStyledHTML(_ content: String) -> String {
        return """
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <style>
                body {
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
                    font-size: 14px;
                    line-height: 1.6;
                    color: #333333;
                    background-color: #ffffff;
                    padding: 16px;
                    margin: 0;
                }
                h1, h2, h3, h4, h5, h6 {
                    color: #1a1a1a;
                    margin-top: 24px;
                    margin-bottom: 16px;
                    font-weight: 600;
                }
                h1 { font-size: 2em; border-bottom: 1px solid #eaecef; padding-bottom: 0.3em; }
                h2 { font-size: 1.5em; border-bottom: 1px solid #eaecef; padding-bottom: 0.3em; }
                h3 { font-size: 1.25em; }
                a { color: #0366d6; text-decoration: none; }
                code {
                    font-family: 'SF Mono', Monaco, 'Courier New', monospace;
                    background-color: rgba(27, 31, 35, 0.05);
                    padding: 0.2em 0.4em;
                    border-radius: 3px;
                    font-size: 85%;
                }
                pre {
                    background-color: #f6f8fa;
                    padding: 16px;
                    border-radius: 6px;
                    overflow-x: auto;
                }
                pre code { background-color: transparent; padding: 0; }
                blockquote {
                    border-left: 4px solid #dfe2e5;
                    padding: 0 16px;
                    margin: 16px 0;
                    color: #6a737d;
                }
                ul, ol { padding-left: 2em; margin: 16px 0; }
                table { border-collapse: collapse; width: 100%; margin: 16px 0; }
                table th, table td { border: 1px solid #dfe2e5; padding: 6px 13px; }
                table th { background-color: #f6f8fa; font-weight: 600; }
                table tr:nth-child(even) { background-color: #f6f8fa; }
                hr { border: none; border-top: 1px solid #eaecef; margin: 24px 0; }
                img { max-width: 100%; }
            </style>
        </head>
        <body>\(content)</body>
        </html>
        """
    }
}
