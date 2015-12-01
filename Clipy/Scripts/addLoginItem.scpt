tell application "System Events"
    make login item at end with properties {path:"%@", kind:application}
end tell