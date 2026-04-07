@echo off
set CWD=%~dp0 
set CWD=%CWD:~0,-2%
set CWD=%CWD:\=\\%
set F=%TEMP%\tremulous.reg
set RPATH=HKEY_CURRENT_USER\Software\Classes\tremulous
rem set RPATH=HKEY_CLASSES_ROOT\tremulous
echo REGEDIT4 > %F%
echo [%RPATH%] >> %F%
echo @="URL:TREM (Tremulous 1.1.0e)" >> %F%
echo "URL Protocol"="" >> %F%
echo [%RPATH%\shell\open\command] >> %F%
echo @="\"%CWD%\\tremulous.exe\" +set fs_basepath \"%CWD%\" +set fs_homepath \"%CWD%\" +connect \"%%1\"" >> %F%
regedit -s %F%
del %F%
