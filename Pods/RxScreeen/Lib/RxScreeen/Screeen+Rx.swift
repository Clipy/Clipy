//
//  Screeen+Rx.swift
//  RxScreeen
//
//  Created by 古林俊佑 on 2016/07/16.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Screeen
import RxSwift
import RxCocoa

public extension ScreenShotObserver {
    /**
     Reactive wrapper for `delegate`.
     For more information take a look at `DelegateProxyType` protocol documentation.
     */
    public var rx_delegate: DelegateProxy {
        return proxyForObject(RxScreeenDelegateProxy.self, self)
    }
}

public extension ScreenShotObserver {

    // MARK: - Responding to screenshot image
    public var rx_image: Observable<NSImage> {
        return Observable
                .of(rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:))),
                    rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:))),
                    rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:))))
                .merge()
                .flatMap { a -> Observable<NSImage> in
                    guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                    guard let imagePath = metadataItem.valueForAttribute("kMDItemPath") as? String else { return Observable.empty() }
                    guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                    return Observable.just(image)
                }
    }

    // MARK: - Responding to screenshot item
    public var rx_item: Observable<NSMetadataItem> {
        return Observable
                .of(rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:))),
                    rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:))),
                    rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:))))
                .merge()
                .flatMap { a -> Observable<NSMetadataItem> in
                    guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                    return Observable.just(metadataItem)
                }
    }

    // MARK: - Delegate methods
    public var rx_addedItem: Observable<NSMetadataItem> {
        return rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:)))
                .flatMap { a -> Observable<NSMetadataItem> in
                    guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                    return Observable.just(metadataItem)
                }
    }

    public var rx_addedImage: Observable<NSImage> {
        return rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:)))
                .flatMap { a -> Observable<NSImage> in
                    guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                    guard let imagePath = metadataItem.valueForAttribute("kMDItemPath") as? String else { return Observable.empty() }
                    guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                    return Observable.just(image)
                }
    }

    public var rx_updatedItem: Observable<NSMetadataItem> {
        return rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:)))
                .flatMap { a -> Observable<NSMetadataItem> in
                    guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                    return Observable.just(metadataItem)
                }
    }

    public var rx_updatedImage: Observable<NSImage> {
        return rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:)))
                .flatMap { a -> Observable<NSImage> in
                    guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                    guard let imagePath = metadataItem.valueForAttribute("kMDItemPath") as? String else { return Observable.empty() }
                    guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                    return Observable.just(image)
            }
    }

    public var rx_removedItem: Observable<NSMetadataItem> {
        return rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
                .flatMap { a -> Observable<NSMetadataItem> in
                    guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                    return Observable.just(metadataItem)
                }
    }

    public var rx_removedImage: Observable<NSImage> {
        return rx_delegate.observe(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
            .flatMap { a -> Observable<NSImage> in
                guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                guard let imagePath = metadataItem.valueForAttribute("kMDItemPath") as? String else { return Observable.empty() }
                guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                return Observable.just(image)
        }
    }

}
