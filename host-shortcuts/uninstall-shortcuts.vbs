Option Explicit
Dim shell, fso, installDir, displayExe, explorerExe
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
installDir = fso.BuildPath(shell.SpecialFolders("Programs"), "Moonlight Display Shortcuts")
If WScript.Arguments.Named.Exists("test") Then installDir = fso.BuildPath(fso.GetParentFolderName(WScript.ScriptFullName), "shortcut-test")
displayExe = shell.ExpandEnvironmentStrings("%SystemRoot%\System32\DisplaySwitch.exe")
explorerExe = shell.ExpandEnvironmentStrings("%SystemRoot%\explorer.exe")
DeleteLink "Clone.lnk", displayExe, "/clone"
DeleteLink "Extend.lnk", displayExe, "/extend"
DeleteLink "Check.lnk", explorerExe, "ms-settings:display"
If fso.FolderExists(installDir) Then
    If fso.GetFolder(installDir).Files.Count = 0 And fso.GetFolder(installDir).SubFolders.Count = 0 Then fso.DeleteFolder installDir, False
End If
WScript.Echo "Removed only the three registered Moonlight shortcuts. Display mode was not changed."

Sub DeleteLink(name, target, args)
    Dim linkPath, link
    linkPath = fso.BuildPath(installDir, name)
    If Not fso.FileExists(linkPath) Then Exit Sub
    Set link = shell.CreateShortcut(linkPath)
    If LCase(link.TargetPath) <> LCase(target) Or link.Arguments <> args Then
        WScript.Echo "Skipped an unrelated shortcut: " & name
        Exit Sub
    End If
    link.Hotkey = ""
    link.Save
    fso.DeleteFile linkPath, True
End Sub
