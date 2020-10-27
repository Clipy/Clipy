//
//  Screeen+Rx.swift
//
//  RxScreeen
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Copyright © 2015-2020 Clipy Project.
//

import AppKit
import Screeen
import RxSwift
import RxCocoa

public extension Reactive where Base: ScreenShotObserver {

    /**
     Reactive wrapper for `delegate`.

     For more information take a look at `DelegateProxyType` protocol documentation.
     */
    var delegate: DelegateProxy<ScreenShotObserver, ScreenShotObserverDelegate> {
        return RxScreeenDelegateProxy.proxy(for: base)
    }

    // MARK: - Responding to screenshot image
    var image: Observable<NSImage> {
        return Observable.merge(
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:))),
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:))),
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
        )
        .flatMap { argument -> Observable<NSImage> in
            guard let metadataItem = argument[1] as? NSMetadataItem else { return Observable.empty() }
            guard let imagePath = metadataItem.value(forAttribute: "kMDItemPath") as? String else { return Observable.empty() }
            guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

            return Observable.just(image)
        }
    }

    // MARK: - Responding to screenshot item
    var item: Observable<NSMetadataItem> {
        return Observable.merge(
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:))),
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:))),
            delegate.methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
        )
        .flatMap { argument -> Observable<NSMetadataItem> in
            guard let metadataItem = argument[1] as? NSMetadataItem else { return Observable.empty() }

            return Observable.just(metadataItem)
        }
    }

    // MARK: - Delegate methods
    var addedItem: Observable<NSMetadataItem> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:)))
            .flatMap { argument -> Observable<NSMetadataItem> in
                guard let metadataItem = argument[1] as? NSMetadataItem else { return Observable.empty() }

                return Observable.just(metadataItem)
            }
    }

    var addedImage: Observable<NSImage> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:addedItem:)))
            .flatMap { argument -> Observable<NSImage> in
                guard let metadataItem = argument[1] as? NSMetadataItem else { return Observable.empty() }
                guard let imagePath = metadataItem.value(forAttribute: "kMDItemPath") as? String else { return Observable.empty() }
                guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                return Observable.just(image)
            }
    }

    var updatedItem: Observable<NSMetadataItem> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:)))
            .flatMap { argument -> Observable<NSMetadataItem> in
                guard let metadataItem = argument[1] as? NSMetadataItem else { return Observable.empty() }

                return Observable.just(metadataItem)
            }
    }

    var updatedImage: Observable<NSImage> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:updatedItem:)))
            .flatMap { argument -> Observable<NSImage> in
                guard let metadataItem = argument[1] as? NSMetadataItem else { return Observable.empty() }
                guard let imagePath = metadataItem.value(forAttribute: "kMDItemPath") as? String else { return Observable.empty() }
                guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                return Observable.just(image)
            }
    }

    var removedItem: Observable<NSMetadataItem> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
            .flatMap { argument -> Observable<NSMetadataItem> in
                guard let metadataItem = argument[1] as? NSMetadataItem else { return Observable.empty() }

                return Observable.just(metadataItem)
            }
    }

    var removedImage: Observable<NSImage> {
        return delegate
            .methodInvoked(#selector(ScreenShotObserverDelegate.screenShotObserver(_:removedItem:)))
            .flatMap { argument -> Observable<NSImage> in
                guard let metadataItem = argument[1] as? NSMetadataItem else { return Observable.empty() }
                guard let imagePath = metadataItem.value(forAttribute: "kMDItemPath") as? String else { return Observable.empty() }
                guard let image = NSImage(contentsOfFile: imagePath) else { return Observable.empty() }

                return Observable.just(image)
            }
    }

}
