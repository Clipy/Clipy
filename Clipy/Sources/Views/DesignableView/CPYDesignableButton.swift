//
//  CPYDesignableButton.swift
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

class CPYDesignableButton: NSButton {

    @IBInspectable var textColor: NSColor = NSColor(resource: .title)

    // MARK: - Initialize
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        initView()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        initView()
    }

    private func initView() {
        let attributedString = NSAttributedString(string: title, attributes: [.foregroundColor: textColor])
        attributedTitle = attributedString
    }
}
