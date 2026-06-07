import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

const shellMocks = vi.hoisted(() => ({
  kill: vi.fn(async () => undefined),
  spawn: vi.fn(async () => ({ kill: shellMocks.kill })),
  sidecar: vi.fn(() => ({ spawn: shellMocks.spawn })),
}));

const backendClientMocks = vi.hoisted(() => ({
  shutdownAppBackend: vi.fn(async () => ({ accepted: true })),
}));

vi.mock('@tauri-apps/plugin-shell', () => ({
  Command: {
    sidecar: shellMocks.sidecar,
  },
}));

vi.mock('../api/backendClient', () => ({
  backendClient: {
    shutdownAppBackend: backendClientMocks.shutdownAppBackend,
  },
}));

import { createAppSessionId, currentBackendSessionId, healthMatchesSession, startBackendSidecar, stopBackendSidecar, tauriDropPath } from './tauriBridge';

const originalWindow = globalThis.window;

beforeEach(() => {
  Object.defineProperty(globalThis, 'window', {
    configurable: true,
    value: { __TAURI_INTERNALS__: {} },
  });
});

afterEach(async () => {
  await stopBackendSidecar().catch(() => undefined);
  Object.defineProperty(globalThis, 'window', {
    configurable: true,
    value: originalWindow,
  });
  vi.restoreAllMocks();
});

describe('tauriBridge session helpers', () => {
  it('creates app session ids with crypto.randomUUID', () => {
    const sessionId = '123e4567-e89b-12d3-a456-426614174000';
    const randomUUID = vi.spyOn(crypto, 'randomUUID').mockReturnValue(sessionId);

    expect(createAppSessionId()).toBe(sessionId);
    expect(randomUUID).toHaveBeenCalledOnce();
  });

  it('matches health to the current app session id', () => {
    expect(healthMatchesSession({ version: '0.1.0', ready: true, appSessionId: 'session-123' }, 'session-123')).toBe(true);
    expect(healthMatchesSession({ version: '0.1.0', ready: true, appSessionId: 'other-session' }, 'session-123')).toBe(false);
    expect(healthMatchesSession({ version: '0.1.0', ready: true }, 'session-123')).toBe(false);
  });

  it('has no backend session before a Tauri sidecar starts', () => {
    expect(currentBackendSessionId()).toBeNull();
  });

  it('clears the app session when sidecar shutdown is rejected', async () => {
    const sessionId = '123e4567-e89b-12d3-a456-426614174000';
    vi.spyOn(crypto, 'randomUUID').mockReturnValue(sessionId);
    backendClientMocks.shutdownAppBackend.mockRejectedValueOnce(new Error('wrong backend'));

    await startBackendSidecar();
    await expect(stopBackendSidecar()).rejects.toThrow('wrong backend');

    expect(currentBackendSessionId()).toBeNull();
    expect(shellMocks.kill).toHaveBeenCalledOnce();
  });

  it('extracts the first real file path from Tauri drop events', () => {
    expect(tauriDropPath({ type: 'drop', paths: ['C:\\Videos\\clip.mp4'], position: { x: 12, y: 34 } })).toBe('C:\\Videos\\clip.mp4');
  });

  it('ignores non-drop Tauri drag events and empty drops', () => {
    expect(tauriDropPath({ type: 'enter', paths: ['C:\\Videos\\clip.mp4'], position: { x: 12, y: 34 } })).toBeNull();
    expect(tauriDropPath({ type: 'drop', paths: [], position: { x: 12, y: 34 } })).toBeNull();
  });
});
