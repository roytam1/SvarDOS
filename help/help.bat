@ECHO OFF
IF EXIST %DOSDIR%\HELP\HELP-%LANG%.AMB GOTO USELANG
AMB %DOSDIR%\HELP\HELP-EN.AMB %1
GOTO DONE

:USELANG
AMB %DOSDIR%\HELP\HELP-%LANG%.AMB %1

:DONE
