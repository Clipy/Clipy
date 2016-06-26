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
        switch recordView {
        case mainShortcutRecordView:
            HotKeyManager.sharedManager.mainKeyCombo.value = nil
        case historyShortcutRecordView:
            HotKeyManager.sharedManager.historyKeyCombo.value = nil
        case snippetShortcutRecordView:
            HotKeyManager.sharedManager.snippetKeyCombo.value = nil
        default: break
        }
    }

    func recordView(recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo) {
        switch recordView {
        case mainShortcutRecordView:
            HotKeyManager.sharedManager.mainKeyCombo.value = keyCombo
        case historyShortcutRecordView:
            HotKeyManager.sharedManager.historyKeyCombo.value = keyCombo
        case snippetShortcutRecordView:
            HotKeyManager.sharedManager.snippetKeyCombo.value = keyCombo
        default: break
        }
    }

    func recordViewDidEndRecording(recordView: RecordView) {}
}
