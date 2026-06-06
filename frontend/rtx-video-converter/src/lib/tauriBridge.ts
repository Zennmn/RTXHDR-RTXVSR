import type { HealthResponse } from '../api/types';

type SidecarChild = {
  kill: () => Promise<void>;
};

let backendChild: SidecarChild | null = null;
let backendSessionId: string | null = null;

export function isTauriRuntime(): boolean {
  return typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
}

export function createAppSessionId(): string {
  return crypto.randomUUID();
}

export function healthMatchesSession(health: HealthResponse, appSessionId: string): boolean {
  return health.appSessionId === appSessionId;
}

export function currentBackendSessionId(): string | null {
  return backendSessionId;
}

export async function startBackendSidecar(port = 49321): Promise<{ runtime: 'tauri' | 'browser'; started: boolean }> {
  if (!isTauriRuntime()) {
    return { runtime: 'browser', started: false };
  }
  if (backendChild !== null) {
    return { runtime: 'tauri', started: true };
  }

  const appSessionId = createAppSessionId();
  backendSessionId = appSessionId;
  try {
    const { Command } = await import('@tauri-apps/plugin-shell');
    const command = Command.sidecar('binaries/vsr_backend', ['--port', String(port), '--app-session-id', appSessionId]);
    backendChild = (await command.spawn()) as SidecarChild;
    return { runtime: 'tauri', started: true };
  } catch (error) {
    backendChild = null;
    backendSessionId = null;
    throw error;
  }
}

export async function stopBackendSidecar(): Promise<void> {
  if (backendChild === null) {
    return;
  }

  const child = backendChild;
  const appSessionId = backendSessionId;
  backendChild = null;
  backendSessionId = null;
  try {
    if (appSessionId !== null) {
      const { backendClient } = await import('../api/backendClient');
      await backendClient.shutdownAppBackend(appSessionId);
    }
  } finally {
    try {
      await child.kill();
    } catch {
      // The app-owned shutdown route may already have ended the sidecar.
    }
  }
}

export async function pickInputVideo(): Promise<string | null> {
  if (!isTauriRuntime()) {
    return null;
  }

  const { open } = await import('@tauri-apps/plugin-dialog');
  const selected = await open({
    multiple: false,
    directory: false,
    filters: [
      {
        name: 'Video',
        extensions: ['mp4', 'mkv', 'mov', 'avi', 'webm'],
      },
    ],
  });
  return typeof selected === 'string' ? selected : null;
}

export async function pickOutputDirectory(): Promise<string | null> {
  if (!isTauriRuntime()) {
    return null;
  }

  const { open } = await import('@tauri-apps/plugin-dialog');
  const selected = await open({
    multiple: false,
    directory: true,
  });
  return typeof selected === 'string' ? selected : null;
}
