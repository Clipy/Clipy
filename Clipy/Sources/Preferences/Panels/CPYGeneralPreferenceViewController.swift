//
//  CPYGeneralPreferenceViewController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2017/08/09.
//  Copyright © 2017年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RealmSwift
import AEXML

final class CPYGeneralPreferenceViewController: NSViewController {}

// MARK: - IBActions
extension CPYGeneralPreferenceViewController {
    @IBAction func exportHistoryButtonTapped(_ sender: Any) {
        let exportXml = AppEnvironment.current.clipService.exportClipboard()

        let panel = NSSavePanel()
        panel.accessoryView = nil
        panel.canSelectHiddenExtension = true
        panel.allowedFileTypes = [Constants.HistoryXml.fileType]
        panel.allowsOtherFileTypes = false
        panel.directoryURL = URL(fileURLWithPath: NSHomeDirectory())
        panel.nameFieldStringValue = "clipy_history"
        let returnCode = panel.runModal()

        if returnCode != NSModalResponseOK { return }

        guard let data = exportXml.xml.data(using: String.Encoding.utf8) else { return }
        guard let url = panel.url else { return }

        do {
            try data.write(to: url, options: .atomic)
        } catch {
            NSBeep()
        }
    }

    @IBAction func importHistoryButtonTapped(_ sender: Any) {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.directoryURL = URL(fileURLWithPath: NSHomeDirectory())
        panel.allowedFileTypes = [Constants.HistoryXml.fileType]
        let returnCode = panel.runModal()

        if returnCode != NSModalResponseOK { return }

        let fileURLs = panel.urls
        guard let url = fileURLs.first else { return }
        guard let data = try? Data(contentsOf: url) else { return }

        do {
            let xmlDocument = try AEXMLDocument(xml: data)
            AppEnvironment.current.clipService.importClipboard(with: xmlDocument)
        } catch {
            NSBeep()
        }
    }
}
