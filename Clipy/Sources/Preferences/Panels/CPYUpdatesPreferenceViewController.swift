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
        versionTextField.stringValue = "v\(Bundle.main.appVersion ?? "")"
    }

}
