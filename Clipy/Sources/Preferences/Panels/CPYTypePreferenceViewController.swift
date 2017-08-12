//
//  CPYTypePreferenceViewController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/17.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYTypePreferenceViewController: NSViewController {

    // MARK: - Properties
    var storeTypes: NSMutableDictionary!

    // MARK: - Initialize
    override func loadView() {
        if let dictionary = AppEnvironment.current.defaults.object(forKey: Constants.UserDefaults.storeTypes) as? [String: Any] {
            storeTypes = NSMutableDictionary(dictionary: dictionary)
        } else {
            storeTypes = NSMutableDictionary()
        }
        super.loadView()
    }

}
