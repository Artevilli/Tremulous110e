@echo off
set F=%TEMP%\tremulous.reg
echo REGEDIT4 > %F%
rem echo [-HKEY_CLASSES_ROOT\tremulous] >> %F%
echo [-HKEY_CURRENT_USER\Software\Classes\tremulous] >> %F%
regedit -s %F%
del %F%
