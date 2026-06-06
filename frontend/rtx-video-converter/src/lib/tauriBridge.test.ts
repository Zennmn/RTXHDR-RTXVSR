import { describe, expect, it, vi } from 'vitest';
import { createAppSessionId, currentBackendSessionId, healthMatchesSession } from './tauriBridge';

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
});
