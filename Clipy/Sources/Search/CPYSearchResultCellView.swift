//
//  CPYSearchResultCellView.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/25.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

final class CPYSearchResultRowView: NSTableRowView {

    override func drawSelection(in dirtyRect: NSRect) {
        if isSelected {
            let selectionRect = bounds.insetBy(dx: 6, dy: 2)
            let path = NSBezierPath(roundedRect: selectionRect, xRadius: 8, yRadius: 8)
            NSColor.selectedContentBackgroundColor.withAlphaComponent(0.85).setFill()
            path.fill()
        }
    }

    override var interiorBackgroundStyle: NSView.BackgroundStyle {
        isSelected ? .emphasized : .normal
    }
}

final class CPYSearchResultCellView: NSTableCellView {

    // MARK: - Properties
    static let reuseIdentifier = NSUserInterfaceItemIdentifier("CPYSearchResultCellView")

    private let iconImageView = NSImageView()
    private let titleLabel = NSTextField(labelWithString: "")
    private let subtitleLabel = NSTextField(labelWithString: "")
    private let kindBadgeLabel = NSTextField(labelWithString: "")

    // MARK: - Initialize
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupViews()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupViews()
    }

    private func setupViews() {
        wantsLayer = true

        iconImageView.translatesAutoresizingMaskIntoConstraints = false
        iconImageView.imageScaling = .scaleProportionallyUpOrDown
        iconImageView.wantsLayer = true
        iconImageView.layer?.cornerRadius = 4
        iconImageView.layer?.masksToBounds = true
        addSubview(iconImageView)

        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.font = .systemFont(ofSize: 13, weight: .medium)
        titleLabel.lineBreakMode = .byTruncatingTail
        titleLabel.maximumNumberOfLines = 1
        addSubview(titleLabel)

        subtitleLabel.translatesAutoresizingMaskIntoConstraints = false
        subtitleLabel.font = .systemFont(ofSize: 11, weight: .regular)
        subtitleLabel.textColor = .secondaryLabelColor
        subtitleLabel.lineBreakMode = .byTruncatingTail
        subtitleLabel.maximumNumberOfLines = 1
        addSubview(subtitleLabel)

        kindBadgeLabel.translatesAutoresizingMaskIntoConstraints = false
        kindBadgeLabel.font = .systemFont(ofSize: 10, weight: .medium)
        kindBadgeLabel.textColor = .tertiaryLabelColor
        kindBadgeLabel.alignment = .right
        addSubview(kindBadgeLabel)

        NSLayoutConstraint.activate([
            iconImageView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 14),
            iconImageView.centerYAnchor.constraint(equalTo: centerYAnchor),
            iconImageView.widthAnchor.constraint(equalToConstant: 24),
            iconImageView.heightAnchor.constraint(equalToConstant: 24),

            titleLabel.leadingAnchor.constraint(equalTo: iconImageView.trailingAnchor, constant: 10),
            titleLabel.topAnchor.constraint(equalTo: topAnchor, constant: 6),
            titleLabel.trailingAnchor.constraint(lessThanOrEqualTo: kindBadgeLabel.leadingAnchor, constant: -8),

            subtitleLabel.leadingAnchor.constraint(equalTo: iconImageView.trailingAnchor, constant: 10),
            subtitleLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 2),
            subtitleLabel.trailingAnchor.constraint(lessThanOrEqualTo: kindBadgeLabel.leadingAnchor, constant: -8),

            kindBadgeLabel.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -14),
            kindBadgeLabel.centerYAnchor.constraint(equalTo: centerYAnchor),
            kindBadgeLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 50)
        ])
    }

    // MARK: - Configure
    func configure(with item: SearchResultItem) {
        titleLabel.stringValue = item.title
        subtitleLabel.stringValue = item.subtitle
        toolTip = item.toolTip

        switch item {
        case .history:
            if let thumbnail = item.thumbnailImage {
                iconImageView.image = thumbnail
            } else {
                iconImageView.image = NSImage(systemSymbolName: "doc.on.clipboard", accessibilityDescription: nil)
                    ?? NSImage(resource: .iconText)
            }
            kindBadgeLabel.stringValue = String(localized: "History")
        case .snippet:
            iconImageView.image = NSImage(resource: .iconText)
            kindBadgeLabel.stringValue = String(localized: "Snippet")
        }
    }

    override var backgroundStyle: NSView.BackgroundStyle {
        didSet {
            let isEmphasized = backgroundStyle == .emphasized
            titleLabel.textColor = isEmphasized ? .white : .labelColor
            subtitleLabel.textColor = isEmphasized ? NSColor.white.withAlphaComponent(0.8) : .secondaryLabelColor
            kindBadgeLabel.textColor = isEmphasized ? NSColor.white.withAlphaComponent(0.6) : .tertiaryLabelColor
        }
    }
}
