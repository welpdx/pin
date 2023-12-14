@echo off
cls

:: Compiling the C file using GCC
::gcc -o pttb pttb.c -Lmingw64/x86_64-w64-mingw32/lib -lole32 -loleaut32 -luuid -s -O3 -Wl,--gc-sections -nostartfiles --entry=pttb

::added ico
echo IDI_MYICON ICON pin.ico > icon.rc
timeout /t 1 > nul
windres icon.rc -O coff -o icon.res
timeout /t 1 > nul
gcc -o pttb pttb.c icon.res -Lmingw64/x86_64-w64-mingw32/lib -lole32 -loleaut32 -luuid -s -O3 -Wl,--gc-sections -nostartfiles --entry=pttb
timeout /t 1 > nul

:: Checking if pttb.exe was created successfully
IF NOT EXIST pttb.exe GOTO END

:: Displaying the size of pttb.exe
for %%I in (pttb.exe) do echo %%~zI

:: Stripping symbols and sections from the binary
strip --strip-all pttb.exe

:: Displaying the size of pttb.exe after stripping
for %%I in (pttb.exe) do echo %%~zI

:END
echo build complete
timeout /t 3 > nul