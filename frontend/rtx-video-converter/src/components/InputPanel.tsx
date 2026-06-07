import type { MediaProbeResponse } from '../api/types';

export function InputPanel({
  inputPath,
  metadata,
  outputDirectory,
  outputPath,
  onBrowseFile,
  onBrowseOutput,
  onDropPath,
  onOutputDirectoryChange,
}: {
  inputPath: string;
  metadata: MediaProbeResponse | null;
  outputDirectory: string;
  outputPath: string;
  onBrowseFile: () => void;
  onBrowseOutput: () => void;
  onDropPath: (path: string) => void;
  onOutputDirectoryChange: (path: string) => void;
}) {
  return (
    <aside className="hidden w-80 shrink-0 flex-col gap-6 overflow-y-auto border-r border-[#e5e5e5] bg-white p-4 md:flex">
      <section>
        <h2 className="mb-3 text-[11px] font-bold uppercase tracking-wider text-[#7a7a7a]">输入源</h2>
        <div
          className="mb-4 flex h-[120px] cursor-pointer flex-col items-center justify-center rounded-lg border-2 border-dashed border-[#d1d1d1] bg-[#fcfcfc] p-6 text-center transition-colors hover:border-[#aaa]"
          onClick={onBrowseFile}
          onDragOver={(event) => event.preventDefault()}
          onDrop={(event) => {
            event.preventDefault();
            const file = event.dataTransfer.files[0] as File & { path?: string };
            if (file?.name) {
              onDropPath(file.path ?? file.name);
            }
          }}
        >
          <div className="mb-2 text-2xl">📂</div>
          <p className="text-xs text-[#5a5a5a]">
            拖拽视频至此或 <span className="text-[#0067c0] hover:underline">浏览</span>
          </p>
        </div>

        <div className="mb-4">
          <label className="mb-1 block text-xs">输入路径</label>
          <input
            type="text"
            value={inputPath}
            readOnly
            className="w-full cursor-default rounded border border-[#d1d1d1] bg-[#f9f9f9] px-2 py-1.5 text-xs text-[#5a5a5a] shadow-sm focus:outline-none"
            placeholder="拖拽视频或点击浏览选择文件"
            title={inputPath || '拖拽视频或点击浏览选择文件'}
          />
        </div>

        <div className="space-y-2 rounded border border-[#e5e5e5] bg-[#f9f9f9] p-3 text-[11px]">
          <div className="flex justify-between gap-3">
            <span>文件名:</span>
            <span className="truncate font-medium" title={metadata?.name ?? inputPath}>
              {metadata?.name ?? (inputPath || '未选择')}
            </span>
          </div>
          <div className="flex justify-between">
            <span>大小:</span>
            <span className="font-medium">{metadata ? `${(metadata.sizeBytes / 1024 / 1024).toFixed(1)} MB` : '-'}</span>
          </div>
          <div className="flex justify-between">
            <span>分辨率:</span>
            <span className="font-medium">{metadata?.resolution || '-'}</span>
          </div>
          <div className="flex justify-between">
            <span>时长:</span>
            <span className="font-medium">{metadata?.duration || '-'}</span>
          </div>
          <div className="flex justify-between">
            <span>编码:</span>
            <span className="font-medium text-[#0067c0]">{metadata?.codec || '-'}</span>
          </div>
        </div>
      </section>

      <section className="flex grow flex-col gap-4">
        <h2 className="text-[11px] font-bold uppercase tracking-wider text-[#7a7a7a]">输出设置</h2>
        <div>
          <label className="mb-1 block text-xs">输出目录</label>
          <div className="flex gap-1">
            <input
              type="text"
              value={outputDirectory}
              onChange={(event) => onOutputDirectoryChange(event.target.value)}
              className="grow rounded border border-[#d1d1d1] bg-white px-2 py-1.5 text-xs shadow-sm focus:border-[#0067c0] focus:outline-none"
            />
            <button className="rounded border border-[#d1d1d1] bg-white px-3 text-xs hover:bg-[#f9f9f9]" onClick={onBrowseOutput}>
              ...
            </button>
          </div>
        </div>
        <div>
          <label className="mb-1 block text-xs">完整输出路径预览</label>
          <p className="break-all rounded border border-[#e5e5e5] bg-[#f9f9f9] p-2 text-[10px] italic text-[#5a5a5a]">{outputPath || '未生成'}</p>
        </div>
      </section>
    </aside>
  );
}
