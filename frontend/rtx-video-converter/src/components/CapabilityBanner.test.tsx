import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import type { CapabilityResponse } from '../api/types';
import { CapabilityBanner } from './CapabilityBanner';

const allCapabilities: CapabilityResponse = {
  d3d11Available: true,
  nvencH264Available: true,
  nvencHevcMain10Available: true,
  rtxSdkFound: true,
  truehdrAvailable: true,
  vsrAvailable: true,
  messages: [],
};

describe('CapabilityBanner', () => {
  it('shows capabilities as checking before the backend reports them', () => {
    const html = renderToStaticMarkup(<CapabilityBanner capabilities={null} message="正在连接后端..." />);

    expect(html).toContain('VSR 检测中 / HDR 检测中');
    expect(html).not.toContain('VSR 不可用 / HDR 不可用');
  });

  it('shows available capabilities after they are loaded', () => {
    const html = renderToStaticMarkup(<CapabilityBanner capabilities={allCapabilities} message="后端已就绪。" />);

    expect(html).toContain('VSR 可用 / HDR 可用');
  });
});
