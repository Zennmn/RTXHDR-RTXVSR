import { describe, expect, it } from 'vitest';
import { evenScaledDimension, formatResolution, parseResolution, scaleResolution } from './resolution';

describe('parseResolution', () => {
  it('parses backend probe resolution text', () => {
    expect(parseResolution('2560x1380')).toEqual({ width: 2560, height: 1380 });
  });

  it('returns null for missing metadata', () => {
    expect(parseResolution('')).toBeNull();
    expect(parseResolution(undefined)).toBeNull();
  });
});

describe('scaleResolution', () => {
  it('supports non-integer VSR scale values', () => {
    expect(formatResolution(scaleResolution({ width: 2560, height: 1380 }, 2.3))).toBe('5888x3174');
  });

  it('matches the backend even-dimension rounding rule', () => {
    expect(evenScaledDimension(101, 1)).toBe(102);
  });
});
