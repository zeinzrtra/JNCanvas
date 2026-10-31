# Product Requirements Document (PRD) - JNCanvas

## 1. Ringkasan Eksekutif (Executive Summary)
**JNCanvas** adalah plugin open-source untuk OBS Studio yang menyediakan **Secondary Render Canvas** independen (seperti kanvas vertikal 9:16 untuk TikTok, Instagram Reels, YouTube Shorts, atau format kustom lainnya) di samping kanvas horizontal utama (16:9). 

Plugin ini dirancang khusus untuk streamer dan content creator modern yang ingin memproduksi tayangan multi-format dalam satu sesi OBS tanpa membebani sistem secara berlebih.

---

## 2. Prinsip Utama Desain (Core Principles)

| Prinsip | Implementasi Teknis di JNCanvas |
| :--- | :--- |
| **⚡ Efisiensi** | **Zero Duplicate Capture**: Memanfaatkan *Source Cloning* dari scene utama OBS tanpa merender ulang source GPU (kamera, game capture, display capture) dua kali. Render loop hanya berjalan saat kanvas aktif/terlihat. |
| **🪶 Ringan** | Dibangun murni menggunakan **C++20** dan **Qt 6** native (`libobs` API). Tanpa runtime tambahan (tanpa Electron, Python, atau framework berat), jejak memori sangat kecil (<30 MB RAM) dan binary berukuran ringkas. |
| **🎯 Fungsional** | Menyediakan fitur esensial yang langsung terpakai: Dock Preview interaktif, Scene terpisah untuk kanvas kedua, Backtrack (Replay Buffer vertikal) untuk klip instan, dan perekaman mandiri. |
| **🧩 Fleksibel** | Tidak terkunci pada rasio 9:16 saja. Mendukung preset: **9:16 (Vertikal)**, **1:1 (Square)**, **4:5 (Portrait Feed)**, dan **Custom Resolution (WxH)** dengan rotasi bebas. |

---

## 3. Arsitektur & Hubungan Sistem

