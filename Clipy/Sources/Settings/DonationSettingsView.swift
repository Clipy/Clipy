import SwiftUI

struct DonationSettingsView: View {
    var body: some View {
        VStack(spacing: 24) {
            Text(.Settings.donationMessage)
                .foregroundStyle(.primary)
                .lineSpacing(4)

            HStack(spacing: 16) {
                donationOption(
                    "Open Collective",
                    image: .openCollectiveLogo,
                    destination: URL(string: "https://opencollective.com/clipy")!
                )
                donationOption(
                    "GitHub Sponsors",
                    image: .gitHubLogo,
                    destination: URL(string: "https://github.com/sponsors/Econa77")!
                )
            }
            .padding(.horizontal, 32)
        }
        .frame(width: 500)
        .padding(.vertical, 20)
        .padding(.horizontal, 30)
    }

    private func donationOption(
        _ title: String,
        image: ImageResource,
        destination: URL
    ) -> some View {
        VStack(spacing: 12) {
            VStack(spacing: 8) {
                Image(image)
                    .resizable()
                    .scaledToFit()
                    .frame(width: 32, height: 32)
                    .accessibilityHidden(true)
                Text(verbatim: title)
            }

            Link(.Settings.support, destination: destination)
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
        }
        .frame(maxWidth: .infinity)
        .foregroundStyle(.primary)
        .accessibilityElement(children: .contain)
    }
}
