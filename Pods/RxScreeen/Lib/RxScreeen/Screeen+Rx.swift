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

extension Reactive where Base: ScreenShotObserver {

    /**
     Reactive wrapper for `delegate`.

     For more information take a look at `DelegateProxyType` protocol documentation.
     */
    public var delegate: DelegateProxy<ScreenShotObserver, ScreenShotObserverDelegate> {
        return RxScreeenDelegateProxy.proxy(for: base)
    }

    // MARK: - Responding to screenshot image
    public var image: Observable<NSImage> {
        return Observable.merge(
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:))),
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:))),
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
        )
        .flatMap { a -> Observable<NSImage> in
            guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
            guard let imagePath = metadataItem.value(forAttribute: "kMDItemPath") as? String else { return Observable.empty() }
            guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

            return Observable.just(image)
        }
    }

    // MARK: - Responding to screenshot item
    public var item: Observable<NSMetadataItem> {
        return Observable.merge(
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:))),
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:))),
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
        )
        .flatMap { a -> Observable<NSMetadataItem> in
            guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }

            return Observable.just(metadataItem)
        }
    }

    // MARK: - Delegate methods
    public var addedItem: Observable<NSMetadataItem> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:)))
            .flatMap { a -> Observable<NSMetadataItem> in
                guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }

                return Observable.just(metadataItem)
            }
    }

    public var addedImage: Observable<NSImage> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:)))
            .flatMap { a -> Observable<NSImage> in
                guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                guard let imagePath = metadataItem.value(forAttribute: "kMDItemPath") as? String else { return Observable.empty() }
                guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                return Observable.just(image)
            }
    }

    public var updatedItem: Observable<NSMetadataItem> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:)))
            .flatMap { a -> Observable<NSMetadataItem> in
                guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }

                return Observable.just(metadataItem)
            }
    }

    public var updatedImage: Observable<NSImage> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:)))
            .flatMap { a -> Observable<NSImage> in
                guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                guard let imagePath = metadataItem.value(forAttribute: "kMDItemPath") as? String else { return Observable.empty() }
                guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                return Observable.just(image)
            }
    }

    public var removedItem: Observable<NSMetadataItem> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
            .flatMap { a -> Observable<NSMetadataItem> in
                guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }

                return Observable.just(metadataItem)
            }
    }

    public var removedImage: Observable<NSImage> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
            .flatMap { a -> Observable<NSImage> in
                guard let metadataItem = a[1] as? NSMetadataItem else { return Observable.empty() }
                guard let imagePath = metadataItem.value(forAttribute: "kMDItemPath") as? String else { return Observable.empty() }
                guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                return Observable.just(image)
            }
    }

}
