import type { JobSnapshot } from '../api/types';

export function StatusFooter({
  job,
  canStart,
  submitting,
  canceling,
  disabledReason,
  onStart,
  onCancel,
}: {
  job: JobSnapshot | null;
  canStart: boolean;
  submitting: boolean;
  canceling: boolean;
  disabledReason: string | null;
  onStart: () => void;
  onCancel: () => void;
}) {
  const active = job !== null && ['queued', 'running', 'canceling'].includes(job.state);
  const progress = job ? Math.round(job.progress * 100) : 0;
  const label = job ? `${job.state}: ${job.stage}` : '等待开始';

  return (
    <footer className="z-20 flex h-[84px] shrink-0 items-center gap-8 border-t border-[#e5e5e5] bg-white px-6 py-3 shadow-[0_-4px_12px_rgba(0,0,0,0.03)]">
      <div className="grow">
        <div className="mb-1.5 flex items-end justify-between">
          <div className="flex items-center gap-3">
            <span className={`text-sm font-bold ${active ? 'text-[#0067c0]' : 'text-[#a1a1a1]'}`}>{progress}%</span>
            <span className="text-xs font-medium text-[#1a1a1a]">状态: {label}</span>
            {job && (
              <span className="text-xs text-[#5a5a5a]">
                {job.framesDone}/{job.framesTotal} frames · {job.fps.toFixed(1)} fps · ETA {job.etaSeconds}s
              </span>
            )}
          </div>
        </div>
        <div className="h-1.5 w-full overflow-hidden rounded-full bg-[#eee]">
          <div className="h-full rounded-full bg-[#0067c0] transition-all duration-300 ease-out" style={{ width: `${progress}%` }} />
        </div>
      </div>
      <button
        onClick={active ? onCancel : onStart}
        disabled={active ? canceling : !canStart || submitting}
        className={`h-11 rounded px-10 text-sm font-semibold shadow-md transition-all disabled:cursor-not-allowed disabled:opacity-50 ${
          active ? 'border border-[#d1d1d1] bg-white text-[#dc2626] hover:bg-black/5' : 'border border-transparent bg-[#0067c0] text-white hover:bg-[#005bb0]'
        }`}
      >
        {active ? (canceling ? '正在取消...' : '取消任务') : submitting ? '提交中...' : '开始转码'}
      </button>
      {!active && disabledReason !== null && <span className="max-w-56 text-xs text-[#7a7a7a]">{disabledReason}</span>}
    </footer>
  );
}
