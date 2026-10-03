//
//  SnippetSyncMerger.swift
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

/// The outcome of reading the shared sync file, distinguishing "no file yet" (safe to treat as
/// empty and write to) from "a file exists but could not be read" (must never be treated as an
/// instruction to delete local data).
enum SnippetSyncRemoteReadResult {
    case notFound
    case success(SnippetSyncFile)
    case unreadable
}

/// The set of local database changes to apply and the file contents to write back, produced by
/// `SnippetSyncMerger.plan`. `fileToWrite` is `nil` when the remote file could not be read, in
/// which case no local changes are produced either.
struct SnippetSyncPlan: Equatable {
    var folderUpserts: [SnippetFolder] = []
    var folderDeletions: [SnippetFolder.ID] = []
    var snippetUpserts: [Snippet] = []
    var snippetDeletions: [Snippet.ID] = []
    var fileToWrite: SnippetSyncFile?

    var hasLocalChanges: Bool {
        !folderUpserts.isEmpty || !folderDeletions.isEmpty || !snippetUpserts.isEmpty || !snippetDeletions.isEmpty
    }
}

struct SnippetSyncBaseline {
    var folders: [SnippetFolder]?
    var snippets: [Snippet]?
}

enum SnippetSyncMerger {
    /// Merges the current local snippet state with the shared sync file.
    ///
    /// - Parameters:
    ///   - localFolders: The folders currently in the local database.
    ///   - localSnippets: The snippets currently in the local database.
    ///   - previousLocal: The folder and snippet records this device last wrote to the sync file.
    ///     A missing baseline means first enable, so absent records are never treated as deletions.
    ///   - remote: The result of reading the shared sync file.
    ///   - now: The current unix time in milliseconds, used to stamp newly discovered local deletions.
    static func plan(
        localFolders: [SnippetFolder],
        localSnippets: [Snippet],
        previousLocal: SnippetSyncBaseline,
        remote: SnippetSyncRemoteReadResult,
        now: Int
    ) -> SnippetSyncPlan {
        let remoteFile: SnippetSyncFile
        switch remote {
        case .unreadable:
            // The file exists but couldn't be read (I/O error, corrupt JSON, or an
            // iCloud placeholder still downloading). Treating it as empty would look like every
            // remote record was deleted, so skip this merge cycle entirely and keep local data untouched.
            return SnippetSyncPlan()
        case .notFound:
            remoteFile = .empty
        case let .success(file):
            remoteFile = file
        }

        let folderResult = mergeEntities(
            local: localFolders.map(SnippetSyncFolderRecord.init),
            previousLocal: previousLocal.folders.map { $0.map(SnippetSyncFolderRecord.init) },
            remoteRecords: remoteFile.folders,
            remoteTombstones: remoteFile.deletedFolders,
            now: now
        )

        let survivingFolderIDs = Set(folderResult.records.map(\.id))

        // A snippet whose folder no longer exists is implicitly deleted along with its folder
        // (mirroring the local database's ON DELETE CASCADE), regardless of what the remote file
        // or previous local state say about that snippet on its own.
        let remoteSnippetRecords = remoteFile.snippets.filter { survivingFolderIDs.contains($0.folderID) }
        let remoteSnippetTombstones = remoteFile.deletedSnippets
        let localSnippetRecords = localSnippets
            .filter { survivingFolderIDs.contains($0.folderID.rawValue) }
            .map(SnippetSyncSnippetRecord.init)
        let orphanedLocalSnippetIDs = localSnippets
            .filter { !survivingFolderIDs.contains($0.folderID.rawValue) }
            .map(\.id)

        let snippetResult = mergeEntities(
            local: localSnippetRecords,
            previousLocal: previousLocal.snippets.map { $0.map(SnippetSyncSnippetRecord.init) },
            remoteRecords: remoteSnippetRecords,
            remoteTombstones: remoteSnippetTombstones,
            now: now
        )

        let mergedFile = SnippetSyncFile(
            folders: folderResult.records,
            snippets: snippetResult.records,
            deletedFolders: folderResult.tombstones,
            deletedSnippets: snippetResult.tombstones
        )

        return SnippetSyncPlan(
            folderUpserts: folderResult.localUpserts.map(SnippetFolder.init),
            folderDeletions: folderResult.localDeletions.map(SnippetFolder.ID.init(rawValue:)),
            snippetUpserts: snippetResult.localUpserts.map(Snippet.init),
            snippetDeletions: (snippetResult.localDeletions + orphanedLocalSnippetIDs.map(\.rawValue))
                .map(Snippet.ID.init(rawValue:)),
            fileToWrite: mergedFile
        )
    }
}

// MARK: - Generic entity merge

private protocol SnippetSyncEntityRecord: Equatable, Encodable {
    var id: UUID { get }
    var updatedAt: Int { get }
    var canonicalContent: Data { get }
}

extension SnippetSyncFolderRecord: SnippetSyncEntityRecord {
    var canonicalContent: Data { canonicalData(self) }
}

