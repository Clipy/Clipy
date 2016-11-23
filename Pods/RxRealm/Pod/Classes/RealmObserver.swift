//
//  RealmObserver.swift
//  Pods
//
//  Created by sergdort on 6/3/16.
//
//

import Foundation
import RxSwift
import RealmSwift

/**
 `RealmObserver` retains target realm object until it receives a .Completed or .Error event
  or the observer is being deinitialized
 */
class RealmObserver<E>: ObserverType {
    var realm: Realm?
    var configuration: Realm.Configuration?
    
    let binding: (Realm, E) -> Void
    
    init(realm: Realm, binding: @escaping (Realm, E) -> Void) {
        self.realm = realm
        self.binding = binding
    }

    init(configuration: Realm.Configuration, binding: @escaping (Realm, E) -> Void) {
        self.configuration = configuration
        self.binding = binding
    }
    
    /**
     Binds next element
     */
    func on(_ event: Event<E>) {
        switch event {
        case .next(let element):
            //this will "cache" the realm on this thread, until completed/errored
            if let configuration = configuration, realm == nil {
                let realm = try! Realm(configuration: configuration)
                binding(realm, element)
                return;
            }
            
            guard let realm = realm else {
                fatalError("No realm in RealmObserver at time of a .Next event")
            }
            
            binding(realm, element)
        
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
    func asObserver() -> AnyObserver<E> {
        return AnyObserver(eventHandler: on)
    }
    
    deinit {
        realm = nil
    }
}
