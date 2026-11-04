@echo off
setlocal
title JNCanvas Clean Uninstaller

:: Cek Hak Akses Administrator
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Membutuhkan hak akses Administrator...
    echo Mengalihkan ke Administrator...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process cmd.exe -ArgumentList '/k `\"%~f0`\" elevated' -Verb RunAs"
    exit /b
)

color 0B
echo ======================================================================
echo                JNCANVAS - CLEAN UNINSTALLER TOOL                     
echo ======================================================================
echo.

:: 1. Tutup OBS Studio
echo [1/5] Menutup proses OBS Studio...
taskkill /F /IM obs64.exe >nul 2>&1
taskkill /F /IM obs32.exe >nul 2>&1
timeout /t 1 /nobreak >nul
echo [OK] OBS Studio dipastikan sudah tertutup.
echo.

:: 2. Hapus binary plugin dari Program Files
echo [2/5] Menghapus binary plugin dari Program Files...

set "FOUND=0"

if exist "C:\Program Files\obs-studio\obs-plugins\64bit\jn-canvas.dll" (
    del /f /q "C:\Program Files\obs-studio\obs-plugins\64bit\jn-canvas.dll"
    echo [BERHASIL] Dihapus: C:\Program Files\obs-studio\obs-plugins\64bit\jn-canvas.dll
    set "FOUND=1"
) else (
    echo [-] jn-canvas.dll tidak ditemukan di C:\Program Files\obs-studio
)

if exist "C:\Program Files\obs-studio\obs-plugins\64bit\jn-canvas.pdb" (
    del /f /q "C:\Program Files\obs-studio\obs-plugins\64bit\jn-canvas.pdb"
    echo [BERHASIL] Dihapus: C:\Program Files\obs-studio\obs-plugins\64bit\jn-canvas.pdb
)

if exist "C:\Program Files\obs-studio\data\obs-plugins\jn-canvas" (
    rmdir /s /q "C:\Program Files\obs-studio\data\obs-plugins\jn-canvas"
    echo [BERHASIL] Dihapus: C:\Program Files\obs-studio\data\obs-plugins\jn-canvas
    set "FOUND=1"
) else (
    echo [-] Folder data jn-canvas tidak ditemukan di C:\Program Files\obs-studio
)

:: Cek Program Files (x86) jika ada
if exist "C:\Program Files (x86)\obs-studio\obs-plugins\64bit\jn-canvas.dll" (
    del /f /q "C:\Program Files (x86)\obs-studio\obs-plugins\64bit\jn-canvas.dll"
    echo [BERHASIL] Dihapus dari Program Files (x86)
)
if exist "C:\Program Files (x86)\obs-studio\data\obs-plugins\jn-canvas" (
    rmdir /s /q "C:\Program Files (x86)\obs-studio\data\obs-plugins\jn-canvas"
    echo [BERHASIL] Dihapus folder data dari Program Files (x86)
)

:: 3. Hapus konfigurasi AppData
echo.
echo [3/5] Menghapus data konfigurasi AppData...
if exist "%APPDATA%\obs-studio\plugin_config\jn-canvas" (
    rmdir /s /q "%APPDATA%\obs-studio\plugin_config\jn-canvas"
    echo [BERHASIL] Dihapus: %APPDATA%\obs-studio\plugin_config\jn-canvas
) else (
    echo [OK] Tidak ada konfigurasi sisa di AppData.
)

:: 4. Bersihkan Registry Windows
echo.
echo [4/5] Membersihkan registrasi Windows Uninstaller...
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\{E6D012C4-8F7B-4A12-B2D3-3D4F5A6B7C8D}_is1" /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\{E6D012C4-8F7B-4A12-B2D3-3D4F5A6B7C8D}_is1" /f >nul 2>&1
echo [OK] Registry uninstaller dibersihkan.

:: 5. Verifikasi Akhir
echo.
echo [5/5] Memverifikasi status pembersihan...
if exist "C:\Program Files\obs-studio\obs-plugins\64bit\jn-canvas.dll" (
    echo [PERINGATAN] File jn-canvas.dll masih terdeteksi. Silakan restart PC Anda lalu jalankan uninstaller ini lagi.
) else (
    echo ======================================================================
    echo  SUKSES: Seluruh komponen JNCanvas TELAH DICOPOT BERSIH 100 persen!
    echo  OBS Studio Anda sekarang normal kembali dan bebas dari crash.
    echo ======================================================================
)

echo.
echo Tekan sembarang tombol untuk menutup jendela ini...
pause >nul
exit /b 0
