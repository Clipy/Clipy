# Make it more obvious that a PR is a work in progress and shouldn't be merged yet
warn("PR is classed as Work in Progress") if github.pr_title.include? "[WIP]"

# Don't let testing shortcuts get into master
fail("fdescribe left in tests") if `grep -r fdescribe ClipyTests/`.length > 1
fail("fit left in tests") if `grep -rI "fit(" ClipyTests/`.length > 1
fail("fcontext left in tests") if `grep -rI "fcontext(" ClipyTests/`.length > 1

# Confirm commit message
if !git.commits.all? { |c| c.message.start_with?(":") }
  warn("Please check the message style of the commit. \nCheck style [CONTRIBUTING.md](https://github.com/Clipy/Clipy/blob/master/.github/CONTRIBUTING.md).")
end
