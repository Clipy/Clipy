# Apply SwiftLint
swiftlint.config_file = '.swiftlint.yml'
swiftlint.lint_files inline_mode: true

# Don't let testing shortcuts get into master
fail("fdescribe left in tests") if `grep -r fdescribe ClipyTests/`.length > 1
fail("fit left in tests") if `grep -rI "fit(" ClipyTests/`.length > 1
fail("fcontext left in tests") if `grep -rI "fcontext(" ClipyTests/`.length > 1
