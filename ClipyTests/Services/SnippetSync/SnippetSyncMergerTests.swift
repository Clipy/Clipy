//
//  SnippetSyncMergerTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/09/28.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation
import Testing
@testable import Clipy

struct SnippetSyncMergerTests {
    private let folderID = SnippetFolder.ID(rawValue: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
    private let otherFolderID = SnippetFolder.ID(rawValue: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
    private let snippetID = Snippet.ID(rawValue: UUID(uuidString: "00000000-0000-0000-0000-000000000010")!)

    @Test
    func firstEnableMergesLocalAndRemoteWithoutDeletingEitherSide() throws {
        let localFolder = folder(id: folderID, title: "Local Folder", updatedAt: 100)
        let localSnippet = snippet(id: snippetID, folderID: folderID, title: "Local Snippet", updatedAt: 100)

        let remoteFolder = folderRecord(id: otherFolderID.rawValue, title: "Remote Folder", updatedAt: 50)

        let plan = SnippetSyncMerger.plan(
            localFolders: [localFolder],
            localSnippets: [localSnippet],
            previousLocal: SnippetSyncBaseline(folders: nil, snippets: nil),
            remote: .success(SnippetSyncFile(folders: [remoteFolder], snippets: [], deletedFolders: [], deletedSnippets: [])),
            now: 1_000
        )

        // The remote-only folder is added locally, and the local-only folder/snippet are kept
        // (and end up written to the file); nothing is deleted on a first enable.
        #expect(plan.folderUpserts.map(\.id) == [otherFolderID])
        #expect(plan.snippetUpserts.isEmpty)
        #expect(plan.folderDeletions.isEmpty)
        #expect(plan.snippetDeletions.isEmpty)

        let file = try #require(plan.fileToWrite)
        #expect(Set(file.folders.map(\.id)) == [folderID.rawValue, otherFolderID.rawValue])
        #expect(file.snippets.map(\.id) == [snippetID.rawValue])
        #expect(file.deletedFolders.isEmpty)
        #expect(file.deletedSnippets.isEmpty)
    }

    @Test
    func mergeOrdersRecordsDeterministicallyRegardlessOfInputOrder() throws {
        let first = folder(id: folderID, title: "First", updatedAt: 100)
        let second = folder(id: otherFolderID, title: "Second", updatedAt: 100)
        let forward = SnippetSyncMerger.plan(
            localFolders: [first, second],
            localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: nil, snippets: nil),
            remote: .notFound,
            now: 1_000
        )
        let backward = SnippetSyncMerger.plan(
            localFolders: [second, first],
            localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: nil, snippets: nil),
            remote: .notFound,
            now: 1_000
        )

        #expect(forward.fileToWrite == backward.fileToWrite)
        #expect(try #require(forward.fileToWrite).folders.map(\.id) == [folderID.rawValue, otherFolderID.rawValue])
    }

    @Test
    func notFoundFileIsTreatedAsEmptyAndLocalStateIsWrittenOut() throws {
        let localFolder = folder(id: folderID, title: "Folder", updatedAt: 100)

        let plan = SnippetSyncMerger.plan(
            localFolders: [localFolder],
            localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: nil, snippets: nil),
            remote: .notFound,
            now: 1_000
        )