extension SnippetSyncSnippetRecord: SnippetSyncEntityRecord {
    var canonicalContent: Data { canonicalData(self) }
}

private func canonicalData<Record: Encodable>(_ record: Record) -> Data {
    let encoder = JSONEncoder()
    encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
    return try! encoder.encode(record)
}

private struct EntityMergeResult<Record> {
    var records: [Record]
    var tombstones: [SnippetSyncTombstone]
    var localUpserts: [Record]
    var localDeletions: [UUID]
}

private struct MergeCandidate<Record> {
    let timestamp: Int
    let isDeleted: Bool
    let record: Record?
    let tombstone: SnippetSyncTombstone?
}

private func recordsByID<Record: SnippetSyncEntityRecord>(_ records: [Record]) -> [UUID: Record] {
    records.reduce(into: [:]) { result, record in
        guard let existing = result[record.id] else {
            result[record.id] = record
            return
        }
        if record.updatedAt > existing.updatedAt ||
            (record.updatedAt == existing.updatedAt &&
             existing.canonicalContent.lexicographicallyPrecedes(record.canonicalContent)) {
            result[record.id] = record
        }
    }
}

private func tombstonesByID(_ tombstones: [SnippetSyncTombstone]) -> [UUID: SnippetSyncTombstone] {
    tombstones.reduce(into: [:]) { result, tombstone in
        if tombstone.deletedAt > (result[tombstone.id]?.deletedAt ?? Int.min) {
            result[tombstone.id] = tombstone
        }
    }
}

/// Merges one entity type (folders, or snippets) by id: the version with the newest timestamp
/// wins, whether that version is a live record or a tombstone. Ties prefer the live record, so a
/// simultaneous edit and delete never destroys data by accident.
private func mergeEntities<Record: SnippetSyncEntityRecord>(
    local: [Record],
    previousLocal: [Record]?,
    remoteRecords: [Record],
    remoteTombstones: [SnippetSyncTombstone],
    now: Int
) -> EntityMergeResult<Record> {
    let localByID = recordsByID(local)
    let remoteByID = recordsByID(remoteRecords)
    let remoteTombstoneByID = tombstonesByID(remoteTombstones)

    // A local deletion is only detectable relative to a known prior baseline: an id this device
    // previously synced but no longer has locally. Without a baseline (first enable), nothing is
    // considered locally deleted, so first enable only ever adds records, never removes them.
    var localTombstoneByID = [UUID: SnippetSyncTombstone]()
    if let previousLocal {
        let previousIDs = Set(previousLocal.map(\.id))
        let currentIDs = Set(localByID.keys)
        for id in previousIDs.subtracting(currentIDs) {
            localTombstoneByID[id] = SnippetSyncTombstone(id: id, deletedAt: now)
        }
    }

    let allIDs = Set(localByID.keys)
        .union(remoteByID.keys)
        .union(remoteTombstoneByID.keys)
        .union(localTombstoneByID.keys)

    var records = [Record]()
    var tombstones = [SnippetSyncTombstone]()
    var localUpserts = [Record]()
    var localDeletions = [UUID]()

    // Stable ordering keeps independently merged files byte-identical across Macs. Without it,
    // differing Set iteration order can make file presenters rewrite each other's output forever.
    for id in allIDs.sorted(by: { $0.uuidString < $1.uuidString }) {
        var candidates = [MergeCandidate<Record>]()
        if let record = localByID[id] {
            candidates.append(MergeCandidate(timestamp: record.updatedAt, isDeleted: false, record: record, tombstone: nil))
        }
        if let record = remoteByID[id] {
            candidates.append(MergeCandidate(timestamp: record.updatedAt, isDeleted: false, record: record, tombstone: nil))
        }
        if let tombstone = remoteTombstoneByID[id] {
            candidates.append(MergeCandidate(timestamp: tombstone.deletedAt, isDeleted: true, record: nil, tombstone: tombstone))
        }
        if let tombstone = localTombstoneByID[id] {
            candidates.append(MergeCandidate(timestamp: tombstone.deletedAt, isDeleted: true, record: nil, tombstone: tombstone))
        }

        var winner = candidates[0]
        for candidate in candidates.dropFirst() {
            if candidate.timestamp > winner.timestamp {
                winner = candidate
            } else if candidate.timestamp == winner.timestamp {
                if winner.isDeleted, !candidate.isDeleted {
                    winner = candidate
                } else if !winner.isDeleted, !candidate.isDeleted,
                          let candidateRecord = candidate.record,
                          let winnerRecord = winner.record,
                          winnerRecord.canonicalContent.lexicographicallyPrecedes(candidateRecord.canonicalContent) {
                    winner = candidate
                }
            }
        }

        if winner.isDeleted, let tombstone = winner.tombstone {
            tombstones.append(tombstone)
            if localByID[id] != nil {
                localDeletions.append(id)
            }
        } else if let record = winner.record {
            records.append(record)
            if localByID[id] != record {
                localUpserts.append(record)
            }
        }
    }

    return EntityMergeResult(records: records, tombstones: tombstones, localUpserts: localUpserts, localDeletions: localDeletions)
}
