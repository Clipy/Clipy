# Contributing to Clipy

:tada: Thank you for contributing to Clipy :tada:

## Styleguides

### Git Commit Messages
- Consider starting the commit message with an applicable emoji:
 - 🎨 `:art:` when improving the format/structure of the code
 - 🚀 `:rocket:` when improving performance
 - 🚱 `:non-potable_water:` when plugging memory leaks
 - 📝 `:memo:` when writing docs
 - 🍎 `:apple:` when fixing something on macOS or Xcode
 - 🐛 `:bug:` when fixing a bug
 - 🔥 `:fire:` when removing code or files
 - 💚 `:green_heart:` when fixing the CI build
 - ✅ `:white_check_mark:` when adding tests
 - 🔒 `:lock:` when dealing with security
 - ⬆️ `:arrow_up:` when upgrading dependencies
 - ⬇️ `:arrow_down:` when downgrading dependencies
 - 👕 `:shirt:` when removing linter warnings

**If you are using Clipy please use the following snippet.**
- [English version](https://github.com/Clipy/Clipy/blob/master/.github/git_message_en.xml)
- [Japanese version](https://github.com/Clipy/Clipy/blob/master/.github/git_message_ja.xml)

## Localization

### Add New Language
<img src="../Images/new_localization.png" width="600">

After adding the language, please make changes to the various `.strings` files as follows.

### Modify an Existing Language
The files to be localized are as follows.
- Localizable.strings ( `Clipy/Resources/#{language_name}.lproj/Localizable.strings` )
- Preferences ( `Clipy/Sources/Preferences/#{language_name}.lproj/*.strings` )
- PreferencesPanels ( `Clipy/Sources/Preferences/Panels/#{language_name}.lproj/*.strings` ) 
- SnippetsEditor ( `Clipy/Sources/Snippets/#{language_name}.lproj/*.strings` ) 

**English localization only, please edit `.xib` files directly**
