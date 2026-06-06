import { useEffect, useState } from 'react';
import { backendClient } from '../api/backendClient';
import type { CapabilityResponse, HealthResponse } from '../api/types';
import { startBackendSidecar, stopBackendSidecar } from '../lib/tauriBridge';

export type BackendStatus = 'starting' | 'ready' | 'offline' | 'degraded';

export interface BackendStatusState {
  status: BackendStatus;
  runtime: 'tauri' | 'browser';
  health: HealthResponse | null;
  capabilities: CapabilityResponse | null;
  message: string;
  refresh: () => Promise<void>;
}

async function waitForHealth(): Promise<HealthResponse> {
  let lastError: unknown;
  for (let attempt = 0; attempt < 20; ++attempt) {
    try {
      return await backendClient.getHealth();
    } catch (error) {
      lastError = error;
      await new Promise((resolve) => window.setTimeout(resolve, 250));
    }
  }
  throw lastError;
}

async function loadBackend(): Promise<{
  runtime: 'tauri' | 'browser';
  health: HealthResponse;
  capabilities: CapabilityResponse;
}> {
  let lastError: unknown;
  for (let attempt = 0; attempt < 3; ++attempt) {
    let sidecar: Awaited<ReturnType<typeof startBackendSidecar>> | null = null;
    try {
      sidecar = await startBackendSidecar();
      const health = await waitForHealth();
      const capabilities = await backendClient.getCapabilities();
      return { runtime: sidecar.runtime, health, capabilities };
    } catch (error) {
      lastError = error;
      if (sidecar?.runtime !== 'tauri') {
        break;
      }
      await stopBackendSidecar().catch(() => undefined);
    }
  }
  throw lastError;
}

export function useBackendStatus(): BackendStatusState {
  const [status, setStatus] = useState<BackendStatus>('starting');
  const [runtime, setRuntime] = useState<'tauri' | 'browser'>('browser');
  const [health, setHealth] = useState<HealthResponse | null>(null);
  const [capabilities, setCapabilities] = useState<CapabilityResponse | null>(null);
  const [message, setMessage] = useState('正在启动后端...');

  const refresh = async () => {
    setStatus('starting');
    setMessage('正在连接后端...');
    try {
      const { runtime: nextRuntime, health: nextHealth, capabilities: nextCapabilities } = await loadBackend();
      setRuntime(nextRuntime);
      setHealth(nextHealth);
      setCapabilities(nextCapabilities);
      const degraded = nextCapabilities.messages.length > 0;
      setStatus(degraded ? 'degraded' : 'ready');
      setMessage(degraded ? '后端已连接，但部分能力不可用。' : '后端已就绪。');
    } catch (error) {
      setHealth(null);
      setCapabilities(null);
      setStatus('offline');
      setMessage(error instanceof Error ? error.message : '后端未连接。');
    }
  };

  useEffect(() => {
    void refresh();
  }, []);

  return { status, runtime, health, capabilities, message, refresh };
}
