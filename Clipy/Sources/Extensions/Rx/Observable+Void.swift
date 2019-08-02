//
//  Observable+Void.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2017/07/20.
//
//  Copyright © 2015-2018 Clipy Project.
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

extension ObservableType where Element: Equatable {
    func mapVoidDistinctUntilChanged() -> Observable<Void> {
        return distinctUntilChanged().map { _ in }
    }
}

extension SharedSequenceConvertibleType where SharingStrategy == DriverSharingStrategy {
    func mapVoid() -> Driver<Void> {
        return map { _ in }
    }
}

extension SharedSequenceConvertibleType where SharingStrategy == DriverSharingStrategy, Element: Equatable {
    func mapVoidDistinctUntilChanged() -> Driver<Void> {
        return distinctUntilChanged().map { _ in }
    }
}
