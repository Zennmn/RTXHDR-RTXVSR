import { backendClient } from '../api/backendClient';

type SidecarChild = {
  kill: () => Promise<void>;
};

let backendChild: SidecarChild | null = null;
let backendAuthToken = '';

const defaultBrowserBackendPort = 49321;
const sidecarPortStart = 49321;
const sidecarPortCount = 1000;

function getOrCreateBackendAuthToken(): string {
  if (backendAuthToken.length === 0) {
    backendAuthToken = crypto.randomUUID();
  }
  return backendAuthToken;
}

function backendBaseUrl(port: number): string {
  return `http://127.0.0.1:${port}`;
}

function chooseBackendPort(): number {
  const values = new Uint32Array(1);
  crypto.getRandomValues(values);
  return sidecarPortStart + (values[0] % sidecarPortCount);
}

export function isTauriRuntime(): boolean {
  return typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
}

export async function startBackendSidecar(port = chooseBackendPort()): Promise<{ runtime: 'tauri' | 'browser'; started: boolean }> {
  if (!isTauriRuntime()) {
    backendClient.setBaseUrl(backendBaseUrl(defaultBrowserBackendPort));
    return { runtime: 'browser', started: false };
  }
  if (backendChild !== null) {
    return { runtime: 'tauri', started: true };
  }

  const token = getOrCreateBackendAuthToken();
  backendClient.setBaseUrl(backendBaseUrl(port));
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
