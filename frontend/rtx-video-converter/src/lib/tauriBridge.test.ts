import { afterEach, describe, expect, it, vi } from 'vitest';

const spawnMock = vi.fn(async () => ({ kill: vi.fn(async () => undefined) }));
const sidecarMock = vi.fn(() => ({ spawn: spawnMock }));

vi.mock('@tauri-apps/plugin-shell', () => ({
  Command: {
    sidecar: sidecarMock,
  },
}));

const originalFetch = globalThis.fetch;
const originalWindow = globalThis.window;

afterEach(() => {
  globalThis.fetch = originalFetch;
  Object.defineProperty(globalThis, 'window', { configurable: true, value: originalWindow });
  vi.resetModules();
  vi.clearAllMocks();
});

describe('startBackendSidecar', () => {
  it('points the backend client at the selected sidecar port', async () => {
    Object.defineProperty(globalThis, 'window', { configurable: true, value: { __TAURI_INTERNALS__: {} } });
    const fetchMock = vi.fn(async () => new Response(JSON.stringify({ version: '0.1.0', ready: true })));
    globalThis.fetch = fetchMock as typeof fetch;

    const { backendClient } = await import('../api/backendClient');
    const { startBackendSidecar } = await import('./tauriBridge');

    await startBackendSidecar(49431);
    await backendClient.getHealth();

    expect(sidecarMock).toHaveBeenCalledWith(
      'binaries/vsr_backend',
      expect.arrayContaining(['--port', '49431']),
    );
    expect(fetchMock).toHaveBeenCalledWith('http://127.0.0.1:49431/api/health', expect.any(Object));
  });
});
