tell application "System Events"
    get the name of every login item
    if login item "%@" exists then
        delete login item "%@"
    end if
end tell