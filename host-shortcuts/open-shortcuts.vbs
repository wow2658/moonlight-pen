Option Explicit
Dim shell, fso, folder, explorerExe
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
folder = fso.BuildPath(shell.SpecialFolders("Programs"), "Moonlight Display Shortcuts")
If Not fso.FolderExists(folder) Then
    WScript.Echo "The installed shortcut folder was not found for this Windows user."
    WScript.Echo "Run INSTALL-HOST-SHORTCUTS.cmd on the Sunshine host under the same Windows account."
    WScript.Quit 1
End If
explorerExe = shell.ExpandEnvironmentStrings("%SystemRoot%\explorer.exe")
shell.Run Chr(34) & explorerExe & Chr(34) & " " & Chr(34) & folder & Chr(34), 1, False
