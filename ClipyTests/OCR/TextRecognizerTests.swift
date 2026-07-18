//
//  TextRecognizerTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/10.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies
import DependenciesTestSupport
import SQLiteData
import Testing
@testable import Clipy

@MainActor
@Suite(
    .dependencies {
        try $0.bootstrapDatabase()
    }
)
struct TextRecognizerTests {
    let repository: PasteboardHistoryRepository
    let textRecognizer: TextRecognizer

    init() {
        let repository = PasteboardHistoryRepository()
        self.repository = repository
        self.textRecognizer = withDependencies {
            $0.pasteboardHistoryRepository = repository
        } operation: {
            TextRecognizer()
        }
    }

    @Test(.timeLimit(.minutes(1)))
    func recognizeTextIfNeededStoresRecognizedText() async throws {
        let content = try #require(PasteboardContent(image: renderedTextImage(text: "CLIPY 12345")))
        let id = PasteboardHistory.ID(rawValue: content.hash)
        repository.save(id: id, content: content, updateAt: 1)

        textRecognizer.recognizeTextIfNeeded(id: id)

        try await waitUntil { self.repository.fetchHistory(id: id)?.ocrText != nil }
        #expect(repository.fetchHistory(id: id)?.ocrText?.contains("12345") == true)
    }

    @Test(.timeLimit(.minutes(1)))
    func recognizeTextIfNeededStoresEmptyTextForImagesWithoutText() async throws {
        let content = try #require(
            PasteboardContent(image: NSImage.create(with: .blue, size: NSSize(width: 64, height: 64)))
        )
        let id = PasteboardHistory.ID(rawValue: content.hash)
        repository.save(id: id, content: content, updateAt: 1)

        textRecognizer.recognizeTextIfNeeded(id: id)

        try await waitUntil { self.repository.fetchHistory(id: id)?.ocrText != nil }
        #expect(repository.fetchHistory(id: id)?.ocrText == "")
    }

    @Test(.timeLimit(.minutes(1)))
    func recognizeTextIfNeededSkipsAlreadyRecognizedHistories() async throws {
        let recognizedContent = try #require(PasteboardContent(image: renderedTextImage(text: "CLIPY 12345")))
        let recognizedID = PasteboardHistory.ID(rawValue: recognizedContent.hash)
        repository.save(id: recognizedID, content: recognizedContent, updateAt: 1)
        repository.updateOCRText(id: recognizedID, ocrText: "Already Recognized")

        let pendingContent = try #require(PasteboardContent(image: renderedTextImage(text: "PENDING 67890")))
        let pendingID = PasteboardHistory.ID(rawValue: pendingContent.hash)
        repository.save(id: pendingID, content: pendingContent, updateAt: 2)

        textRecognizer.recognizeTextIfNeeded(id: recognizedID)
        textRecognizer.recognizeTextIfNeeded(id: pendingID)

        try await waitUntil { self.repository.fetchHistory(id: pendingID)?.ocrText != nil }
        #expect(repository.fetchHistory(id: recognizedID)?.ocrText == "Already Recognized")
    }

    @Test(.timeLimit(.minutes(1)))
    func recognizeTextIfNeededLeavesHistoriesWithoutImagesUnrecognized() async throws {
        let textContent = try #require(
            PasteboardContent(assets: [PasteboardContent.Asset(type: .string, data: Data("Hello".utf8))])
        )
        let textID = PasteboardHistory.ID(rawValue: textContent.hash)
        repository.save(id: textID, content: textContent, updateAt: 1)

        let imageContent = try #require(
            PasteboardContent(image: NSImage.create(with: .blue, size: NSSize(width: 64, height: 64)))
        )
        let imageID = PasteboardHistory.ID(rawValue: imageContent.hash)
        repository.save(id: imageID, content: imageContent, updateAt: 2)

        textRecognizer.recognizeTextIfNeeded(id: textID)
        textRecognizer.recognizeTextIfNeeded(id: imageID)

        try await waitUntil { self.repository.fetchHistory(id: imageID)?.ocrText != nil }
        let textHistory = try #require(repository.fetchHistory(id: textID))
        #expect(textHistory.ocrText == nil)
    }
}

private extension TextRecognizerTests {
    func renderedTextImage(text: String) -> NSImage {
        let size = NSSize(width: 480, height: 120)
        let image = NSImage(size: size)
        image.lockFocus()
        NSColor.white.setFill()
        NSRect(origin: .zero, size: size).fill()
        let attributes: [NSAttributedString.Key: Any] = [
            .font: NSFont.systemFont(ofSize: 48, weight: .bold),
            .foregroundColor: NSColor.black
        ]
        (text as NSString).draw(at: NSPoint(x: 24, y: 32), withAttributes: attributes)
        image.unlockFocus()
        return image
    }
}
