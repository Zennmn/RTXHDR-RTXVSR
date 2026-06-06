import { afterEach, describe, expect, it, vi } from 'vitest';
import { ApiError, BackendClient } from './backendClient';

const originalFetch = globalThis.fetch;

afterEach(() => {
  globalThis.fetch = originalFetch;
  vi.restoreAllMocks();
});

describe('BackendClient', () => {
  it('loads health', async () => {
    globalThis.fetch = vi.fn(async () => new Response(JSON.stringify({ version: '0.1.0', ready: true }))) as typeof fetch;

    const client = new BackendClient('http://127.0.0.1:49321');
    await expect(client.getHealth()).resolves.toEqual({ version: '0.1.0', ready: true });
  });

  it('requests app-owned backend shutdown', async () => {
    const fetchMock = vi.fn(async () => new Response(JSON.stringify({ accepted: true }))) as unknown as typeof fetch;
    globalThis.fetch = fetchMock;

    const client = new BackendClient('http://127.0.0.1:49321');
    await expect(client.shutdownAppBackend('session-123')).resolves.toEqual({ accepted: true });

    expect(fetchMock).toHaveBeenCalledWith('http://127.0.0.1:49321/api/app/shutdown', {
      method: 'POST',
      body: JSON.stringify({ appSessionId: 'session-123' }),
      headers: {
        'Content-Type': 'application/json',
      },
    });
  });

  it('parses backend error envelopes', async () => {
    globalThis.fetch = vi.fn(async () =>
      new Response(JSON.stringify({ error: { code: 'invalid_json', message: 'Bad JSON', details: 'line 1' } }), {
        status: 400,
      }),
    ) as typeof fetch;

    const client = new BackendClient('http://127.0.0.1:49321');

    await expect(client.getJob('missing')).rejects.toMatchObject({
      code: 'invalid_json',
      message: 'Bad JSON',
      details: 'line 1',
      status: 400,
    });
  });

  it('turns network failures into ApiError', async () => {
    globalThis.fetch = vi.fn(async () => {
      throw new TypeError('fetch failed');
    }) as typeof fetch;

    const client = new BackendClient('http://127.0.0.1:49321');

    await expect(client.getHealth()).rejects.toBeInstanceOf(ApiError);
    await expect(client.getHealth()).rejects.toMatchObject({ code: 'network_error' });
  });
});
