fastlane documentation
----

# Installation

Make sure you have the latest version of the Xcode command line tools installed:

```sh
xcode-select --install
```

For _fastlane_ installation instructions, see [Installing _fastlane_](https://docs.fastlane.tools/#installing-fastlane)

# Available Actions

## Mac

### mac sync_development_signing

```sh
[bundle exec] fastlane mac sync_development_signing
```

Sync macOS development certificates and profiles (read-only by default)

Use force:true to allow certificate creation and regenerate provisioning profiles

API key options key_id, issuer_id, and key_content override ENV; key_content must be Base64 encoded

### mac sync_developer_id_signing

```sh
[bundle exec] fastlane mac sync_developer_id_signing
```

Sync Developer ID certificates and profiles for production distribution (read-only by default)

Use force:true to regenerate provisioning profiles using a valid imported certificate

API key options key_id, issuer_id, and key_content override ENV; key_content must be Base64 encoded

### mac import_developer_id_signing

```sh
[bundle exec] fastlane mac import_developer_id_signing
```

Import a manually created Developer ID certificate and private key into match storage

Prompts for the .cer and .p12 paths and an optional provisioning profile

API key options key_id, issuer_id, and key_content override ENV; key_content must be Base64 encoded

----

This README.md is auto-generated and will be re-generated every time [_fastlane_](https://fastlane.tools) is run.

More information about _fastlane_ can be found on [fastlane.tools](https://fastlane.tools).

The documentation of _fastlane_ can be found on [docs.fastlane.tools](https://docs.fastlane.tools).
