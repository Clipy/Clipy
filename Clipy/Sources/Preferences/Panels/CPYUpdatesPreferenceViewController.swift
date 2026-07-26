//
//  CPYUpdatesPreferenceViewController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/03/17.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Combine
import Dependencies
import Sparkle

class CPYUpdatesPreferenceViewController: NSViewController {

    // MARK: - Properties
    @IBOutlet private weak var lastUpdateCheckDateTextField: NSTextField!
    @IBOutlet private weak var versionTextField: NSTextField!

    @Dependency(\.sparkle)
    private var sparkle
    private var cancellables: Set<AnyCancellable> = []

    // MARK: - Initialize
    override func loadView() {
        super.loadView()
        sparkle.lastUpdateCheckDate()
            .assign(to: \.objectValue, on: lastUpdateCheckDateTextField)
            .store(in: &cancellables)
        versionTextField.stringValue = "v\(Bundle.main.appVersion ?? "")"
    }

    @IBAction private func checkForUpdates(_ sender: Any) {
        sparkle.checkForUpdates(sender: sender)
    }
}
