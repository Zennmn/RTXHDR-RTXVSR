# Frontend

Windows UI workspace.

The current React/Vite UI project lives in `rtx-video-converter/`.

Keep the UI as a separate project in this folder and talk to the backend through the local HTTP API documented in `../FRONTEND_INTEGRATION.md`.

Recommended first UI boundary:

- Start `vsr_backend.exe --port 49321` as a local sidecar process.
- Poll `GET /api/health` and `GET /api/capabilities` before enabling conversion.
- Create, poll, and cancel jobs through `/api/jobs`.
- Do not include backend C++ headers directly from the frontend.
