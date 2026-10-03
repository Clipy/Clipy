//
//  SnippetSyncFilePresenter.swift
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

/// Watches the shared sync file for coordinated changes made by other processes via
/// `NSFileCoordinator`'s presenter mechanism. A cloud download finishing does not always
/// produce a presenter callback, so the service also retries unreadable files independently.
final class SnippetSyncFilePresenter: NSObject, NSFilePresenter {
    let presentedItemURL: URL?
    let presentedItemOperationQueue = OperationQueue()
    private let onChange: () -> Void

    init(url: URL, onChange: @escaping () -> Void) {
        self.presentedItemURL = url
        self.onChange = onChange
        presentedItemOperationQueue.maxConcurrentOperationCount = 1
        super.init()
        NSFileCoordinator.addFilePresenter(self)
    }

    func stop() {
        NSFileCoordinator.removeFilePresenter(self)
    }

    func presentedItemDidChange() {
        onChange()
    }

    func accommodatePresentedItemDeletion(completionHandler: @escaping (Error?) -> Void) {
        completionHandler(nil)
    }
}
