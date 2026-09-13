import AppKit
import Settings
import Sharing
import SwiftUI
import UniformTypeIdentifiers

struct ExcludedApplicationsSettingsView: View {
    @Shared(.excludedApplications)
    private var applications

    @State
    private var selection: Set<String> = []
    @State
    private var isImporting = false

    private var canRemoveSelection: Bool {
        !selection.isDisjoint(with: applications.map(\.identifier))
    }

    var body: some View {
        VStack(alignment: .leading) {
            List(selection: $selection) {
                ForEach(applications, id: \.identifier) { application in
                    ApplicationSettingsRow(application: application)
                        .tag(application.identifier)
                }
            }
            .listStyle(.inset)
            .accessibilityLabel(Text(.Settings.excludedApplications))
            .frame(height: 320)
            .overlay {
                if applications.isEmpty {
                    VStack(spacing: 8) {
                        Image(systemName: "app.dashed")
                            .font(.largeTitle)
                            .accessibilityHidden(true)
                        Text(.Settings.noExcludedApps)
                            .font(.headline)
                        Text(.Settings.addAnAppToStopSavingItsClipboardHistory)
                            .settingDescription()
                    }
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
                    .padding()
                    .allowsHitTesting(false)
                }
            }
            .border(.separator)
            .onDeleteCommand(perform: removeSelection)

            ControlGroup {
                Button {
                    isImporting = true
                } label: {
                    Label(.Settings.addApps, systemImage: "plus")
                }
                Button(action: removeSelection) {
                    Label(.Settings.remove, systemImage: "minus")
                }
                .disabled(!canRemoveSelection)
            }
            .labelStyle(.iconOnly)
        }
        .frame(width: 500)
        .padding(.vertical, 20)
        .padding(.horizontal, 30)
        .fileImporter(
            isPresented: $isImporting,
            allowedContentTypes: [.applicationBundle],
            allowsMultipleSelection: true,
            onCompletion: handleImport
        )
        .fileDialogDefaultDirectoryIfAvailable(
            FileManager.default.urls(for: .applicationDirectory, in: .localDomainMask).first
        )
    }

    private func handleImport(_ result: Result<[URL], Error>) {
        guard case let .success(urls) = result else { return }
        var additions: [ApplicationInformation] = []
        for url in urls {
            let hasAccess = url.startAccessingSecurityScopedResource()
            defer {
                if hasAccess {
                    url.stopAccessingSecurityScopedResource()
                }
            }
            guard let info = Bundle(url: url)?.infoDictionary,
                  let application = ApplicationInformation(info: info as [String: AnyObject]) else {
                continue
            }
            additions.append(application)
        }
        $applications.withLock { applications in
            for application in additions where !applications.contains(where: { $0.identifier == application.identifier }) {
                applications.append(application)
            }
        }
        selection.removeAll()
    }

    private func removeSelection() {
        $applications.withLock { $0.removeAll { selection.contains($0.identifier) } }
        selection.removeAll()
    }
}

private struct ApplicationSettingsRow: View {
    let application: ApplicationInformation

    private var icon: NSImage {
        if let url = NSWorkspace.shared.urlForApplication(withBundleIdentifier: application.identifier) {
            return NSWorkspace.shared.icon(forFile: url.path)
        }
        return NSWorkspace.shared.icon(for: .applicationBundle)
    }

    var body: some View {
        HStack(spacing: 10) {
            Image(nsImage: icon)
                .resizable()
                .frame(width: 28, height: 28)
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 4) {
                Text(application.name)
                Text(application.identifier)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            .lineLimit(1)
        }
        .padding(.vertical, 4)
    }
}

private extension View {
    @ViewBuilder
    func fileDialogDefaultDirectoryIfAvailable(_ directory: URL?) -> some View {
        if #available(macOS 14.0, *) {
            fileDialogDefaultDirectory(directory)
        } else {
            self
        }
    }
}
