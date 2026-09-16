//
//  PasteboardHistoryExtensionsTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies
import DependenciesTestSupport
import Sharing
import Testing
@testable import Clipy

@Suite(.dependencies)
struct PasteboardHistoryExtensionsTests {
    private let history = Self.pasteboardHistory(title: "Clipboard title", pasteboardTypes: [.string])

    @Test(arguments: [
        NSPasteboard.PasteboardType.png,
        .tiff,
        .deprecatedTIFF
    ])
    func typedTitleIncludesImagePrefix(pasteboardType: NSPasteboard.PasteboardType) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 20

        let history = Self.pasteboardHistory(title: "  Screenshot\nMetadata  ", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])

        #expect(history.typedTitle == "(Image) Screenshot")
        #expect(emptyTitleHistory.typedTitle == "(Image)")

        $maximumMenuItemTitleLength.withLock { $0 = 5 }

        #expect(history.typedTitle == "(Image) Sc...")
        #expect(emptyTitleHistory.typedTitle == "(Image)")
    }

    @Test(arguments: [
        NSPasteboard.PasteboardType.pdf,
        .deprecatedPDF
    ])
    func typedTitleIncludesPDFPrefix(pasteboardType: NSPasteboard.PasteboardType) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 20

        let history = Self.pasteboardHistory(title: "Document", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])

        #expect(history.typedTitle == "(PDF) Document")
        #expect(emptyTitleHistory.typedTitle == "(PDF)")

        $maximumMenuItemTitleLength.withLock { $0 = 5 }

        #expect(history.typedTitle == "(PDF) Do...")
        #expect(emptyTitleHistory.typedTitle == "(PDF)")
    }

    @Test(arguments: [
        NSPasteboard.PasteboardType.fileURL,
        .deprecatedFilenames
    ])
    func typedTitleIncludesFilesPrefix(pasteboardType: NSPasteboard.PasteboardType) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 20

        let history = Self.pasteboardHistory(title: "Archive", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])

        #expect(history.typedTitle == "(Files) Archive")
        #expect(emptyTitleHistory.typedTitle == "(Files)")

        $maximumMenuItemTitleLength.withLock { $0 = 5 }

        #expect(history.typedTitle == "(Files) Ar...")
        #expect(emptyTitleHistory.typedTitle == "(Files)")
    }

    @Test(arguments: [
        NSPasteboard.PasteboardType.string,
        .deprecatedString,
        .rtf,
        .deprecatedRTF
    ])
    func typedTitleOmitsPrefix(pasteboardType: NSPasteboard.PasteboardType) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 20

        let history = Self.pasteboardHistory(title: "Plain text", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])

        #expect(history.typedTitle == "Plain text")
        #expect(emptyTitleHistory.typedTitle == "")

        $maximumMenuItemTitleLength.withLock { $0 = 5 }

        #expect(history.typedTitle == "Pl...")
        #expect(emptyTitleHistory.typedTitle == "")
    }

    @Test
    func toolTipShowsFullTitleWhenEnabled() {
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems = true
        @Shared(.maximumToolTipLength) var maximumToolTipLength = 20

        #expect(history.toolTip == "Clipboard title")
    }

    @Test
    func toolTipShortensWhenEnabled() {
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems = true
        @Shared(.maximumToolTipLength) var maximumToolTipLength = 9

        #expect(history.toolTip == "Clipboard")
    }

    @Test
    func toolTipIsNilWhenDisabled() {
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems = false

        #expect(history.toolTip == nil)
    }
}

private extension PasteboardHistoryExtensionsTests {
    static func pasteboardHistory(
        title: String,
        pasteboardTypes: [NSPasteboard.PasteboardType]
    ) -> PasteboardHistory {
        PasteboardHistory(
            id: PasteboardHistory.ID(rawValue: UUID().uuidString),
            title: title,
            ocrText: nil,
            pasteboardTypes: pasteboardTypes,
            createdAt: 1,
            updateAt: 1,
            deviceID: nil
        )
    }
}
