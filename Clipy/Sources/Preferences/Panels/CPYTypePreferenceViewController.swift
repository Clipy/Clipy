//
//  CPYTypePreferenceViewController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/03/17.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Sharing

final class CPYTypePreferenceViewController: NSViewController {
    // MARK: - Properties
    @objc var storeTypes: NSMutableDictionary!

    @Shared(.pasteboardTypeSettings)
    private var pasteboardTypeSettings

    // MARK: - Initialize
    override func loadView() {
        let values = PasteboardAvailableType.allCases.reduce(into: [String: Any]()) { values, type in
            values[type.rawValue] = NSNumber(value: pasteboardTypeSettings[type])
        }
        storeTypes = NSMutableDictionary(dictionary: values)
        super.loadView()
        PasteboardAvailableType.allCases.forEach { availableType in
            storeTypes.addObserver(self, forKeyPath: availableType.rawValue, options: .new, context: nil)
        }
    }

    deinit {
        PasteboardAvailableType.allCases.forEach { availableType in
            storeTypes.removeObserver(self, forKeyPath: availableType.rawValue)
        }
    }

    // swiftlint:disable:next block_based_kvo
    override func observeValue(forKeyPath keyPath: String?, of object: Any?, change: [NSKeyValueChangeKey: Any]?, context: UnsafeMutableRawPointer?) {
        guard let dictionary = object as? NSMutableDictionary, dictionary == storeTypes else { return }
        guard let keyPath, let type = PasteboardAvailableType(rawValue: keyPath) else { return }
        guard let value = storeTypes[keyPath] as? NSNumber else { return }
        $pasteboardTypeSettings.withLock { $0[type] = value.boolValue }
    }
}
