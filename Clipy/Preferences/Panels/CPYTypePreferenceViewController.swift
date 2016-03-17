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
        storeTypes = (defaults.objectForKey(kCPYPrefStoreTypesKey) as! NSMutableDictionary).mutableCopy() as! NSMutableDictionary
        super.loadView()
    }
    
}
