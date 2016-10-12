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

final class RxScreeenDelegateProxy: DelegateProxy, ScreenShotObserverDelegate, DelegateProxyType {
    static func currentDelegateFor(object: AnyObject) -> AnyObject? {
        let screeen: ScreenShotObserver = (object as? ScreenShotObserver)!
        return screeen.delegate
    }

    static func setCurrentDelegate(delegate: AnyObject?, toObject object: AnyObject) {
        let screeen: ScreenShotObserver = (object as? ScreenShotObserver)!
        screeen.delegate = delegate as? ScreenShotObserverDelegate
    }
}
