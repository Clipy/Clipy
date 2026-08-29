//
//  CPYExcludeAppPreferenceViewController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/08/08.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Dependencies

class CPYExcludeAppPreferenceViewController: NSViewController {
    // MARK: - Properties
    @IBOutlet private weak var tableView: NSTableView!

    @Dependency(\.excludeAppService)
    private var excludeAppService
}

// MARK: - IBActions
extension CPYExcludeAppPreferenceViewController {
    @IBAction private func addAppButtonTapped(_ sender: AnyObject) {
        let openPanel = NSOpenPanel()
        openPanel.allowedFileTypes = ["app"]
        openPanel.allowsMultipleSelection = true
        openPanel.resolvesAliases = true
        openPanel.prompt = String(localized: "Add")
        let directories = NSSearchPathForDirectoriesInDomains(.applicationDirectory, .localDomainMask, true)
        let basePath = (directories.isEmpty) ? NSHomeDirectory() : directories.first!
        openPanel.directoryURL = URL(fileURLWithPath: basePath)

        let returnCode = openPanel.runModal()
        if returnCode != NSApplication.ModalResponse.OK { return }

        let fileURLs = openPanel.urls
        fileURLs.forEach {
            guard let bundle = Bundle(url: $0), let info = bundle.infoDictionary else { return }
            guard let applicationInformation = ApplicationInformation(info: info as [String: AnyObject]) else { return }
            excludeAppService.add(with: applicationInformation)
        }
        tableView.reloadData()
    }

    @IBAction private func deleteAppButtonTapped(_ sender: AnyObject) {
        let index = tableView.selectedRow
        if index == -1 {
            NSSound.beep()
            return
        }
        excludeAppService.delete(with: index)
        tableView.reloadData()
    }
}

// MARK: - NSTableView DataSource
extension CPYExcludeAppPreferenceViewController: NSTableViewDataSource {
    func numberOfRows(in tableView: NSTableView) -> Int {
        return excludeAppService.applications.count
    }

    func tableView(_ tableView: NSTableView, objectValueFor tableColumn: NSTableColumn?, row: Int) -> Any? {
        return excludeAppService.applications[row].name
    }
}
