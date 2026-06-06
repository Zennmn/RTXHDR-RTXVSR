import type { CapabilityResponse } from '../api/types';

export function CapabilityBanner({ capabilities, message }: { capabilities: CapabilityResponse | null; message: string }) {
  const messages = capabilities?.messages ?? [];
  const vsrStatus = capabilities === null ? '检测中' : capabilities.vsrAvailable ? '可用' : '不可用';
  const hdrStatus = capabilities === null ? '检测中' : capabilities.truehdrAvailable ? '可用' : '不可用';

  return (
    <div className="rounded-xl border border-[#e5e5e5] bg-white p-4 shadow-sm">
      <div className="flex items-center justify-between gap-4">
        <div>
          <p className="text-sm font-semibold text-[#1a1a1a]">后端状态</p>
          <p className="text-xs text-[#5a5a5a]">{message}</p>
        </div>
        <div className="text-[11px] text-[#5a5a5a]">
          VSR {vsrStatus} / HDR {hdrStatus}
        </div>
      </div>
      {messages.length > 0 && (
        <ul className="mt-3 space-y-1 text-xs text-[#8a5a00]">
          {messages.map((item) => (
            <li key={item}>{item}</li>
          ))}
        </ul>
      )}
    </div>
  );
}
