# Privacy Policy

Last updated: June 4, 2026

Clipy is a clipboard extension app for macOS. Clipboard data can contain
sensitive information, so Clipy will never transmit clipboard contents or
snippet contents to external services without the user's consent.

This policy clarifies whether Clipy collects personally identifying information,
whether Clipy collects copied contents, and what network communication Clipy
performs.

## Summary

- Clipy does not ask users to provide names, email addresses, account
  information, or other directly identifying personal information.
- Clipy does not transmit clipboard text, clipboard images, snippets, or other
  copied contents to Clipy servers, Firebase, or any other third-party service.
- Clipy's intended network communication is limited to update checks via
  `clipy-app.com` and diagnostics/usage measurement via Firebase.
- Firebase Analytics / Firebase Crashlytics are enabled by default and can be
  disabled in Clipy's Preferences.

## Data Stored Locally

Clipy stores clipboard history, snippets, preferences, and related app data
locally on your Mac.

Clipboard contents and snippets are not sent to external services by Clipy.
However, clipboard managers can store sensitive information locally. We
recommend excluding password managers and other sensitive apps from Clipy's
history recording when possible.

Clipy does not currently claim that locally stored clipboard history is
encrypted. If your Mac contains sensitive data, we recommend enabling FileVault
and using macOS security features appropriately.

## Network Communication

Clipy's intended network communication is limited to the following services:

- `clipy-app.com`: used by Sparkle to check for app updates.
- Firebase: used for analytics and crash reporting.

Clipy does not intentionally use other network services.

## Firebase Analytics

Clipy uses Firebase Analytics to understand basic app usage, such as the number
of users, app launches, app version, macOS version, language, and general feature
usage.

Firebase Analytics is enabled by default. You can disable analytics in Clipy's
Preferences.

Clipy does not use Firebase Analytics to collect user-created or copied content,
including clipboard contents, snippet contents, file contents, copied secrets,
or personal information entered by the user.

## Firebase Crashlytics

Clipy uses Firebase Crashlytics to collect crash reports and error diagnostics.
Crash reports help us understand and fix stability problems.

Crash reports may include information such as:

- App version
- macOS version
- Device model or architecture
- Stack traces
- Crash timestamps
- Firebase installation identifiers
- Diagnostic logs added by Clipy

Clipy does not intentionally include clipboard contents or snippet contents in
Crashlytics logs, custom keys, or error reports.

You can disable crash reporting in Clipy's Preferences.

## Update Checks

Clipy uses Sparkle to check for updates from `clipy-app.com`. Update checks may
send standard request information such as the app version, macOS version, and IP
address to the update server.

## Third-Party Processing

Firebase is provided by Google. Data sent to Firebase is processed according to
Google's Firebase terms and privacy documentation.

Sparkle is used for update checks. The update feed is hosted by Clipy on
`clipy-app.com`.

## User Controls

You can enable or disable analytics and crash reporting from Clipy's Preferences.

When disabled, Clipy will stop intentionally sending analytics and crash
diagnostic data from future app usage. Some previously sent data may remain in
Firebase according to Firebase's retention policies.

## Changes

This privacy policy may be updated when Clipy's behavior or third-party services
change.

## Contact

For privacy or security questions, please open an issue on GitHub.

GitHub: https://github.com/Clipy/Clipy
