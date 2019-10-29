// 
//  PasteboardTypeSpec.swift
//
//  ClipyTests
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
// 
//  Created by 胡继续 on 2019/8/12.
// 
//  Copyright © 2015-2019 Clipy Project.
//

import Foundation
import Quick
import Nimble
@testable import Clipy

class PasteboardTypeSpec: QuickSpec {
    override func spec() {
        // swiftlint:disable quick_discouraged_call
        AvailableType.allCases.forEach { type in
            describe("AvailableType." + type.rawValue) {
                type.targetPbTypes.forEach { pbType in
                    it(pbType.rawValue) {
                        expect(pbType.isString) === type.isString
                        expect(pbType.isRTF) === type.isRTF
                        expect(pbType.isRTFD) === type.isRTFD
                        expect(pbType.isPDF) === type.isPDF
                        expect(pbType.isFilenames) === type.isFilenames
                        expect(pbType.isURL) === type.isURL
                    }
                }
            }
        }
        // swiftlint:enable quick_discouraged_call
    }
}
