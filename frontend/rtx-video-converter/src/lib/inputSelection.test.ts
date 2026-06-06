import { describe, expect, it } from 'vitest';
import { nextOutputDirectoryForInput } from './inputSelection';

describe('nextOutputDirectoryForInput', () => {
  it('uses the input file directory when no output directory has been selected', () => {
    expect(nextOutputDirectoryForInput('C:\\Videos\\clip.mp4', '')).toBe('C:\\Videos');
  });

  it('preserves a user-selected output directory', () => {
    expect(nextOutputDirectoryForInput('C:\\Videos\\clip.mp4', 'D:\\Exports')).toBe('D:\\Exports');
  });

  it('handles browser-style slash paths when deriving the input directory', () => {
    expect(nextOutputDirectoryForInput('/home/user/videos/clip.mp4', '')).toBe('\\home\\user\\videos');
  });

  it('keeps whitespace-only output directories as empty and derives from input', () => {
    expect(nextOutputDirectoryForInput('C:\\Videos\\clip.mp4', '   ')).toBe('C:\\Videos');
  });
});
