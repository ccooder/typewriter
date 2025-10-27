set bundleID to "com.tencent.qq"

try
	tell application "System Events"
		set appProcess to first process whose bundle identifier is bundleID
		set appName to name of appProcess
	end tell

	set qq_win_list to {}
	tell appProcess
		set processName to name
		set theWindows to windows
	end tell

	set windowsCount to count of theWindows
	if windowsCount is greater than 0 then
		repeat with theWindow in theWindows
			tell theWindow
				set end of qq_win_list to name of theWindow
			end tell
		end repeat
	end if

	return qq_win_list
on error errMsg
	return "error"
end try