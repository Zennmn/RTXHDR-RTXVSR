# RTX Video Converter — WinUI 3 frontend

Native Windows 11 frontend for the existing `vsr_backend.exe` sidecar.

## Requirements

- Windows 10 1809 or newer (Windows 11 recommended)
- .NET 8 SDK
- Visual Studio 2022 with Windows application development tools, or the .NET CLI
- The existing C++ backend build output

## Build

```powershell
dotnet build RTXVideoConverter.WinUI.csproj -c Release -p:Platform=x64
```

The project uses the self-contained Windows App SDK configuration. Put `vsr_backend.exe` next to the frontend executable or under its `runtime/` directory for packaged runs.

## Run

```powershell
dotnet run --project RTXVideoConverter.WinUI.csproj -p:Platform=x64
```

For visual QA without a backend binary, set `RTX_UI_DESIGN_PREVIEW=1`. This only supplies ready-state capability data; it does not enable conversion.

## Backend contract

The frontend retains the existing localhost JSON API and app-session ownership contract described in the repository-level `FRONTEND_INTEGRATION.md`.
