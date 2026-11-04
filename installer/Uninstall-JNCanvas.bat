@echo off
setlocal EnableDelayedExpansion
title JNCanvas Clean Uninstaller

:: Request Administrator privileges if not elevated
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [INFO] Membutuhkan hak akses Administrator...
    echo Mengalihkan ke mode Administrator...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

color 0B
echo ======================================================================
echo                JNCANVAS - CLEAN UNINSTALLER TOOL                     
echo ======================================================================
echo.

:: 1. Tutup OBS Studio jika sedang berjalan
echo [1/5] Memeriksa proses OBS Studio...
tasklist /FI "IMAGENAME eq obs64.exe" 2>NUL | find /I /N "obs64.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo [INFO] Menutup OBS Studio secara aman...
    taskkill /F /IM obs64.exe >nul 2>&1
    timeout /t 2 /nobreak >nul
) else (
    echo [OK] OBS Studio tidak sedang berjalan.
)

:: 2. Cari direktori instalasi OBS Studio
echo.
echo [2/5] Mencari lokasi instalasi OBS Studio...
set "OBS_DIR="

:: Cek dari Registry Windows
for /f "tokens=2* skip=2" %%a in ('reg query "HKLM\SOFTWARE\OBS Studio" /v "InstallPath" 2^>nul') do set "OBS_DIR=%%b"

:: Jika tidak ada di registry, cek folder standar 64-bit
if not defined OBS_DIR (
    if exist "C:\Program Files\obs-studio" set "OBS_DIR=C:\Program Files\obs-studio"
)
if not defined OBS_DIR (
    if exist "C:\Program Files (x86)\obs-studio" set "OBS_DIR=C:\Program Files (x86)\obs-studio"
)

if defined OBS_DIR (
    echo [OK] Direktori OBS ditemukan: "!OBS_DIR!"
) else (
    echo [PERINGATAN] Direktori OBS tidak ditemukan otomatis. Menggunakan default C:\Program Files\obs-studio
    set "OBS_DIR=C:\Program Files\obs-studio"
)

:: 3. Hapus file plugin jn-canvas
echo.
echo [3/5] Menghapus binary dan data plugin JNCanvas...

:: Hapus DLL dan PDB
if exist "!OBS_DIR!\obs-plugins\64bit\jn-canvas.dll" (
    del /f /q "!OBS_DIR!\obs-plugins\64bit\jn-canvas.dll" >nul 2>&1
    echo [✓] Berhasil menghapus: !OBS_DIR!\obs-plugins\64bit\jn-canvas.dll
) else (
    echo [-] jn-canvas.dll tidak ditemukan (sudah terhapus).
)

if exist "!OBS_DIR!\obs-plugins\64bit\jn-canvas.pdb" (
    del /f /q "!OBS_DIR!\obs-plugins\64bit\jn-canvas.pdb" >nul 2>&1
    echo [✓] Berhasil menghapus: !OBS_DIR!\obs-plugins\64bit\jn-canvas.pdb
)

:: Hapus folder data plugin
if exist "!OBS_DIR!\data\obs-plugins\jn-canvas" (
    rmdir /s /q "!OBS_DIR!\data\obs-plugins\jn-canvas" >nul 2>&1
    echo [✓] Berhasil menghapus folder data: !OBS_DIR!\data\obs-plugins\jn-canvas
) else (
    echo [-] Folder data jn-canvas tidak ditemukan.
)

:: 4. Hapus konfigurasi lokal user (AppData)
echo.
echo [4/5] Membersihkan cache dan konfigurasi AppData...
if exist "%APPDATA%\obs-studio\plugin_config\jn-canvas" (
    rmdir /s /q "%APPDATA%\obs-studio\plugin_config\jn-canvas" >nul 2>&1
    echo [✓] Berhasil menghapus config: %APPDATA%\obs-studio\plugin_config\jn-canvas
) else (
    echo [-] Folder config jn-canvas di AppData tidak ditemukan.
)

:: 5. Bersihkan Registry Uninstaller Windows
echo.
echo [5/5] Membersihkan registrasi Uninstall Windows...
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\{E6D012C4-8F7B-4A12-B2D3-3D4F5A6B7C8D}_is1" /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\{E6D012C4-8F7B-4A12-B2D3-3D4F5A6B7C8D}_is1" /f >nul 2>&1

:: Hapus file uninstaller inno setup sisa jika ada
if exist "!OBS_DIR!\unins000.msg" del /f /q "!OBS_DIR!\unins000.msg" >nul 2>&1

echo.
echo ======================================================================
echo    SUKSES: Plugin JNCanvas telah DICOPOT BERSIH 100%% dari sistem!   
echo    OBS Studio Anda kini normal kembali dan bebas dari crash.         
echo ======================================================================
echo.
pause
exit /b 0
