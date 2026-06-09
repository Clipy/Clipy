//
//  PasteboardAvailableTypeTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/31.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Testing
@testable import Clipy

@MainActor
@Suite
struct PasteboardAvailableTypeTests {
    @Test
    func availableTypesPreservesPasteboardTypeOrder() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.pdf, .string, .fileURL, .rtf],
            storeAvailableTypes: [.string, .rtf, .pdf, .filenames],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.pdf, .string, .fileURL, .rtf])
    }

    @Test
    func availableTypesFiltersDisabledStoreTypes() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.pdf, .string, .tiff, .rtf],
            storeAvailableTypes: [.string, .tiff],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.string, .tiff])
    }

    @Test
    func availableTypesFiltersDeprecatedTypesForDisabledStoreTypes() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.deprecatedString, .deprecatedPDF, .deprecatedURL],
            storeAvailableTypes: [.url],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.deprecatedURL])
    }

    @Test
    func availableTypesPrefersModernStringWhenBothStringTypesAreAvailable() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.deprecatedString, .string],
            storeAvailableTypes: [.string],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.string])
    }

    @Test
    func availableTypesSkipsTIFFWhenPNGIsAvailable() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.tiff, .deprecatedTIFF, .png],
            storeAvailableTypes: [.tiff],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.png])
    }

    @Test
    func availableTypesUsesTIFFWhenOnlyTIFFTypesAreAvailable() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.tiff, .deprecatedTIFF],
            storeAvailableTypes: [.tiff],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.tiff])
    }

    @Test
    func availableTypesSkipsDeprecatedTypesWhenModernTypesAreAvailable() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [
                .deprecatedString,
                .deprecatedFilenames,
                .fileURL,
                .string,
                .deprecatedPDF,
                .pdf
            ],
            storeAvailableTypes: [.filenames, .string, .pdf],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.fileURL, .string, .pdf])
    }

    @Test
    func availableTypesUsesDeprecatedTypesWhenModernTypesAreUnavailable() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.deprecatedURL, .deprecatedFilenames, .deprecatedString, .deprecatedPDF, .deprecatedTIFF],
            storeAvailableTypes: [.filenames, .string, .pdf, .url, .tiff],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.deprecatedURL, .deprecatedFilenames, .deprecatedString, .deprecatedPDF, .deprecatedTIFF])
    }

    @Test
    func availableTypesIgnoresTransientPasteboards() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.string, .transient],
            storeAvailableTypes: [.string],
            ignoresConcealedType: false
        )
        #expect(availableTypes.isEmpty)
    }

    @Test
    func availableTypesKeepsConcealedPasteboardByDefault() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.string, .concealed],
            storeAvailableTypes: [.string],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.string, .concealed])
    }

    @Test
    func availableTypesPlacesConcealedPasteboardAfterStoredTypes() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.concealed, .string],
            storeAvailableTypes: [.string],
            ignoresConcealedType: false
        )
        #expect(availableTypes == [.string, .concealed])
    }

    @Test
    func availableTypesIgnoresConcealedPasteboardWhenEnabled() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.string, .concealed],
            storeAvailableTypes: [.string],
            ignoresConcealedType: true
        )
        #expect(availableTypes.isEmpty)
    }

    @Test
    func availableTypesDoesNotStoreOnlyConcealedPasteboards() {
        let availableTypes = PasteboardAvailableType.availableTypes(
            from: [.concealed],
            storeAvailableTypes: [.string],
            ignoresConcealedType: false
        )
        #expect(availableTypes.isEmpty)
    }
}