```
┌─────────────────────────────────────────────────────────────┐
│                        OBS STUDIO                           │
│                                                             │
│  ┌─────────────────────────┐   ┌─────────────────────────┐  │
│  │   Main Canvas (16:9)    │   │   JNCanvas (Secondary)  │  │
│  │   (libobs default view) │   │     (obs_view_create)   │  │
│  └───────────┬─────────────┘   └───────────┬─────────────┘  │
│              │                             │                │
│              ▼                             ▼                │
│     Main Rendering Engine        JNCanvas Dedicated Scene   │
│     (DirectX 11 / OpenGL)        (Cloned or Custom Sources) │
│                                            │                │
│                                ┌───────────┴─────────────┐  │
│                                ▼                         ▼  │
│                       Qt 6 Preview Dock       Export API Pipeline│
│                       (obs_display_create)    (Direct to JNMulti)│
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Spesifikasi Detail UI & Fitur Utama

### 4.1. Tampilan UI Dock: Model Tabbed (Sesuai Desain Acuan)
UI JNCanvas dirancang mengadopsi standar layout native OBS yang bersih dan efisien:
1. **Dock Preview**:
   - Menampilkan render viewport kanvas kedua secara realtime dengan akselerasi GPU.
   - Posisi fleksibel: Dapat disematkan di **atas, kanan, kiri**, atau menjadi floating window di samping canvas utama OBS.
2. **Dock Kontrol (`Canvas 2`)**:
   - **Top Header Bar**: 
     - Menampilkan nama source aktif (misal: *Webcam Gaming* atau *No source selected*).
     - Tombol cepat: `[⚙ Properties]` dan `[🎨 Filters]`.
   - **Daftar Konten (Body)**:
     - Menampilkan daftar elemen dengan ikon tipe source, nama source, tombol **Toggle Visibility (Mata)**, dan tombol **Lock (Gembok)**.
     - Toolbar aksi di bawah: `[+]` (Tambah), `[🗑]` (Hapus), `[⚙]` (Pengaturan), `[▲]` (Pindah ke atas), `[▼]` (Pindah ke bawah).
   - **Bottom Navigation Tabs**:
     - **Tab `[Scenes]`**: Mengelola daftar scene khusus kanvas kedua.
     - **Tab `[Sources]`**: Mengelola layer/source di scene yang sedang aktif.

---

### 4.2. Fitur "Clone to Canvas" via Context Menu (Klik Kanan)
Menu klik kanan pada source dibuat **100% identik dengan menu klik kanan bawaan OBS** (Transform, Order, Filters, Properties), dengan **1 menu tambahan khusus**:

- **Jika klik kanan source di Canvas 2**:
  - Terdapat menu: **`Clone to Canvas 1` ➔ [Submenu: Daftar Scene Canvas 1]**
  - Mengarahkan mouse ke menu ini akan menampilkan semua scene yang ada di Canvas utama OBS. Saat salah satu scene diklik, source tersebut langsung terduplikasi ke scene Canvas 1 tanpa membebani GPU (*zero duplicate capture*).
- **Jika klik kanan source di Canvas 1 (Canvas Utama OBS)**:
  - Terdapat menu: **`Clone to Canvas 2` ➔ [Submenu: Daftar Scene Canvas 2]**
  - Langsung mengklon source dari canvas utama ke scene kanvas kedua yang dipilih.

---

### 4.3. Fitur Recording & Replay Buffer (Standar OBS & Super Ringan)
- **Dedicated Record**:
  - Format rekaman mengikuti standar OBS (MP4 / MKV).
  - Menggunakan encoder hardware yang sama dengan OBS (NVENC / QuickSync / AMF / x264).
- **Replay Buffer (Backtrack)**:
  - Mekanisme identik dengan Replay Buffer bawaan OBS.
  - Menggunakan circular RAM buffer ringan (RAM hanya terpakai sebesar durasi buffer yang aktif, misal 30 detik).
  - Tombol simpan klip instan dengan hotkey support.

---

### 4.4. Jalur Ekspor Native (C-API untuk JNMultiStream)
- JNCanvas mengekspor fungsi internal C:
  ```cpp
  obs_source_t* jn_canvas_get_output_source();
  obs_view_t*   jn_canvas_get_view();
  ```
- Memungkinkan JNMultiStream membaca data frame buffer secara instan tanpa perantara virtual cam.

---

## 5. Antarmuka Pengguna (UI/UX)
- **Dock Minimalis**: Menyesuaikan tema bawaan OBS Studio (Dark/Yami/Acuta).
- **Control Bar Ringkas**:
  - Tombol Record Vertikal (REC).
  - Tombol Backtrack (SAVE CLIP).
  - Tombol Settings (Resolusi, FPS, Scene Vertikal).
  - Indikator Resolusi aktif (contoh: `1080x1920 @ 60fps`).

---

## 6. Spesifikasi Teknis & Non-Fungsional
- **Bahasa**: C++20.
- **Framework UI**: Qt 6 (Widgets).
- **Core Library**: `libobs`, `obs-frontend-api`.
- **Target OS**: Windows 10/11 x64 (Prioritas Pertama), macOS & Linux (Fase Lanjutan).
- **Overhead CPU**: Maksimal < 1.5% tambahan CPU pada sistem Intel Core i5/AMD Ryzen 5 ke atas.
- **Jejak Memori (RAM)**: < 35 MB saat idle, buffer backtrack sesuai durasi klip yang dipilih.
- **Stabilitas**: Thread-safe render loop, zero crash pada saat switch scene atau resize dock.

---

## 7. Roadmap Rilis JNCanvas
- **Milestone 1**: Setup project skeleton C++, CMake, dan GitHub Actions auto-compile Windows.
- **Milestone 2**: Inisialisasi `obs_view_t` dan integrasi preview ke Qt 6 QDockWidget.
- **Milestone 3**: Implementasi Scene Manager & Source Cloner khusus JNCanvas.
- **Milestone 4**: Implementasi Dedicated Record & Backtrack Replay Buffer.
- **Milestone 5**: Implementasi Export Interface untuk JNMultiStream & Rilis Alpha publik.

