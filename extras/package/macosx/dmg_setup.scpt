on run argv
    tell application "Finder"
        tell disk (item 1 of argv)
            open
            set current view of container window to icon view
            set toolbar visible of container window to false
            set statusbar visible of container window to false
            set the bounds of container window to {300, 100, 750, 500}
            set theViewOptions to the icon view options of container window
            set arrangement of theViewOptions to not arranged
            set icon size of theViewOptions to 104
            # Don't set a background image, for now
            # set background picture of theViewOptions to file ".background/background.png"
            set position of item "VLC.app" of container window to {110, 100}
            set position of item "Applications" of container window to {335, 100}
            set position of item "Read Me.rtf" of container window to {110, 275}
            set position of item "Goodies" of container window to {335, 275}
            # Force saving changes to the disk by closing and opening the window
            close
            open
            update without registering applications
            delay 5
        end tell
    end tell
end run
