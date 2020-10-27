//
//  ScreenShotObserver.swift
//
//  Screeen
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Copyright © 2015-2020 Clipy Project.
//

import Foundation

@objc
public protocol ScreenShotObserverDelegate: AnyObject {
    @objc optional func screenShotObserver(_ observer: ScreenShotObserver, addedItem item: NSMetadataItem)
    @objc optional func screenShotObserver(_ observer: ScreenShotObserver, updatedItem item: NSMetadataItem)
    @objc optional func screenShotObserver(_ observer: ScreenShotObserver, removedItem item: NSMetadataItem)
}

public final class ScreenShotObserver: NSObject {

    // MARK: - Properties
    public weak var delegate: ScreenShotObserverDelegate?
    public var isEnabled = true

    private static let desktopDirectoryPath: String? = NSSearchPathForDirectoriesInDomains(.desktopDirectory, .userDomainMask, true).first
    private let query = NSMetadataQuery()
    private let notificationCenter: NotificationCenter

    // MARK: - Initialize
    public override convenience init() {
        self.init(searchDirectoryPaths: [ScreenShotObserver.desktopDirectoryPath].compactMap { $0 }, notificationCenter: .default)
    }

    public convenience init(searchDirectoryPaths: [String]) {
        self.init(searchDirectoryPaths: searchDirectoryPaths, notificationCenter: .default)
    }

    init(searchDirectoryPaths: [String], notificationCenter: NotificationCenter) {
        self.notificationCenter = notificationCenter
        super.init()
        // Observe update notification
        notificationCenter.addObserver(self,
                                       selector: #selector(ScreenShotObserver.updateQuery(_:)),
                                       name: NSNotification.Name.NSMetadataQueryDidUpdate,
                                       object: query)
        // Query setting
        query.delegate = self
        if !searchDirectoryPaths.isEmpty {
            query.searchScopes = searchDirectoryPaths
        }
        query.predicate = NSPredicate(format: "kMDItemIsScreenCapture = 1")
    }

    deinit {
        notificationCenter.removeObserver(self)
        query.stop()
        query.delegate = nil
    }

    // MARK: - Start
    public func start() {
        query.start()
    }

    public func stop() {
        query.stop()
    }

    // MARK: - Notification
    @objc func updateQuery(_ notification: Notification) {
        guard isEnabled else { return }

        if let items = notification.userInfo?[kMDQueryUpdateAddedItems as String] as? [NSMetadataItem] {
            items.forEach { delegate?.screenShotObserver?(self, addedItem: $0) }
        } else if let items = notification.userInfo?[kMDQueryUpdateChangedItems as String] as? [NSMetadataItem] {
            items.forEach { delegate?.screenShotObserver?(self, updatedItem: $0) }
        } else if let items = notification.userInfo?[kMDQueryUpdateRemovedItems as String] as? [NSMetadataItem] {
            items.forEach { delegate?.screenShotObserver?(self, removedItem: $0) }
        }
    }
}

extension ScreenShotObserver: NSMetadataQueryDelegate {}
