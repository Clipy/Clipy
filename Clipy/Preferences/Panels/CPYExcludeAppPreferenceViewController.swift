//
//  CPYExcludeAppPreferenceViewController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/08/08.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYExcludeAppPreferenceViewController: NSViewController {
    // MARK: - Properties
    @IBOutlet weak var tableView: NSTableView!
}

// MARK: - IBActions
extension CPYExcludeAppPreferenceViewController {
    @IBAction func addAppButtonTapped(sender: AnyObject) {
        let openPanel = NSOpenPanel()
        openPanel.allowedFileTypes = ["app"]
        openPanel.allowsMultipleSelection = true
        openPanel.resolvesAliases = true
        openPanel.prompt = LocalizedString.Add.value
        let directories = NSSearchPathForDirectoriesInDomains(.ApplicationDirectory, .LocalDomainMask, true)
        let basePath = (directories.isEmpty) ? NSHomeDirectory() : directories.first!
        openPanel.directoryURL = NSURL(fileURLWithPath: basePath)

        let returnCode = openPanel.runModal()
        if returnCode != NSOKButton { return }

        let fileURLs = openPanel.URLs
        fileURLs.forEach {
            guard let bundle = NSBundle(URL: $0), info = bundle.infoDictionary else { return }
            guard let appInfo = CPYAppInfo(info: info) else { return }
            ExcludeAppManager.sharedManager.addExcludeApp(appInfo)
            tableView.reloadData()
        }
    }

    @IBAction func deleteAppButtonTapped(sender: AnyObject) {
        let index = tableView.selectedRow
        if index == -1 {
            NSBeep()
            return
        }
        ExcludeAppManager.sharedManager.deleteExcludeApp(index)
        tableView.reloadData()
    }
}

// MARK: - NSTableView DataSource
extension CPYExcludeAppPreferenceViewController: NSTableViewDataSource {
    func numberOfRowsInTableView(tableView: NSTableView) -> Int {
        return ExcludeAppManager.sharedManager.applications.count
    }

    func tableView(tableView: NSTableView, objectValueForTableColumn tableColumn: NSTableColumn?, row: Int) -> AnyObject? {
        return ExcludeAppManager.sharedManager.applications[row].name
    }
}
