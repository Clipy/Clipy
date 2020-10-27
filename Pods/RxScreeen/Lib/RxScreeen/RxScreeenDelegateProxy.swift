//
//  RxScreeenDelegateProxy.swift
//
//  RxScreeen
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Copyright © 2015-2020 Clipy Project.
//

import Foundation
import Screeen
import RxSwift
import RxCocoa

extension ScreenShotObserver: HasDelegate {
    public typealias Delegate = ScreenShotObserverDelegate
}

final class RxScreeenDelegateProxy: DelegateProxy<ScreenShotObserver, ScreenShotObserverDelegate>, DelegateProxyType, ScreenShotObserverDelegate {

    public weak private(set) var screeen: ScreenShotObserver?

    public init(screeen: ParentObject) {
        self.screeen = screeen
        super.init(parentObject: screeen, delegateProxy: RxScreeenDelegateProxy.self)
    }

    static func registerKnownImplementations() {
        self.register { RxScreeenDelegateProxy(screeen: $0) }
    }

}
