@echo off

REM NOTE: this batch file must be executed from administrative cmd

REM install the driver
sc create SecuredFileSystem type= filesys binPath= C:\secured-file-system\kernel-mode\cyber-class-project.sys
sc start SecuredFileSystem

REM install required modules 
pip install -r C:\secured-file-system\requirements.txt

REM start the server in a hidden manner
start pythonw.exe C:\secured-file-system\user-mode\server.pyw

REM start UserMode.exe in a hidden manner:
REM create the VBScript file
echo Set oShell = CreateObject("Wscript.Shell") > ".\invisible.vbs"
echo oShell.Run "C:\secured-file-system\user-mode\UserMode.exe", 0, true >> ".\invisible.vbs"

REM run the VBScript
cscript .\invisible.vbs

del .\invisible.vbs

exit
