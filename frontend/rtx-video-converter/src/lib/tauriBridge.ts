import { backendClient } from '../api/backendClient';

type SidecarChild = {
  kill: () => Promise<void>;
};

let backendChild: SidecarChild | null = null;
let backendAuthToken = '';

function getOrCreateBackendAuthToken(): string {
  if (backendAuthToken.length === 0) {
    backendAuthToken = crypto.randomUUID();
  }
  return backendAuthToken;
}

export function isTauriRuntime(): boolean {
  return typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
}

export async function startBackendSidecar(port = 49321): Promise<{ runtime: 'tauri' | 'browser'; started: boolean }> {
  if (!isTauriRuntime()) {
    return { runtime: 'browser', started: false };
  }
  if (backendChild !== null) {
    return { runtime: 'tauri', started: true };
  }

  const token = getOrCreateBackendAuthToken();
  backendClient.setAuthToken(token);
  const { Command } = await import('@tauri-apps/plugin-shell');
  const command = Command.sidecar('binaries/vsr_backend', ['--port', String(port), '--auth-token', token]);
  backendChild = (await command.spawn()) as SidecarChild;
  return { runtime: 'tauri', started: true };
}

export async function stopBackendSidecar(): Promise<void> {
  if (backendChild === null) {
    return;
  }

  const child = backendChild;
  backendChild = null;
  await child.kill();
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
