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

final class CPYTypePreferenceViewController: NSViewController {
    // MARK: - Properties
    @objc var storeTypes: NSMutableDictionary!

    // MARK: - Initialize
    override func loadView() {
        if let dictionary = AppEnvironment.current.defaults.object(forKey: Constants.UserDefaults.storeTypes) as? [String: Any] {
            storeTypes = NSMutableDictionary(dictionary: dictionary)
        } else {
            storeTypes = NSMutableDictionary()
        }
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
        AppEnvironment.current.defaults.set(storeTypes, forKey: Constants.UserDefaults.storeTypes)
    }
}
