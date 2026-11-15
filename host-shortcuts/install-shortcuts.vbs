Option Explicit
Dim shell, fso, wmi, installDir, testMode, displayExe, explorerExe, oldHelpers
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
testMode = WScript.Arguments.Named.Exists("test")
displayExe = shell.ExpandEnvironmentStrings("%SystemRoot%\System32\DisplaySwitch.exe")
explorerExe = shell.ExpandEnvironmentStrings("%SystemRoot%\explorer.exe")
If Not fso.FileExists(displayExe) Or Not fso.FileExists(explorerExe) Then
    WScript.Echo "Required Windows components are missing. Nothing was installed."
    WScript.Quit 1
End If
If testMode Then
    installDir = fso.BuildPath(fso.GetParentFolderName(WScript.ScriptFullName), "shortcut-test")
Else
    Set wmi = GetObject("winmgmts:\\.\root\cimv2")
    If wmi.ExecQuery("SELECT Name FROM Win32_Process WHERE Name='sunshine.exe'").Count = 0 Then
        If wmi.ExecQuery("SELECT Name FROM Win32_Service WHERE Name LIKE '%sunshine%'").Count = 0 Then
            WScript.Echo "Sunshine host was not found. Run this on the host PC, not the pen laptop."
            WScript.Quit 1
        End If
    End If
    Set oldHelpers = wmi.ExecQuery("SELECT Name FROM Win32_Process WHERE Name='Moonlight-Display-Helper.exe'")
    If oldHelpers.Count > 0 Then
        WScript.Echo "The old host helper is running. Exit it from its tray icon before installing these shortcuts."
        WScript.Quit 1
    End If
    installDir = fso.BuildPath(shell.SpecialFolders("Programs"), "Moonlight Display Shortcuts")
End If
If Not fso.FolderExists(installDir) Then fso.CreateFolder installDir
CreateLink "Clone.lnk", displayExe, "/clone", "CTRL+ALT+SHIFT+D", 68, "Moonlight: duplicate host displays"
CreateLink "Extend.lnk", displayExe, "/extend", "CTRL+ALT+SHIFT+E", 69, "Moonlight: extend host displays"
CreateLink "Check.lnk", explorerExe, "ms-settings:display", "CTRL+ALT+SHIFT+H", 72, "Moonlight: open host display settings"
WScript.Echo "PASS: created and verified all three shortcuts."
WScript.Echo "Location: " & installDir
WScript.Echo "No display mode was changed by this installer."
WScript.Echo "Protocol v2: Ctrl+Alt+Shift+D / E / H (not the old F13/F14/F15 shortcuts)."
If Not testMode Then
    WScript.Echo "Reconnect Moonlight and click ? in the screen panel."
    WScript.Echo "The host Display Settings page should open. Then test Clone / Extend."
    WScript.Echo "To diagnose: first open Check.lnk directly, then press Ctrl+Alt+Shift+H on the host keyboard."
    WScript.Echo "If hotkeys do not respond, sign out and sign back in to the host once, after saving your work."
    shell.Run Chr(34) & explorerExe & Chr(34) & " " & Chr(34) & installDir & Chr(34), 1, False
End If

Sub CreateLink(name, target, args, hotkey, virtualKey, description)
    Dim linkPath, link, existing, stream, bytes, failure
    linkPath = fso.BuildPath(installDir, name)
    If fso.FileExists(linkPath) Then
        Set existing = shell.CreateShortcut(linkPath)
        If LCase(existing.TargetPath) <> LCase(target) Or existing.Arguments <> args Then
            Err.Raise vbObjectError + 1, , "An unrelated shortcut exists: " & linkPath
        End If
    End If
    Set link = shell.CreateShortcut(linkPath)
    link.TargetPath = target
    link.Arguments = args
    link.Hotkey = hotkey
    link.WorkingDirectory = fso.GetParentFolderName(target)
    link.Description = description
    link.WindowStyle = 1
    link.Save
    Set link = shell.CreateShortcut(linkPath)
    If LCase(link.TargetPath) <> LCase(target) Or link.Arguments <> args Then
        Err.Raise vbObjectError + 2, , "Shortcut target verification failed."
    End If
    ' Shell Link header: byte 64 = virtual key, byte 65 = SHIFT|CTRL|ALT (7).
    Set stream = CreateObject("ADODB.Stream")
    stream.Type = 1
    stream.Open
    stream.LoadFromFile linkPath
    stream.Position = 64
    bytes = stream.Read(2)
    failure = (AscB(MidB(bytes, 1, 1)) <> virtualKey Or AscB(MidB(bytes, 2, 1)) <> 7)
    stream.Close
    If failure Then Err.Raise vbObjectError + 3, , "Shortcut hotkey verification failed."
    WScript.Echo "Verified: " & name & "  " & hotkey
End Sub
