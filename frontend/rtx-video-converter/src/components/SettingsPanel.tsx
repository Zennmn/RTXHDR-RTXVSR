import type { CapabilityResponse, ProcessingMode, UiCodec } from '../api/types';
import type { ConversionSettings } from '../types';
import { WinSegmentedControl, WinSlider, WinSwitch } from './WinUI';

export function SettingsPanel({
  settings,
  capabilities,
  disabled,
  onChange,
}: {
  settings: ConversionSettings;
  capabilities: CapabilityResponse | null;
  disabled: boolean;
  onChange: (settings: ConversionSettings) => void;
}) {
  const hdrDisabled = disabled || capabilities?.truehdrAvailable === false;
  const vsrDisabled = disabled || capabilities?.vsrAvailable === false;
  const codecDisabled = disabled || settings.mode !== 'vsr';
  const update = (patch: Partial<ConversionSettings>) => onChange({ ...settings, ...patch });

  return (
    <section className="flex grow flex-col overflow-y-auto bg-[#f3f3f3]">
      <div className="w-full max-w-4xl space-y-6 p-6">
        <WinSegmentedControl
          options={[
            { label: 'VSR 超分', value: 'vsr', disabled: vsrDisabled },
            { label: 'HDR 映射', value: 'hdr', disabled: hdrDisabled },
            { label: 'VSR + HDR', value: 'both', disabled: vsrDisabled || hdrDisabled },
          ]}
          value={settings.mode}
          onChange={(value) => update({ mode: value as ProcessingMode, codec: value === 'vsr' ? settings.codec : 'hevc' })}
        />

        <div className="grid w-full grid-cols-1 gap-6 md:grid-cols-2">
          <div
            className={`rounded-xl border border-[#e5e5e5] bg-white p-5 shadow-sm transition-opacity duration-300 ${
              settings.mode === 'hdr' ? 'pointer-events-none opacity-40' : 'opacity-100'
            }`}
          >
            <h3 className="mb-4 text-sm font-semibold text-[#1a1a1a]">VSR 参数</h3>
            <div className="space-y-5">
              <WinSlider
                label="提升倍率"
                value={settings.vsrScale}
                min={1}
                max={4}
                step={1}
                onChange={(vsrScale) => update({ vsrScale })}
                formatValue={(value) => `${value}.0x`}
              />

              <div>
                <div className="mb-2 flex justify-between text-xs">
                  <span className="text-[#1a1a1a]">处理质量</span>
                  <span className="font-mono text-[#1a1a1a]">质量 {settings.vsrQuality}</span>
                </div>
                <div className="flex gap-1">
                  {[1, 2, 3, 4].map((quality) => (
                    <button
                      key={quality}
                      onClick={() => update({ vsrQuality: quality })}
                      className={`flex-1 rounded border py-1.5 text-xs transition-colors ${
                        settings.vsrQuality === quality
                          ? 'border-[#0067c0] bg-blue-50 font-medium text-[#0067c0]'
                          : 'border-[#e5e5e5] bg-[#f9f9f9] text-[#1a1a1a] hover:bg-gray-100'
                      }`}
                    >
                      {quality}
                    </button>
                  ))}
                </div>
              </div>
            </div>
          </div>

          <div
            className={`rounded-xl border border-[#e5e5e5] bg-white p-5 shadow-sm transition-opacity duration-300 ${
              settings.mode === 'vsr' ? 'pointer-events-none opacity-40' : 'opacity-100'
            }`}
          >
            <h3 className="mb-4 text-sm font-semibold text-[#1a1a1a]">HDR 参数</h3>
            <div className="grid grid-cols-2 gap-4">
              <div>
                <label className="mb-1 block text-[10px] text-[#7a7a7a]">对比度</label>
                <input
                  type="number"
                  value={settings.hdrContrast}
                  onChange={(event) => update({ hdrContrast: Number(event.target.value) })}
                  className="w-full rounded border border-[#d1d1d1] p-1.5 text-xs"
                />
              </div>
              <div>
                <label className="mb-1 block text-[10px] text-[#7a7a7a]">饱和度</label>
                <input
                  type="number"
                  value={settings.hdrSaturation}
                  onChange={(event) => update({ hdrSaturation: Number(event.target.value) })}
                  className="w-full rounded border border-[#d1d1d1] p-1.5 text-xs"
                />
              </div>
              <div>
                <label className="mb-1 block text-[10px] text-[#7a7a7a]">中灰度</label>
                <input
                  type="number"
                  value={settings.hdrMiddleGray}
                  onChange={(event) => update({ hdrMiddleGray: Number(event.target.value) })}
                  className="w-full rounded border border-[#d1d1d1] p-1.5 text-xs"
                />
              </div>
              <div>
                <label className="mb-1 block text-[10px] text-[#7a7a7a]">最大亮度 (Nits)</label>
                <input
                  type="number"
                  value={settings.hdrMaxLuminance}
                  onChange={(event) => update({ hdrMaxLuminance: Number(event.target.value) })}
                  className="w-full rounded border border-[#d1d1d1] p-1.5 text-xs"
                />
              </div>
            </div>
          </div>
        </div>

        <div className="rounded-xl border border-[#e5e5e5] bg-white p-5 shadow-sm">
          <h3 className="mb-4 text-sm font-semibold text-[#1a1a1a]">编码器与轨道</h3>
          <div className="grid grid-cols-1 gap-6 text-xs md:grid-cols-3">
            <div>
              <label className="mb-1 block text-[#7a7a7a]">视频编码</label>
              <select
                value={settings.codec}
                onChange={(event) => update({ codec: event.target.value as UiCodec })}
                disabled={codecDisabled}
                className="w-full rounded border border-[#d1d1d1] bg-[#f9f9f9] p-2 text-[#1a1a1a] focus:border-[#0067c0] focus:outline-none"
              >
                <option value="h264" disabled={capabilities?.nvencH264Available === false}>
                  NVENC H.264
                </option>
                <option value="hevc" disabled={capabilities?.nvencHevcMain10Available === false}>
                  NVENC HEVC Main10
                </option>
              </select>
            </div>
            <div>
              <label className="mb-1 block text-[#7a7a7a]">封装容器</label>
              <div className="cursor-not-allowed rounded border border-[#d1d1d1] bg-[#f9f9f9] p-2 text-[#1a1a1a] opacity-80">
                MPEG-4 (.mp4)
              </div>
            </div>
            <div>
              <label className="mb-1 block text-[#7a7a7a]">音频/字幕</label>
              <div className="mt-2 flex gap-4">
                <WinSwitch label="音频" checked={settings.keepAudio} onChange={(keepAudio) => update({ keepAudio })} />
                <WinSwitch label="字幕" checked={settings.keepSubtitles} onChange={(keepSubtitles) => update({ keepSubtitles })} />
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
