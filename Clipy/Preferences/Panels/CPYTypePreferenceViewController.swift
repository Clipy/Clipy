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
    private let defaults = NSUserDefaults.standardUserDefaults()

    // MARK: - Initialize
    override func loadView() {
        if let types = defaults.objectForKey(kCPYPrefStoreTypesKey)?.mutableCopy() as? NSMutableDictionary {
            storeTypes = types
        } else {
            storeTypes = NSMutableDictionary()
        }
        super.loadView()
    }

}
