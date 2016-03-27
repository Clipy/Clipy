//
//  CPYUpdatesPreferenceViewController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/17.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYUpdatesPreferenceViewController: NSViewController {

    // MARK: - Properties
    @IBOutlet weak var versionTextField: NSTextField!

    // MARK: - Initialize
    override func loadView() {
        super.loadView()
        if let versionString = NSBundle.mainBundle().objectForInfoDictionaryKey("CFBundleShortVersionString") as? String {
            versionTextField.stringValue = "v\(versionString)"
        }
    }
    
}
