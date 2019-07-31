//
//  HotKey.swift
//  Magnet
//
//  Created by 古林俊佑 on 2016/03/09.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Carbon

public final class HotKey: Equatable {

    // MARK: - Properties
    public let identifier: String
    public let keyCombo: KeyCombo
    public let callback: ((HotKey) -> Void)?
    public let target: AnyObject?
    public let action: Selector?
    public let actionQueue: ActionQueue

    var hotKeyId: UInt32?
    var hotKeyRef: EventHotKeyRef?

    // MARK: - Enum Value
    public enum ActionQueue {
        case main
        case session

        public func execute(closure: @escaping () -> Void) {
            switch self {
            case .main:
                DispatchQueue.main.async {
                    closure()
                }
            case .session:
                closure()
            }
        }
    }

    // MARK: - Initialize
    public init(identifier: String, keyCombo: KeyCombo, target: AnyObject, action: Selector, actionQueue: ActionQueue = .main) {
        self.identifier     = identifier
        self.keyCombo       = keyCombo
        self.callback       = nil
        self.target         = target
        self.action         = action
        self.actionQueue    = actionQueue
    }

    public init(identifier: String, keyCombo: KeyCombo, actionQueue: ActionQueue = .main, handler: @escaping ((HotKey) -> Void)) {
        self.identifier     = identifier
        self.keyCombo       = keyCombo
        self.callback       = handler
        self.target         = nil
        self.action         = nil
        self.actionQueue    = actionQueue
    }
    
}

// MARK: - Invoke
public extension HotKey {
    func invoke() {
        guard let callback = self.callback else {
            guard let target = self.target as? NSObject, let selector = self.action else { return }
            guard target.responds(to: selector) else { return }
            actionQueue.execute { [weak self] in
                guard let wSelf = self else { return }
                target.perform(selector, with: wSelf)
            }
            return
        }
        actionQueue.execute { [weak self] in
            guard let wSelf = self else { return }
            callback(wSelf)
        }
    }
}

// MARK: - Register & UnRegister
public extension HotKey {
    @discardableResult
    func register() -> Bool {
        return HotKeyCenter.shared.register(with: self)
    }

    func unregister() {
        return HotKeyCenter.shared.unregister(with: self)
    }
}

// MARK: - Equatable
public func == (lhs: HotKey, rhs: HotKey) -> Bool {
    return lhs.identifier == rhs.identifier &&
            lhs.keyCombo == rhs.keyCombo &&
            lhs.hotKeyId == rhs.hotKeyId &&
            lhs.hotKeyRef == rhs.hotKeyRef
}
