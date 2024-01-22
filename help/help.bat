@ECHO OFF
REM :: NO PARAMS GIVEN?
IF "%1"=="" GOTO USERGUIDE

REM :: HELP ON HELP WANTED?
IF %1==/? GOTO HELPONHELP

REM :: UPCASE PARAM #1
SET OLDPATH=%PATH%
PATH %1
SET COMMAND=%PATH%
PATH %OLDPATH%
SET OLDPATH=

REM :: SPECIAL CASE
IF %COMMAND%==HELP GOTO HELPONHELP

REM :: ANY SVARCOM INTERNAL COMMAND?
REM :: ALL UNDERSTAND THE /? SYNTAX
IF %COMMAND%==BREAK    GOTO RUNCOMMANDHELP
IF %COMMAND%==CALL     GOTO RUNCOMMANDHELP
IF %COMMAND%==CD       GOTO RUNCOMMANDHELP
IF %COMMAND%==CHDIR    GOTO RUNCOMMANDHELP
IF %COMMAND%==CHCP     GOTO RUNCOMMANDHELP
IF %COMMAND%==CLS      GOTO RUNCOMMANDHELP
IF %COMMAND%==COPY     GOTO RUNCOMMANDHELP
IF %COMMAND%==DATE     GOTO RUNCOMMANDHELP
IF %COMMAND%==DEL      GOTO RUNCOMMANDHELP
IF %COMMAND%==ERASE    GOTO RUNCOMMANDHELP
IF %COMMAND%==DIR      GOTO RUNCOMMANDHELP
IF %COMMAND%==ECHO     GOTO RUNCOMMANDHELP
IF %COMMAND%==EXIT     GOTO RUNCOMMANDHELP
IF %COMMAND%==FOR      GOTO RUNCOMMANDHELP
IF %COMMAND%==GOTO     GOTO RUNCOMMANDHELP
IF %COMMAND%==IF       GOTO RUNCOMMANDHELP
IF %COMMAND%==LN       GOTO RUNCOMMANDHELP
IF %COMMAND%==MD       GOTO RUNCOMMANDHELP
IF %COMMAND%==MKDIR    GOTO RUNCOMMANDHELP
IF %COMMAND%==PATH     GOTO RUNCOMMANDHELP
IF %COMMAND%==PAUSE    GOTO RUNCOMMANDHELP
IF %COMMAND%==PROMPT   GOTO RUNCOMMANDHELP
IF %COMMAND%==RD       GOTO RUNCOMMANDHELP
IF %COMMAND%==RMDIR    GOTO RUNCOMMANDHELP
IF %COMMAND%==REM      GOTO RUNCOMMANDHELP
IF %COMMAND%==REN      GOTO RUNCOMMANDHELP
IF %COMMAND%==RENAME   GOTO RUNCOMMANDHELP
IF %COMMAND%==SET      GOTO RUNCOMMANDHELP
IF %COMMAND%==SHIFT    GOTO RUNCOMMANDHELP
IF %COMMAND%==TIME     GOTO RUNCOMMANDHELP
IF %COMMAND%==TRUENAME GOTO RUNCOMMANDHELP
IF %COMMAND%==TYPE     GOTO RUNCOMMANDHELP
IF %COMMAND%==VER      GOTO RUNCOMMANDHELP
IF %COMMAND%==VERIFY   GOTO RUNCOMMANDHELP
IF %COMMAND%==VOL      GOTO RUNCOMMANDHELP

REM :: LOOKUP HELP FILE FOR ANY EXTERNAL COMMANDS
IF EXIST %DOSDIR%\HELP\EN\%COMMAND%.AMB SET AMBFILE=EN\%COMMAND%
IF EXIST %DOSDIR%\HELP\%LANG%\%COMMAND%.AMB SET AMBFILE=%LANG%\%COMMAND%
REM :: PARAM #2 BECOMES PARAM #1 TO SPECIFY CHAPTER NAME
IF NOT "%AMBFILE%"=="" SHIFT
IF NOT "%AMBFILE%"=="" GOTO RUNAMB

REM :: EXTERNAL (CORE) COMMAND PRESENT?
IF EXIST %DOSDIR%\%COMMAND%.COM GOTO RUNCOMMANDHELP
IF EXIST %DOSDIR%\%COMMAND%.EXE GOTO RUNCOMMANDHELP
IF EXIST %DOSDIR%\BIN\%COMMAND%.COM GOTO RUNCOMMANDHELP
IF EXIST %DOSDIR%\BIN\%COMMAND%.EXE GOTO RUNCOMMANDHELP

:USERGUIDE
REM :: LOOKUP HELP FILE FOR SVARDOS USER GUIDE
REM :: PARAM #1 SPECIFIES CHAPTER NAME
IF EXIST %DOSDIR%\HELP\HELP-EN.AMB SET AMBFILE=HELP-EN
IF EXIST %DOSDIR%\HELP\HELP-%LANG%.AMB SET AMBFILE=HELP-%LANG%
IF NOT "%AMBFILE%"=="" GOTO RUNAMB

ECHO SORRY! NO HELP AVAILABLE
GOTO DONE

:HELPONHELP
ECHO HELP [COMMAND] [CHAPTER]
GOTO DONE

:RUNCOMMANDHELP
%COMMAND% /?
GOTO DONE

:RUNAMB
AMB %DOSDIR%\HELP\%AMBFILE%.AMB %1

:DONE
SET COMMAND=
SET AMBFILE=
