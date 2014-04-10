@echo off
REM Must have a subfolder called 'doxygen', which contains the exe and config
REM config has '../src', which is why we cd into it here. Cannot run from a
REM network share unless it's mapped to a drive letter!
cd doxygen
doxygen.exe winwipe.doxygen > doxygen.log
copy html\doxygen.css doxygen_original.css
copy custom.css html\doxygen.css
pause > nul