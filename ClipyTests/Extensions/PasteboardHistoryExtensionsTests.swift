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
    func menuTitleUsesImageFallbackWhenEmpty(pasteboardType: NSPasteboard.PasteboardType) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 20

        let history = Self.pasteboardHistory(title: "  Screenshot\nMetadata  ", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])
        let whitespaceTitleHistory = Self.pasteboardHistory(title: " \t\n ", pasteboardTypes: [pasteboardType])

        #expect(history.menuTitle == "Screenshot")
        #expect(emptyTitleHistory.menuTitle == "(Image)")
        #expect(whitespaceTitleHistory.menuTitle == "(Image)")

        $maximumMenuItemTitleLength.withLock { $0 = 5 }

        #expect(history.menuTitle == "Sc...")
        #expect(emptyTitleHistory.menuTitle == "(Image)")
        #expect(whitespaceTitleHistory.menuTitle == "(Image)")
    }

    @Test(arguments: [
        NSPasteboard.PasteboardType.pdf,
        .deprecatedPDF
    ])
    func menuTitleUsesPDFFallbackWhenEmpty(pasteboardType: NSPasteboard.PasteboardType) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 20

        let history = Self.pasteboardHistory(title: "Document", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])
        let whitespaceTitleHistory = Self.pasteboardHistory(title: " \t\n ", pasteboardTypes: [pasteboardType])

        #expect(history.menuTitle == "Document")
        #expect(emptyTitleHistory.menuTitle == "(PDF)")
        #expect(whitespaceTitleHistory.menuTitle == "(PDF)")

        $maximumMenuItemTitleLength.withLock { $0 = 5 }

        #expect(history.menuTitle == "Do...")
        #expect(emptyTitleHistory.menuTitle == "(PDF)")
        #expect(whitespaceTitleHistory.menuTitle == "(PDF)")
    }

    @Test(arguments: [
        NSPasteboard.PasteboardType.fileURL,
        .deprecatedFilenames
    ])
    func menuTitleUsesFilesFallbackWhenEmpty(pasteboardType: NSPasteboard.PasteboardType) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 20

        let history = Self.pasteboardHistory(title: "Archive", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])
        let whitespaceTitleHistory = Self.pasteboardHistory(title: " \t\n ", pasteboardTypes: [pasteboardType])

        #expect(history.menuTitle == "Archive")
        #expect(emptyTitleHistory.menuTitle == "(Files)")
        #expect(whitespaceTitleHistory.menuTitle == "(Files)")

        $maximumMenuItemTitleLength.withLock { $0 = 5 }

        #expect(history.menuTitle == "Ar...")
        #expect(emptyTitleHistory.menuTitle == "(Files)")
        #expect(whitespaceTitleHistory.menuTitle == "(Files)")
    }

    @Test(arguments: [
        NSPasteboard.PasteboardType.string,
        .deprecatedString,
        .rtf,
        .deprecatedRTF,
        .rtfd,
        .deprecatedRTFD,
        .URL,
        .deprecatedURL,
        .html
    ])
    func menuTitleOmitsPrefix(pasteboardType: NSPasteboard.PasteboardType) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 20

        let history = Self.pasteboardHistory(title: "Plain text", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])
        let whitespaceTitleHistory = Self.pasteboardHistory(title: " \t\n ", pasteboardTypes: [pasteboardType])

        #expect(history.menuTitle == "Plain text")
        #expect(emptyTitleHistory.menuTitle == "")
        #expect(whitespaceTitleHistory.menuTitle == "")

        $maximumMenuItemTitleLength.withLock { $0 = 5 }

        #expect(history.menuTitle == "Pl...")
        #expect(emptyTitleHistory.menuTitle == "")
        #expect(whitespaceTitleHistory.menuTitle == "")
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
