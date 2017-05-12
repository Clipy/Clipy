# Contributing to Clipy

:tada: Thank you for contributing to Clipy :tada:

## Localization

### Add New Language
<img src="../Resources/new_localization.png" width="600">

After adding the language, please make changes to the various `.strings` files as follows.

### Modify an Existing Language
The files to be localized are as follows.
- Localizable.strings ( `Clipy/Resources/#{language_name}.lproj/Localizable.strings` )
- Preferences ( `Clipy/Sources/Preferences/#{language_name}.lproj/*.strings` )
- PreferencesPanels ( `Clipy/Sources/Preferences/Panels/#{language_name}.lproj/*.strings` )
- SnippetsEditor ( `Clipy/Sources/Snippets/#{language_name}.lproj/*.strings` )

**English localization only, please edit `.xib` files directly**
