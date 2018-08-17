//
//  InputSource.swift
//
//  Sauce
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2018/07/30.
//
//  Copyright © 2018 Clipy Project.
//

import Foundation
import Carbon

public final class InputSource {

    // MARK: - Properties
    public let id: String
    public let modeID: String?
    public let isASCIICapable: Bool
    public let isEnableCapable: Bool
    public let isSelectCapable: Bool
    public let isEnabled: Bool
    public let isSelected: Bool
    public let localizedName: String?
    public let source: TISInputSource

    // MARK: - Initialize
    init(source: TISInputSource) {
        self.id = source.value(forProperty: kTISPropertyInputSourceID, type: String.self)!
        self.modeID = source.value(forProperty: kTISPropertyInputModeID, type: String.self)
        self.isASCIICapable = source.value(forProperty: kTISPropertyInputSourceIsASCIICapable, type: Bool.self) ?? false
        self.isEnableCapable = source.value(forProperty: kTISPropertyInputSourceIsEnableCapable, type: Bool.self) ?? false
        self.isSelectCapable = source.value(forProperty: kTISPropertyInputSourceIsSelectCapable, type: Bool.self) ?? false
        self.isEnabled = source.value(forProperty: kTISPropertyInputSourceIsEnabled, type: Bool.self) ?? false
        self.isSelected = source.value(forProperty: kTISPropertyInputSourceIsSelected, type: Bool.self) ?? false
        self.localizedName = source.value(forProperty: kTISPropertyLocalizedName, type: String.self)
        self.source = source
    }

}

// MARK: - Hashable
extension InputSource: Hashable {
    public var hashValue: Int {
        return id.hashValue ^ (modeID?.hashValue ?? 0)
    }
}

// MARK: - Equatable
extension InputSource: Equatable {
    public static func == (lhs: InputSource, rhs: InputSource) -> Bool {
        return lhs.id == rhs.id &&
            lhs.modeID == rhs.modeID
    }
}
