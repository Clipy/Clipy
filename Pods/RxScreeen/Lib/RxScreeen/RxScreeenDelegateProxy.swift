//
//  RxScreeenDelegateProxy.swift
//  RxScreeen
//
//  Created by 古林俊佑 on 2016/07/16.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
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
