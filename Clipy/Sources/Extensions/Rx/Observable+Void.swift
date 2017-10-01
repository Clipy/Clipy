//
//  Observable+Void.swift
//  Clipy
//
//  Created by 古林俊佑 on 2017/07/20.
//  Copyright © 2017年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import RxSwift
import RxCocoa
import RxOptional

extension ObservableType {
    func mapVoid() -> Observable<Void> {
        return map { _ in }
    }
}

extension ObservableType where E: Equatable {
    func mapVoidDistinctUntilChanged() -> Observable<Void> {
        return distinctUntilChanged().map { _ in }
    }
}

extension ObservableType where E: OptionalType, E.Wrapped: Equatable {
    func mapVoidDistinctUntilChanged() -> Observable<Void> {
        return filterNil().distinctUntilChanged().map { _ in }
    }
}

extension SharedSequenceConvertibleType where SharingStrategy == DriverSharingStrategy {
    func mapVoid() -> Driver<Void> {
        return map { _ in  }
    }
}

extension SharedSequenceConvertibleType where SharingStrategy == DriverSharingStrategy, E: Equatable {
    func mapVoidDistinctUntilChanged() -> Driver<Void> {
        return distinctUntilChanged().map { _ in }
    }
}

extension SharedSequenceConvertibleType where SharingStrategy == DriverSharingStrategy, E: OptionalType, E.Wrapped: Equatable {
    func mapVoidDistinctUntilChanged() -> Driver<Void> {
        return filterNil().distinctUntilChanged().map { _ in }
    }
}
