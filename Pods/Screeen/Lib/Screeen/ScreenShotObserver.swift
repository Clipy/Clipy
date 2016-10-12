//
//  ScreenShotObserver.swift
//  Screeen
//
//  Created by 古林俊佑 on 2016/07/12.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

@objc public protocol ScreenShotObserverDelegate: class {
    optional func screenShotObserver(observer: ScreenShotObserver, addedItem item: NSMetadataItem)
    optional func screenShotObserver(observer: ScreenShotObserver, updatedItem item: NSMetadataItem)
    optional func screenShotObserver(observer: ScreenShotObserver, removedItem item: NSMetadataItem)
}

public final class ScreenShotObserver: NSObject {

    // MARK: - Properties
    public weak var delegate: ScreenShotObserverDelegate?
    public var isEnabled = true

    private let query = NSMetadataQuery()

    // MARK: - Initialize
    override public init() {
        super.init()
        // Observe update notification
        NSNotificationCenter.defaultCenter()
            .addObserver(self,
                         selector: #selector(ScreenShotObserver.updateQuery(_:)),
                         name: NSMetadataQueryDidUpdateNotification,
                         object: query)
        // Query setting
        query.delegate = self
        query.predicate = NSPredicate(format: "kMDItemIsScreenCapture = 1")
        query.startQuery()
    }

    deinit {
        NSNotificationCenter.defaultCenter().removeObserver(self)
        query.stopQuery()
        query.delegate = nil
    }


    func updateQuery(notification: NSNotification) {
        if !isEnabled { return }

        if let items = notification.userInfo?[kMDQueryUpdateAddedItems] as? [NSMetadataItem] {
            items.forEach { delegate?.screenShotObserver?(self, addedItem: $0) }
        } else if let items = notification.userInfo?[kMDQueryUpdateChangedItems] as? [NSMetadataItem] {
            items.forEach { delegate?.screenShotObserver?(self, updatedItem: $0) }
        } else if let items = notification.userInfo?[kMDQueryUpdateRemovedItems] as? [NSMetadataItem] {
            items.forEach { delegate?.screenShotObserver?(self, removedItem: $0) }
        }
    }
}

extension ScreenShotObserver: NSMetadataQueryDelegate {}
