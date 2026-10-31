# JNCanvas 📱

> **Secondary / Vertical Canvas Plugin for OBS Studio**
> Ringan, Cepat, Bebas Biaya, dan 100% Open Source.

---

## ✨ Fitur Utama

- 📱 **Secondary Canvas Viewport**: Render canvas kedua (9:16 Vertical, 1:1 Square, Custom WxH) dengan akselerasi GPU DirectX/OpenGL.
- 🗂 **Tabbed Control Dock (`Canvas 2`)**:
  - Tab `[Scenes]`: Kelola daftar scene khusus kanvas vertikal.
  - Tab `[Sources]`: Kelola source dengan tombol visibilitas (👁) dan gembok (🔒).
  - Header interaktif: Nama source terpilih + tombol `[⚙ Properties]` & `[🎨 Filters]`.
- 🔁 **Clone to Canvas (Zero GPU Overhead)**:
  - Klik kanan source di Canvas 2 ➔ `Clone to Canvas 1` ➔ Pilih Scene tujuan di OBS Utama.
  - Klik kanan source di Canvas Utama ➔ `Clone to Canvas 2` ➔ Pilih Scene tujuan di Canvas 2.
- ⏺ **Dedicated Recording**: Rekam video khusus kanvas kedua ke format MP4 / MKV.
- ⚡ **Backtrack (Klip Instan Replay Buffer)**: Buffer melingkar di RAM untuk menyimpan klip 30 detik terakhir secara instan.
- 🔗 **Inter-Plugin C-API**: Menyediakan API native yang dapat dibaca langsung oleh **`JNMultiStream`**.

---

## 🛠 Panduan Build (Windows x64 Tanpa Visual Studio Lokal)

Project ini sudah dilengkapi **GitHub Actions CI/CD**:
1. Buat repository baru di GitHub akun abang (misal `JNCanvas`).
2. Push seluruh isi folder ini ke GitHub:
   ```bash
   cd /home/zeinzrtra/obs/JNCanvas
   git init
   git add .
   git commit -m "feat: initial commit JNCanvas v1.0.0"
   git branch -M main
   git remote add origin https://github.com/<username>/JNCanvas.git
   git push -u origin main
   ```
3. Buka tab **Actions** di repositori GitHub abang.
4. Server GitHub (Windows VM) akan otomatis meng-compile kodenya.
5. Download file artifact **`jn-canvas-windows-x64.zip`** yang sudah jadi!

---

## 💻 Cara Install di OBS Studio (Windows)

1. Unduh dan ekstrak file `jn-canvas-windows-x64.zip`.
2. Salin isi folder `obs-plugins/64bit/jn-canvas.dll` ke folder instalasi OBS abang:
   `C:\Program Files\obs-studio\obs-plugins\64bit\`
3. Buka OBS Studio. Dock **Canvas 2 - Preview** dan **Canvas 2** akan otomatis muncul di menu **Docks**!
