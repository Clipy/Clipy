import SwiftUI

struct NewBadge: View {
    private static let maximumViewCount = 5

    @Binding private var viewCount: Int
    private let settingValue: Bool

    @State private var hasAppeared = false
    @State private var isVisible: Bool

    init(viewCount: Binding<Int>, settingValue: Bool) {
        _viewCount = viewCount
        self.settingValue = settingValue
        _isVisible = State(initialValue: viewCount.wrappedValue < Self.maximumViewCount)
    }

    var body: some View {
        if isVisible {
            Text(verbatim: "NEW")
                .font(.system(size: 9, weight: .semibold))
                .foregroundStyle(Color.accentColor)
                .padding(.horizontal, 5)
                .padding(.vertical, 2)
                .background(Color.accentColor.opacity(0.25), in: Capsule())
                .accessibilityHidden(true)
                .onAppear {
                    guard !hasAppeared else { return }
                    hasAppeared = true
                    if viewCount < Self.maximumViewCount {
                        viewCount += 1
                    }
                }
                .onChange(of: settingValue) {
                    viewCount = Self.maximumViewCount
                }
        }
    }
}
