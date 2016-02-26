//
//  ObservableType+Clipy.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/02/26.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import RxCocoa
import RxSwift

extension ObservableType where E: OptionalType {
    func ignoreNil() -> Observable<E.Wrapped> {
        return self.flatMap { (v: E) -> Observable<E.Wrapped> in
            if let w = v.value {
                return Observable.just(w)
            } else {
                return Observable.empty()
            }
        }
    }
}