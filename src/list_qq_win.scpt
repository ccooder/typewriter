tell application "System Events"
	set qq_win to {}
	repeat with theProcess in processes
		if not background only of theProcess then
			tell theProcess
				set processName to name
				set theWindows to windows
			end tell
			set windowsCount to count of theWindows

			if processName is "QQ" then
				if windowsCount is greater than 0 then
					repeat with theWindow in theWindows
						tell theWindow

							set end of qq_win to name of theWindow
						end tell
					end repeat
				end if
			end if
		end if
	end repeat
	return qq_win
end tell