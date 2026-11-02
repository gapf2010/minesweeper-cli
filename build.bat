@echo off
rem Builds both games with MinGW gcc, or MSVC cl when gcc is not installed.
where gcc >nul 2>nul
if %errorlevel%==0 (
  gcc -O2 -o mine-classic.exe minesweeper-classic.c || exit /b 1
  gcc -O2 -o mine-path.exe minesweeper-pathfinder.c || exit /b 1
) else (
  cl /nologo /O2 /Fe:mine-classic.exe minesweeper-classic.c || exit /b 1
  cl /nologo /O2 /Fe:mine-path.exe minesweeper-pathfinder.c || exit /b 1
)
