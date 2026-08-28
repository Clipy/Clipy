//
//  CPYShortcutsPreferenceViewController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/02/26.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Dependencies
import KeyHolder
import Magnet

class CPYShortcutsPreferenceViewController: NSViewController {

    // MARK: - Properties
    @IBOutlet private weak var mainShortcutRecordView: RecordView!
    @IBOutlet private weak var historyShortcutRecordView: RecordView!
    @IBOutlet private weak var snippetShortcutRecordView: RecordView!
    @IBOutlet private weak var searchShortcutRecordView: RecordView!
    @IBOutlet private weak var clearHistoryShortcutRecordView: RecordView!

    @Dependency(\.hotKeyService)
    private var hotKeyService

    // MARK: - Initialize
    override func loadView() {
        super.loadView()
        mainShortcutRecordView.delegate = self
        historyShortcutRecordView.delegate = self
        snippetShortcutRecordView.delegate = self
        searchShortcutRecordView.delegate = self
        clearHistoryShortcutRecordView.delegate = self
        prepareHotKeys()
    }

}

// MARK: - Shortcut
private extension CPYShortcutsPreferenceViewController {
    func prepareHotKeys() {
        mainShortcutRecordView.keyCombo = hotKeyService.mainKeyCombo
        historyShortcutRecordView.keyCombo = hotKeyService.historyKeyCombo
        snippetShortcutRecordView.keyCombo = hotKeyService.snippetKeyCombo
        searchShortcutRecordView.keyCombo = hotKeyService.searchKeyCombo
        clearHistoryShortcutRecordView.keyCombo = hotKeyService.clearHistoryKeyCombo
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

    func recordView(_ recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo?) {
        switch recordView {
        case mainShortcutRecordView:
            hotKeyService.change(with: .main, keyCombo: keyCombo)
        case historyShortcutRecordView:
            hotKeyService.change(with: .history, keyCombo: keyCombo)
        case snippetShortcutRecordView:
            hotKeyService.change(with: .snippet, keyCombo: keyCombo)
        case searchShortcutRecordView:
            hotKeyService.change(with: .search, keyCombo: keyCombo)
        case clearHistoryShortcutRecordView:
            hotKeyService.changeClearHistoryKeyCombo(keyCombo)
        default: break
        }
    }

    func recordViewDidEndRecording(_ recordView: RecordView) {}
}
