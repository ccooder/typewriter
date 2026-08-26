on run argv
    set bundleID to "com.tencent.qq"
    
    -- 检查是否传递了参数
    if (count of argv) = 0 then
        return "错误：请提供窗口标题作为参数"
    end if
    
    set targetWindowTitle to item 1 of argv
    set action to item 2 of argv
    
    try
    	tell application "System Events"
    		set appProcess to first process whose bundle identifier is bundleID
    		set appName to name of appProcess
    	end tell

    	-- 组合多种方法确保窗口激活
    	tell application appName
    		activate
    		delay 0.3
    	end tell

    	tell application "System Events"
    		tell process appName
    			-- 查找窗口
    			set targetWindow to first window whose name is targetWindowTitle

    			-- 方法1：尝试设置main属性
    			set value of attribute "AXMain" of targetWindow to true

    			-- 方法2：尝试聚焦窗口
    			try
    				set focused of targetWindow to true
    			end try

    			-- 方法3：模拟点击窗口
    			try
    				perform action "AXRaise" of targetWindow
    			end try
    		end tell
    	end tell

    	return "success"
    on error errMsg
    	return "error"
    end try
end run
