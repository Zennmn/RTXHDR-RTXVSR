# RTX Video Converter Frontend

React/Vite/Tauri frontend for the RTXHDR-RTXVSR Windows app.

## Browser Development

Start the backend manually:

```powershell
.\build\backend\Debug\vsr_backend.exe --port 49321
```

Then run the frontend:

```powershell
npm install
npm run dev
```

Browser mode can call the backend but cannot open native Windows file/folder dialogs.

## Tauri Development

Prepare a backend sidecar binary after building `vsr_backend.exe`:

```powershell
npm run sidecar:prepare -- -BackendExe ..\..\build\backend\Debug\vsr_backend.exe
```

Then run:

```powershell
npm run tauri:dev
```

Tauri requires Rust and Cargo in `PATH`.

## Windows Packaging

Build the installer:

```powershell
npm run tauri:build
```

Build the installer and a no-install portable package:

```powershell
npm run tauri:build:portable
```

The portable output is written to:

```text
src-tauri/target/release/bundle/portable/
```
