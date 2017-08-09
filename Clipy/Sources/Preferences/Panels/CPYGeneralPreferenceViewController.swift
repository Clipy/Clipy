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
        let realm = try! Realm()
        let defaults = UserDefaults.standard
        let maxHistory = defaults.integer(forKey: Constants.UserDefaults.maxHistorySize)
        let ascending = !defaults.bool(forKey: Constants.UserDefaults.reorderClipsAfterPasting)
        let clips = realm.objects(CPYClip.self).sorted(byKeyPath: #keyPath(CPYClip.updateTime), ascending: ascending)

        let xmlDocument = AEXMLDocument()
        let rootElement = xmlDocument.addChild(name: Constants.HistoryXml.rootElement)

        for (index, clip) in clips.enumerated() {
            if maxHistory <= index { break }

            guard !clip.isInvalidated else { continue }
            guard let clipData = NSKeyedUnarchiver.unarchiveObject(withFile: clip.dataPath) as? CPYClipData else { continue }
            guard !clipData.stringValue.isEmpty else { continue }

            let historyElement = rootElement.addChild(name: Constants.HistoryXml.historyElement)
            historyElement.addChild(name: Constants.HistoryXml.contentElement, value: clipData.stringValue)
        }

        let panel = NSSavePanel()
        panel.accessoryView = nil
        panel.canSelectHiddenExtension = true
        panel.allowedFileTypes = [Constants.HistoryXml.fileType]
        panel.allowsOtherFileTypes = false
        panel.directoryURL = URL(fileURLWithPath: NSHomeDirectory())
        panel.nameFieldStringValue = "clipy_history"
        let returnCode = panel.runModal()

        if returnCode != NSModalResponseOK { return }

        guard let data = xmlDocument.xml.data(using: String.Encoding.utf8) else { return }
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
            let realm = try! Realm()
            let xmlDocument = try AEXMLDocument(xml: data)
            xmlDocument[Constants.HistoryXml.rootElement]
                .children
                .forEach { historyElement in
                    if let content = historyElement[Constants.HistoryXml.contentElement].value {
                        let data = CPYClipData(string: content)
                        // Saved time and path
                        let unixTime = Int(Date().timeIntervalSince1970)
                        let savedPath = CPYUtilities.applicationSupportFolder() + "/\(NSUUID().uuidString).data"
                        // Overwrite same history
                        let isOverwriteHistory = UserDefaults.standard.bool(forKey: Constants.UserDefaults.overwriteSameHistory)
                        let savedHash = (isOverwriteHistory) ? data.hash : Int(arc4random() % 1000000)
                        // Create Realm object
                        let clip = CPYClip()
                        clip.dataPath = savedPath
                        clip.title = data.stringValue[0...10000]
                        clip.dataHash = "\(savedHash)"
                        clip.updateTime = unixTime
                        clip.primaryType = data.primaryType ?? ""

                        if CPYUtilities.prepareSaveToPath(CPYUtilities.applicationSupportFolder()) {
                            if NSKeyedArchiver.archiveRootObject(data, toFile: savedPath) {
                                realm.transaction {
                                    realm.add(clip, update: true)
                                }
                            }
                        }
                    }
                }
        } catch {
            NSBeep()
        }
    }
}
