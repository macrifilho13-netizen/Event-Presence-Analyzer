Dim fso, scriptDir
Set fso = CreateObject("Scripting.FileSystemObject")
scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)

Set WshShell = CreateObject("WScript.Shell")
WshShell.Run "powershell.exe -ExecutionPolicy Bypass -NoExit -Command ""cd '" & scriptDir & "'; .\interface_gui.ps1; if ($?) { Write-Host ''; Write-Host 'Pressione ENTER para sair...'; Read-Host } else { Write-Host ''; Write-Host 'ERRO! Pressione ENTER para sair...'; Read-Host }""", 1, False
Set WshShell = Nothing
