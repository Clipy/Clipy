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
        let history = Self.pasteboardHistory(title: "  Screenshot\nMetadata  ", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])

        withDependencies {
            $0.defaultAppStorage.set(20, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect(history.typedTitle == "(Image) Screenshot")
            #expect(emptyTitleHistory.typedTitle == "(Image)")
        }
        withDependencies {
            $0.defaultAppStorage.set(5, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect(history.typedTitle == "(Image) Sc...")
            #expect(emptyTitleHistory.typedTitle == "(Image)")
        }
    }

    @Test(arguments: [
        NSPasteboard.PasteboardType.pdf,
        .deprecatedPDF
    ])
    func typedTitleIncludesPDFPrefix(pasteboardType: NSPasteboard.PasteboardType) {
        let history = Self.pasteboardHistory(title: "Document", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])

        withDependencies {
            $0.defaultAppStorage.set(20, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect(history.typedTitle == "(PDF) Document")
            #expect(emptyTitleHistory.typedTitle == "(PDF)")
        }
        withDependencies {
            $0.defaultAppStorage.set(5, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect(history.typedTitle == "(PDF) Do...")
            #expect(emptyTitleHistory.typedTitle == "(PDF)")
        }
    }

    @Test(arguments: [
        NSPasteboard.PasteboardType.fileURL,
        .deprecatedFilenames
    ])
    func typedTitleIncludesFilesPrefix(pasteboardType: NSPasteboard.PasteboardType) {
        let history = Self.pasteboardHistory(title: "Archive", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])

        withDependencies {
            $0.defaultAppStorage.set(20, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect(history.typedTitle == "(Files) Archive")
            #expect(emptyTitleHistory.typedTitle == "(Files)")
        }
        withDependencies {
            $0.defaultAppStorage.set(5, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect(history.typedTitle == "(Files) Ar...")
            #expect(emptyTitleHistory.typedTitle == "(Files)")
        }
    }

    @Test(arguments: [
        NSPasteboard.PasteboardType.string,
        .deprecatedString,
        .rtf,
        .deprecatedRTF
    ])
    func typedTitleOmitsPrefix(pasteboardType: NSPasteboard.PasteboardType) {
        let history = Self.pasteboardHistory(title: "Plain text", pasteboardTypes: [pasteboardType])
        let emptyTitleHistory = Self.pasteboardHistory(title: "", pasteboardTypes: [pasteboardType])

        withDependencies {
            $0.defaultAppStorage.set(20, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect(history.typedTitle == "Plain text")
            #expect(emptyTitleHistory.typedTitle == "")
        }
        withDependencies {
            $0.defaultAppStorage.set(5, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect(history.typedTitle == "Pl...")
            #expect(emptyTitleHistory.typedTitle == "")
        }
    }

    @Test
    func toolTipShowsFullTitleWhenEnabled() {
        withDependencies {
            $0.defaultAppStorage.set(true, forKey: Constants.UserDefaults.showToolTipOnMenuItem)
            $0.defaultAppStorage.set(20, forKey: Constants.UserDefaults.maxLengthOfToolTip)
        } operation: {
            #expect(history.toolTip == "Clipboard title")
        }
    }

    @Test
    func toolTipShortensWhenEnabled() {
        withDependencies {
            $0.defaultAppStorage.set(true, forKey: Constants.UserDefaults.showToolTipOnMenuItem)
            $0.defaultAppStorage.set(9, forKey: Constants.UserDefaults.maxLengthOfToolTip)
        } operation: {
            #expect(history.toolTip == "Clipboard")
        }
    }

    @Test
    func toolTipIsNilWhenDisabled() {
        withDependencies {
            $0.defaultAppStorage.set(false, forKey: Constants.UserDefaults.showToolTipOnMenuItem)
            $0.defaultAppStorage.set(9, forKey: Constants.UserDefaults.maxLengthOfToolTip)
        } operation: {
            #expect(history.toolTip == nil)
        }
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
