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
    @IBAction func addAppButtonTapped(_ sender: AnyObject) {
        let openPanel = NSOpenPanel()
        openPanel.allowedFileTypes = ["app"]
        openPanel.allowsMultipleSelection = true
        openPanel.resolvesAliases = true
        openPanel.prompt = LocalizedString.Add.value
        let directories = NSSearchPathForDirectoriesInDomains(.applicationDirectory, .localDomainMask, true)
        let basePath = (directories.isEmpty) ? NSHomeDirectory() : directories.first!
        openPanel.directoryURL = URL(fileURLWithPath: basePath)

        let returnCode = openPanel.runModal()
        if returnCode != NSModalResponseOK { return }

        let fileURLs = openPanel.urls
        fileURLs.forEach {
            guard let bundle = Bundle(url: $0), let info = bundle.infoDictionary else { return }
            guard let appInfo = CPYAppInfo(info: info as [String : AnyObject]) else { return }
            ExcludeAppService.shared.add(with: appInfo)
            tableView.reloadData()
        }
    }

    @IBAction func deleteAppButtonTapped(_ sender: AnyObject) {
        let index = tableView.selectedRow
        if index == -1 {
            NSBeep()
            return
        }
        ExcludeAppService.shared.delete(with: index)
        tableView.reloadData()
    }
}

// MARK: - NSTableView DataSource
extension CPYExcludeAppPreferenceViewController: NSTableViewDataSource {
    func numberOfRows(in tableView: NSTableView) -> Int {
        return ExcludeAppService.shared.applications.count
    }

    func tableView(_ tableView: NSTableView, objectValueFor tableColumn: NSTableColumn?, row: Int) -> Any? {
        return ExcludeAppService.shared.applications[row].name
    }
}
