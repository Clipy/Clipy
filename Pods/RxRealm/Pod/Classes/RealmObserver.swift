//
//  RxRealm extensions
//
//  Copyright (c) 2016 RxSwiftCommunity. All rights reserved.
//  Created by sergdort on 6/3/16.
//

import Foundation
import RealmSwift
import RxSwift

/**
 `RealmObserver` retains target realm object until it receives a .Completed or .Error event
 or the observer is being deinitialized
 */
class RealmObserver<Element>: ObserverType {
    var realm: Realm?
    var configuration: Realm.Configuration?
    
    let binding: (Realm?, Element, Error?) -> Void
    
    init(realm: Realm, binding: @escaping (Realm?, Element, Error?) -> Void) {
        self.realm = realm
        self.binding = binding
    }
    
    init(configuration: Realm.Configuration, binding: @escaping (Realm?, Element, Error?) -> Void) {
        self.configuration = configuration
        self.binding = binding
    }
    
    /**
     Binds next element
     */
    func on(_ event: Event<Element>) {
        switch event {
        case let .next(element):
            //this will "cache" the realm on this thread, until completed/errored
            if let configuration = configuration, realm == nil {
                do {
                    let realm = try Realm(configuration: configuration)
                    binding(realm, element, nil)
                } catch let e {
                    binding(nil, element, e)
                }
                return
            }
            
            guard let realm = realm else {
                fatalError("No realm in RealmObserver at time of a .Next event")
            }
            
            binding(realm, element, nil)
            
        case .error:
            realm = nil
        case .completed:
            realm = nil
        }
    }
    
    /**
     Erases the type of observer
     
     - returns: AnyObserver, type erased observer
     */
    func asObserver() -> AnyObserver<Element> {
        return AnyObserver(eventHandler: on)
    }
    
    deinit {
        realm = nil
    }
}