        #expect(plan.folderUpserts.isEmpty)
        #expect(plan.folderDeletions.isEmpty)
        #expect(try #require(plan.fileToWrite).folders == [folderRecord(id: folderID.rawValue, title: "Folder", updatedAt: 100)])
    }

    @Test
    func equalTimestampEditsConvergeAcrossTwoDevices() throws {
        let deviceA = folder(id: folderID, title: "Alpha", updatedAt: 100)
        let deviceB = folder(id: folderID, title: "Beta", updatedAt: 100)
        let fileFromA = SnippetSyncFile(folders: [SnippetSyncFolderRecord(deviceA)], snippets: [], deletedFolders: [], deletedSnippets: [])
        let fileFromB = SnippetSyncFile(folders: [SnippetSyncFolderRecord(deviceB)], snippets: [], deletedFolders: [], deletedSnippets: [])

        let mergeOnA = SnippetSyncMerger.plan(
            localFolders: [deviceA], localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: [deviceA], snippets: []),
            remote: .success(fileFromB), now: 1_000
        )
        let mergeOnB = SnippetSyncMerger.plan(
            localFolders: [deviceB], localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: [deviceB], snippets: []),
            remote: .success(fileFromA), now: 1_000
        )

        #expect(mergeOnA.fileToWrite == mergeOnB.fileToWrite)
        #expect(try #require(mergeOnA.fileToWrite).folders.map(\.title) == ["Beta"])
        #expect(try #require(mergeOnB.fileToWrite).folders.map(\.title) == ["Beta"])
        #expect(mergeOnA.folderUpserts.map(\.title) == ["Beta"])
        #expect(mergeOnB.folderUpserts.isEmpty)

        let snippetA = snippet(id: snippetID, folderID: folderID, title: "Alpha", updatedAt: 100)
        let snippetB = snippet(id: snippetID, folderID: folderID, title: "Beta", updatedAt: 100)
        let snippetMergeOnA = SnippetSyncMerger.plan(
            localFolders: [folder(id: folderID, title: "Folder", updatedAt: 100)], localSnippets: [snippetA],
            previousLocal: SnippetSyncBaseline(folders: nil, snippets: [snippetA]),
            remote: .success(SnippetSyncFile(
                folders: [folderRecord(id: folderID.rawValue, title: "Folder", updatedAt: 100)],
                snippets: [SnippetSyncSnippetRecord(snippetB)], deletedFolders: [], deletedSnippets: []
            )), now: 1_000
        )
        let snippetMergeOnB = SnippetSyncMerger.plan(
            localFolders: [folder(id: folderID, title: "Folder", updatedAt: 100)], localSnippets: [snippetB],
            previousLocal: SnippetSyncBaseline(folders: nil, snippets: [snippetB]),
            remote: .success(SnippetSyncFile(
                folders: [folderRecord(id: folderID.rawValue, title: "Folder", updatedAt: 100)],
                snippets: [SnippetSyncSnippetRecord(snippetA)], deletedFolders: [], deletedSnippets: []
            )), now: 1_000
        )

        #expect(snippetMergeOnA.fileToWrite == snippetMergeOnB.fileToWrite)
        #expect(try #require(snippetMergeOnA.fileToWrite).snippets.map(\.title) == ["Beta"])
        #expect(try #require(snippetMergeOnB.fileToWrite).snippets.map(\.title) == ["Beta"])
        #expect(snippetMergeOnA.snippetUpserts.map(\.title) == ["Beta"])
        #expect(snippetMergeOnB.snippetUpserts.isEmpty)
    }

    @Test
    func equalTimestampUnicodeVariantsUseSerializedByteOrder() throws {
        let firstTitle = "a\u{301}\u{327}"
        let secondTitle = "a\u{327}\u{301}"
        let first = folderRecord(id: folderID.rawValue, title: firstTitle, updatedAt: 100)
        let second = folderRecord(id: folderID.rawValue, title: secondTitle, updatedAt: 100)
        let expected = try #require([first, second].max {
            serialized($0).lexicographicallyPrecedes(serialized($1))
        })

        let firstSide = SnippetSyncMerger.plan(
            localFolders: [SnippetFolder(first)], localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: [SnippetFolder(first)], snippets: []),
            remote: .success(SnippetSyncFile(folders: [second], snippets: [], deletedFolders: [], deletedSnippets: [])),
            now: 1_000
        )
        let secondSide = SnippetSyncMerger.plan(
            localFolders: [SnippetFolder(second)], localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: [SnippetFolder(second)], snippets: []),
            remote: .success(SnippetSyncFile(folders: [first], snippets: [], deletedFolders: [], deletedSnippets: [])),
            now: 1_000
        )

        #expect(firstTitle == secondTitle)
        let firstFile = try #require(firstSide.fileToWrite)
        let secondFile = try #require(secondSide.fileToWrite)
        #expect(serialized(firstFile) == serialized(secondFile))
        #expect(serialized(try #require(firstFile.folders.first)) == serialized(expected))
    }

    @Test
    func duplicateIDsInDecodedFileAndLocalCollectionsMergeSafely() throws {
        let json = """
        {
          "folders": [
            { "id": "00000000-0000-0000-0000-000000000001", "title": "Remote old", "index": 0, "isEnabled": true, "updatedAt": 500 },
            { "id": "00000000-0000-0000-0000-000000000001", "title": "Remote new", "index": 0, "isEnabled": true, "updatedAt": 700 }
          ],
          "snippets": [
            { "id": "00000000-0000-0000-0000-000000000010", "folderID": "00000000-0000-0000-0000-000000000001", "title": "Remote snippet old", "content": "old", "index": 0, "isEnabled": true, "updatedAt": 500 },
            { "id": "00000000-0000-0000-0000-000000000010", "folderID": "00000000-0000-0000-0000-000000000001", "title": "Remote snippet new", "content": "new", "index": 0, "isEnabled": true, "updatedAt": 700 }
          ],
          "deletedFolders": [
            { "id": "00000000-0000-0000-0000-000000000001", "deletedAt": 600 },
            { "id": "00000000-0000-0000-0000-000000000001", "deletedAt": 700 }
          ],
          "deletedSnippets": [
            { "id": "00000000-0000-0000-0000-000000000010", "deletedAt": 600 },
            { "id": "00000000-0000-0000-0000-000000000010", "deletedAt": 700 }
          ]
        }
        """
        let duplicateFile = try JSONDecoder().decode(SnippetSyncFile.self, from: Data(json.utf8))
        let localOld = folder(id: folderID, title: "Local old", updatedAt: 400)
        let localNew = folder(id: folderID, title: "Local new", updatedAt: 450)
        let localSnippetOld = snippet(id: snippetID, folderID: folderID, title: "Local snippet old", updatedAt: 400)
        let localSnippetNew = snippet(id: snippetID, folderID: folderID, title: "Local snippet new", updatedAt: 450)

        let plan = SnippetSyncMerger.plan(
            localFolders: [localOld, localNew], localSnippets: [localSnippetOld, localSnippetNew],
            previousLocal: SnippetSyncBaseline(folders: [localOld, localNew], snippets: [localSnippetOld, localSnippetNew]),
            remote: .success(duplicateFile), now: 1_000
        )

        #expect(plan.folderDeletions.isEmpty)
        #expect(try #require(plan.fileToWrite).folders.map(\.title) == ["Remote new"])
        #expect(try #require(plan.fileToWrite).snippets.map(\.title) == ["Remote snippet new"])
        #expect(try #require(plan.fileToWrite).deletedFolders.isEmpty)
        #expect(try #require(plan.fileToWrite).deletedSnippets.isEmpty)
    }

    @Test
    func equalTimestampLiveRecordWinsOverTombstone() throws {
        let localFolder = folder(id: folderID, title: "Live", updatedAt: 100)
        let tombstone = SnippetSyncTombstone(id: folderID.rawValue, deletedAt: 100)
        let plan = SnippetSyncMerger.plan(
            localFolders: [localFolder], localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: [localFolder], snippets: []),
            remote: .success(SnippetSyncFile(folders: [], snippets: [], deletedFolders: [tombstone], deletedSnippets: [])),
            now: 1_000
        )

        #expect(plan.folderDeletions.isEmpty)
        #expect(try #require(plan.fileToWrite).folders == [SnippetSyncFolderRecord(localFolder)])
        #expect(try #require(plan.fileToWrite).deletedFolders.isEmpty)
    }

    @Test
    func conflictingEditsAreResolvedByNewestUpdatedAt() throws {
        let localFolder = folder(id: folderID, title: "Local Title", updatedAt: 100)
        let remoteFolder = folderRecord(id: folderID.rawValue, title: "Remote Title", updatedAt: 200)

        let plan = SnippetSyncMerger.plan(
            localFolders: [localFolder],
            localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: [localFolder], snippets: []),
            remote: .success(SnippetSyncFile(folders: [remoteFolder], snippets: [], deletedFolders: [], deletedSnippets: [])),
            now: 1_000
        )

        // The remote edit is newer, so it wins and gets applied locally.
        #expect(plan.folderUpserts.map(\.title) == ["Remote Title"])
        #expect(try #require(plan.fileToWrite).folders.map(\.title) == ["Remote Title"])
    }

    @Test
    func localEditNewerThanRemoteWinsWithoutRewritingLocalDatabase() throws {
        let localFolder = folder(id: folderID, title: "Local Title", updatedAt: 200)
        let remoteFolder = folderRecord(id: folderID.rawValue, title: "Remote Title", updatedAt: 100)

        let plan = SnippetSyncMerger.plan(
            localFolders: [localFolder],
            localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: [localFolder], snippets: []),
            remote: .success(SnippetSyncFile(folders: [remoteFolder], snippets: [], deletedFolders: [], deletedSnippets: [])),
            now: 1_000
        )

        // The local edit already reflects the winner, so there's nothing new to apply locally,
        // but the file still needs to be updated to carry the newer local title.
        #expect(plan.folderUpserts.isEmpty)
        #expect(try #require(plan.fileToWrite).folders.map(\.title) == ["Local Title"])
    }

    @Test
    func remoteTombstoneNewerThanLocalEditDeletesLocalFolderAndItsSnippets() throws {
        let localFolder = folder(id: folderID, title: "Folder", updatedAt: 100)
        let localSnippet = snippet(id: snippetID, folderID: folderID, title: "Snippet", updatedAt: 150)
        let tombstone = SnippetSyncTombstone(id: folderID.rawValue, deletedAt: 200)

        let plan = SnippetSyncMerger.plan(
            localFolders: [localFolder],
            localSnippets: [localSnippet],
            previousLocal: SnippetSyncBaseline(folders: [localFolder], snippets: [localSnippet]),
            remote: .success(SnippetSyncFile(folders: [], snippets: [], deletedFolders: [tombstone], deletedSnippets: [])),
            now: 1_000
        )

        #expect(plan.folderDeletions == [folderID])
        // The snippet is cascade-deleted with its folder even though the tombstone never
        // mentions the snippet directly and the snippet's own edit is newer than the tombstone.
        #expect(plan.snippetDeletions == [snippetID])

        let file = try #require(plan.fileToWrite)
        #expect(file.folders.isEmpty)
        #expect(file.snippets.isEmpty)
        #expect(file.deletedFolders == [tombstone])
    }

    @Test
    func remoteTombstoneOlderThanLocalEditLosesToTheEdit() throws {
        let localFolder = folder(id: folderID, title: "Folder", updatedAt: 200)
        let tombstone = SnippetSyncTombstone(id: folderID.rawValue, deletedAt: 100)

        let plan = SnippetSyncMerger.plan(
            localFolders: [localFolder],
            localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: [localFolder], snippets: []),
            remote: .success(SnippetSyncFile(folders: [], snippets: [], deletedFolders: [tombstone], deletedSnippets: [])),
            now: 1_000
        )

        #expect(plan.folderDeletions.isEmpty)
        #expect(try #require(plan.fileToWrite).folders.map(\.id) == [folderID.rawValue])
        #expect(try #require(plan.fileToWrite).deletedFolders.isEmpty)
    }

    @Test
    func localDeletionSinceLastSyncProducesATombstoneForOtherDevices() throws {
        let remoteFolder = folderRecord(id: folderID.rawValue, title: "Folder", updatedAt: 100)

        let plan = SnippetSyncMerger.plan(
            localFolders: [],
            localSnippets: [],
            previousLocal: SnippetSyncBaseline(folders: [folder(id: folderID, title: "Folder", updatedAt: 100)], snippets: []),
            remote: .success(SnippetSyncFile(folders: [remoteFolder], snippets: [], deletedFolders: [], deletedSnippets: [])),
            now: 1_000
        )

        // Already gone locally, so there's nothing left to delete locally...
        #expect(plan.folderDeletions.isEmpty)
        // ...but the file needs a tombstone so other Macs delete their copy too.
        let file = try #require(plan.fileToWrite)
        #expect(file.folders.isEmpty)
        #expect(file.deletedFolders == [SnippetSyncTombstone(id: folderID.rawValue, deletedAt: 1_000)])
    }

    @Test
    func unreadableFileProducesNoChangesAndLeavesLocalDataUntouched() {
        let localFolder = folder(id: folderID, title: "Folder", updatedAt: 100)
        let localSnippet = snippet(id: snippetID, folderID: folderID, title: "Snippet", updatedAt: 100)

        let plan = SnippetSyncMerger.plan(
            localFolders: [localFolder],
            localSnippets: [localSnippet],
            previousLocal: SnippetSyncBaseline(folders: [localFolder], snippets: [localSnippet]),
            remote: .unreadable,
            now: 1_000
        )

        #expect(plan == SnippetSyncPlan())
        #expect(!plan.hasLocalChanges)
        #expect(plan.fileToWrite == nil)
    }
}

private extension SnippetSyncMergerTests {
    func folder(id: SnippetFolder.ID, title: String, updatedAt: Int) -> SnippetFolder {
        SnippetFolder(id: id, title: title, index: 0, isEnabled: true, updatedAt: updatedAt)
    }

    func snippet(id: Snippet.ID, folderID: SnippetFolder.ID, title: String, updatedAt: Int) -> Snippet {
        Snippet(id: id, folderID: folderID, title: title, content: "content", index: 0, isEnabled: true, updatedAt: updatedAt)
    }

    func folderRecord(id: UUID, title: String, updatedAt: Int) -> SnippetSyncFolderRecord {
        SnippetSyncFolderRecord(id: id, title: title, index: 0, isEnabled: true, updatedAt: updatedAt)
    }

    func serialized<T: Encodable>(_ value: T) -> Data {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        return try! encoder.encode(value)
    }
}
