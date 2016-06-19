//
//  CPYShortcutsPreferenceViewController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/02/26.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import KeyHolder
import Magnet

class CPYShortcutsPreferenceViewController: NSViewController {

    // MARK: - Properties
    @IBOutlet weak var mainShortcutRecordView: RecordView!
    @IBOutlet weak var historyShortcutRecordView: RecordView!
    @IBOutlet weak var snippetShortcutRecordView: RecordView!

    private let defaults = NSUserDefaults.standardUserDefaults()

    // MARK: - Initialize
    override func loadView() {
        super.loadView()
        mainShortcutRecordView.delegate = self
        historyShortcutRecordView.delegate = self
        snippetShortcutRecordView.delegate = self
        prepareHotKeys()
    }

}

// MARK: - Shortcut
private extension CPYShortcutsPreferenceViewController {
    private func prepareHotKeys() {
        mainShortcutRecordView.keyCombo = HotKeyManager.sharedManager.mainKeyCombo.value
        historyShortcutRecordView.keyCombo = HotKeyManager.sharedManager.historyKeyCombo.value
        snippetShortcutRecordView.keyCombo = HotKeyManager.sharedManager.snippetKeyCombo.value
    }
}

// MARK: - RecordView Delegate
extension CPYShortcutsPreferenceViewController: RecordViewDelegate {
    func recordViewShouldBeginRecording(recordView: RecordView) -> Bool {
        return true
    }

    func recordView(recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool {
        return true
    }

    func recordViewDidClearShortcut(recordView: RecordView) {
        if recordView == mainShortcutRecordView {
            HotKeyManager.sharedManager.mainKeyCombo.value = nil
        } else if recordView == historyShortcutRecordView {
            HotKeyManager.sharedManager.historyKeyCombo.value = nil
        } else if recordView == snippetShortcutRecordView {
            HotKeyManager.sharedManager.snippetKeyCombo.value = nil
        }
    }

    func recordView(recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo) {
        if recordView == mainShortcutRecordView {
            HotKeyManager.sharedManager.mainKeyCombo.value = keyCombo
        } else if recordView == historyShortcutRecordView {
            HotKeyManager.sharedManager.historyKeyCombo.value = keyCombo
        } else if recordView == snippetShortcutRecordView {
            HotKeyManager.sharedManager.snippetKeyCombo.value = keyCombo
        }
    }

    func recordViewDidEndRecording(recordView: RecordView) {}
}
