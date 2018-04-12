//
//  ScreenShotObserver.swift
//  Screeen
//
//  Created by 古林俊佑 on 2016/07/12.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

@objc public protocol ScreenShotObserverDelegate: class {
    @objc optional func screenShotObserver(_ observer: ScreenShotObserver, addedItem item: NSMetadataItem)
    @objc optional func screenShotObserver(_ observer: ScreenShotObserver, updatedItem item: NSMetadataItem)
    @objc optional func screenShotObserver(_ observer: ScreenShotObserver, removedItem item: NSMetadataItem)
}

public final class ScreenShotObserver: NSObject {

    // MARK: - Properties
    public weak var delegate: ScreenShotObserverDelegate?
    public var isEnabled = true

    fileprivate let query = NSMetadataQuery()

    // MARK: - Initialize
    override public init() {
        super.init()
        // Observe update notification
        NotificationCenter.default
            .addObserver(self,
                         selector: #selector(ScreenShotObserver.updateQuery(_:)),
                         name: NSNotification.Name.NSMetadataQueryDidUpdate,
                         object: query)
        // Query setting
        query.delegate = self
        query.predicate = NSPredicate(format: "kMDItemIsScreenCapture = 1")
        query.start()
    }

    deinit {
        NotificationCenter.default.removeObserver(self)
        query.stop()
        query.delegate = nil
    }

    @objc func updateQuery(_ notification: Notification) {
        if !isEnabled { return }

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
