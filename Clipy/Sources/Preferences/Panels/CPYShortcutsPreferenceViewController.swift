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
fileprivate extension CPYShortcutsPreferenceViewController {
    fileprivate func prepareHotKeys() {
        mainShortcutRecordView.keyCombo = AppEnvironment.current.hotKeyService.mainKeyCombo
        historyShortcutRecordView.keyCombo = AppEnvironment.current.hotKeyService.historyKeyCombo
        snippetShortcutRecordView.keyCombo = AppEnvironment.current.hotKeyService.snippetKeyCombo
    }
}

// MARK: - RecordView Delegate
extension CPYShortcutsPreferenceViewController: RecordViewDelegate {
    func recordViewShouldBeginRecording(_ recordView: RecordView) -> Bool {
        return true
    }

    func recordView(_ recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool {
        return true
    }

    func recordViewDidClearShortcut(_ recordView: RecordView) {
        switch recordView {
        case mainShortcutRecordView:
            AppEnvironment.current.hotKeyService.change(with: .main, keyCombo: nil)
        case historyShortcutRecordView:
            AppEnvironment.current.hotKeyService.change(with: .history, keyCombo: nil)
        case snippetShortcutRecordView:
            AppEnvironment.current.hotKeyService.change(with: .snippet, keyCombo: nil)
        default: break
        }
    }

    func recordView(_ recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo) {
        switch recordView {
        case mainShortcutRecordView:
            AppEnvironment.current.hotKeyService.change(with: .main, keyCombo: keyCombo)
        case historyShortcutRecordView:
            AppEnvironment.current.hotKeyService.change(with: .history, keyCombo: keyCombo)
        case snippetShortcutRecordView:
            AppEnvironment.current.hotKeyService.change(with: .snippet, keyCombo: keyCombo)
        default: break
        }
    }

    func recordViewDidEndRecording(_ recordView: RecordView) {}
}
