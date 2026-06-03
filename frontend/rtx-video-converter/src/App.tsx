import React, { useState, useEffect } from 'react';
import { Settings, FileVideo } from 'lucide-react';
import { WinSlider, WinSegmentedControl, WinSwitch } from './components/WinUI';
import { ProcessingMode, Codec, FileInfo } from './types';

export default function App() {
  const [fileInfo, setFileInfo] = useState<FileInfo | null>(null);
  const [outputPath, setOutputPath] = useState('C:\\Videos\\Output');
  const [mode, setMode] = useState<ProcessingMode>('both');
  
  // VSR settings
  const [vsrScale, setVsrScale] = useState(2);
  const [vsrQuality, setVsrQuality] = useState(4);
  
  // HDR settings
  const [hdrContrast, setHdrContrast] = useState(100);
  const [hdrSaturation, setHdrSaturation] = useState(100);
  const [hdrMidGray, setHdrMidGray] = useState(44);
  const [hdrMaxBrightness, setHdrMaxBrightness] = useState(1000);
  
  // Output settings
  const [codec, setCodec] = useState<Codec>('HEVC Main10');
  const [keepAudio, setKeepAudio] = useState(true);
  const [keepSubtitles, setKeepSubtitles] = useState(true);

  // Single Task Processing State
  const [isProcessing, setIsProcessing] = useState(false);
  const [progress, setProgress] = useState(0);
  const [stage, setStage] = useState('等待开始');

  // Lock Codec based on mode
  useEffect(() => {
    if (mode === 'hdr' || mode === 'both') {
      setCodec('HEVC Main10');
    }
  }, [mode]);

  // Simulate file drop
  const handleDrop = (e: React.DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    const droppedName = e.dataTransfer.files[0]?.name || 'demo_anime_episode.mp4';
    setFileInfo({
      name: droppedName,
      size: 345001234,
      resolution: '1920x1080',
      duration: '00:24:15',
      codec: 'H.264 8-bit'
    });
  };

  const handleDragOver = (e: React.DragEvent<HTMLDivElement>) => e.preventDefault();

  const handleStartOrCancel = () => {
    if (isProcessing) {
      setIsProcessing(false);
      setStage('已取消');
      setProgress(0);
    } else {
      setIsProcessing(true);
      setProgress(0);
      setStage('验证中...');
    }
  };

  // Processing Simulation Engine
  useEffect(() => {
    if (!isProcessing) return;
    
    const interval = setInterval(() => {
      setProgress(prev => {
        let newProgress = prev + (Math.random() * 1.5 + 0.5);
        if (newProgress >= 100) {
          setIsProcessing(false);
          setStage('转码完成');
          return 100;
        }
        
        let newStage = stage;
        if (newProgress < 3) newStage = '验证视频流中...';
        else if (newProgress < 30) newStage = '读取 / 预处理';
        else if (newProgress < 85) newStage = `RTX AI 处理中 (${mode.toUpperCase()})`;
        else if (newProgress < 98) newStage = 'NVENC 硬件编码中';
        else newStage = 'Muxing 封装中...';
        
        setStage(newStage);
        return newProgress;
      });
    }, 500);
    
    return () => clearInterval(interval);
  }, [isProcessing, mode, stage]);

  const previewFilename = fileInfo ? `${fileInfo.name.replace(/\.[^/.]+$/, '')}_RTX.mp4` : '待定...';
  const fullOutputPath = outputPath.endsWith('\\') || outputPath.endsWith('/') 
    ? `${outputPath}${previewFilename}` 
    : `${outputPath}\\${previewFilename}`;

  return (
    <div className="flex flex-col h-screen bg-[#f3f3f3] text-[#1a1a1a] overflow-hidden font-sans select-none border border-[#d1d1d1]">
      
      {/* TopBar */}
      <header className="h-12 flex items-center justify-between px-4 bg-[#f9f9f9] border-b border-[#e5e5e5] shrink-0">
        <div className="flex items-center gap-3">
          <div className="w-6 h-6 bg-[#76b900] rounded-sm flex items-center justify-center text-white font-bold text-xs">RTX</div>
          <h1 className="text-sm font-semibold text-[#1d1d1d]">RTX Video Converter</h1>
        </div>
        <div className="flex items-center gap-4">
          <button className="p-1.5 hover:bg-black/5 rounded text-xs text-[#1a1a1a] flex items-center gap-1">
            <Settings className="w-3.5 h-3.5" /> 设置
          </button>
        </div>
      </header>

      {/* Main Content Area */}
      <main className="flex flex-1 overflow-hidden">
        
        {/* Left Pane - Input & Output */}
        <aside className="w-80 shrink-0 border-r border-[#e5e5e5] bg-white flex flex-col p-4 gap-6 overflow-y-auto hidden md:flex">
          <section>
            <h2 className="text-[11px] font-bold uppercase tracking-wider text-[#7a7a7a] mb-3">输入源</h2>
            
            {/* Dropzone */}
            <div 
              className="border-2 border-dashed border-[#d1d1d1] rounded-lg p-6 flex flex-col items-center justify-center text-center cursor-pointer bg-[#fcfcfc] mb-4 hover:border-[#aaa] transition-colors h-[120px]"
              onDrop={handleDrop}
              onDragOver={handleDragOver}
            >
              <div className="text-2xl mb-2">📂</div>
              <p className="text-xs text-[#5a5a5a]">拖拽视频至此或 <span className="text-[#0067c0] cursor-pointer hover:underline">浏览</span></p>
            </div>
            
            {/* File Info */}
            <div className="bg-[#f9f9f9] border border-[#e5e5e5] rounded p-3 text-[11px] space-y-2">
              <div className="flex justify-between">
                <span>文件名:</span>
                <span className="font-medium truncate max-w-[140px]" title={fileInfo?.name || '未选择'}>{fileInfo?.name || '未选择'}</span>
              </div>
              <div className="flex justify-between">
                <span>分辨率:</span>
                <span className="font-medium">{fileInfo ? `${fileInfo.resolution} → ${mode!=='hdr'?'4K/8K':'不变'}` : '-'}</span>
              </div>
              <div className="flex justify-between">
                <span>时长:</span>
                <span className="font-medium">{fileInfo?.duration || '-'}</span>
              </div>
              <div className="flex justify-between">
                <span>编码:</span>
                <span className="font-medium text-[#0067c0]">{fileInfo?.codec || '-'}</span>
              </div>
            </div>
          </section>

          {/* Output Path */}
          <section className="flex-grow flex flex-col gap-4">
            <h2 className="text-[11px] font-bold uppercase tracking-wider text-[#7a7a7a] mb-0">输出设置</h2>
            
            <div>
              <label className="text-xs mb-1 block">输出目录</label>
              <div className="flex gap-1">
                <input type="text" value={outputPath} onChange={e=>setOutputPath(e.target.value)} className="flex-grow text-xs border border-[#d1d1d1] px-2 py-1.5 rounded bg-white shadow-sm focus:outline-none focus:border-[#0067c0]" />
                <button className="px-3 border border-[#d1d1d1] rounded bg-white text-xs hover:bg-[#f9f9f9]" title="选择文件夹">...</button>
              </div>
            </div>

            <div>
              <label className="text-xs mb-1 block">完整输出路径预览</label>
              <p className="text-[10px] text-[#5a5a5a] break-all italic bg-[#f9f9f9] p-2 border border-[#e5e5e5] rounded">
                {fullOutputPath}
              </p>
            </div>
          </section>
        </aside>

        {/* Right Pane - Settings */}
        <section className="flex-grow flex flex-col bg-[#f3f3f3] overflow-y-auto">
           <div className="p-6 space-y-6 max-w-4xl w-full">
              
              {/* Mode Selector */}
              <WinSegmentedControl 
                options={[
                  { label: 'VSR 超分', value: 'vsr' },
                  { label: 'HDR 映射', value: 'hdr' },
                  { label: 'VSR + HDR', value: 'both' }
                ]}
                value={mode}
                onChange={(v) => setMode(v as ProcessingMode)}
              />

              <div className="grid grid-cols-1 md:grid-cols-2 gap-6 w-full">
                
                {/* VSR Section */}
                <div className={`bg-white p-5 rounded-xl border border-[#e5e5e5] shadow-sm transition-opacity duration-300 ${mode === 'hdr' ? 'opacity-40 pointer-events-none' : 'opacity-100'}`}>
                  <h3 className="text-sm font-semibold mb-4 text-[#1a1a1a]">VSR 参数</h3>
                  <div className="space-y-5">
                    <WinSlider label="提升倍率" value={vsrScale} min={1} max={4} step={1} onChange={setVsrScale} formatValue={v => `${v}.0x`} />
                    
                    <div>
                      <div className="flex justify-between text-xs mb-2">
                        <span className="text-[#1a1a1a]">处理质量</span>
                        <span className="font-mono text-[#1a1a1a]">质量 {vsrQuality} {vsrQuality === 4 ? '(自动)' : ''}</span>
                      </div>
                      <div className="flex gap-1">
                        {[1, 2, 3, 4].map(q => (
                          <button 
                            key={q}
                            onClick={() => setVsrQuality(q)}
                            className={`flex-1 py-1.5 text-xs border rounded transition-colors ${vsrQuality === q ? 'border-[#0067c0] bg-blue-50 text-[#0067c0] font-medium' : 'bg-[#f9f9f9] border-[#e5e5e5] text-[#1a1a1a] hover:bg-gray-100'}`}
                          >
                            {q}
                          </button>
                        ))}
                      </div>
                    </div>
                  </div>
                </div>

                {/* HDR Section */}
                <div className={`bg-white p-5 rounded-xl border border-[#e5e5e5] shadow-sm transition-opacity duration-300 ${mode === 'vsr' ? 'opacity-40 pointer-events-none' : 'opacity-100'}`}>
                  <h3 className="text-sm font-semibold mb-4 text-[#1a1a1a]">HDR 参数</h3>
                  
                  <div className="grid grid-cols-2 gap-y-4 gap-x-4">
                    <div>
                      <label className="text-[10px] text-[#7a7a7a] block mb-1">对比度</label>
                      <input type="number" step="1" value={hdrContrast} onChange={e=>setHdrContrast(Number(e.target.value))} className="w-full font-mono text-xs border border-[#d1d1d1] focus:border-[#0067c0] focus:outline-none p-1.5 rounded" />
                    </div>
                    <div>
                      <label className="text-[10px] text-[#7a7a7a] block mb-1">饱和度</label>
                      <input type="number" step="1" value={hdrSaturation} onChange={e=>setHdrSaturation(Number(e.target.value))} className="w-full font-mono text-xs border border-[#d1d1d1] focus:border-[#0067c0] focus:outline-none p-1.5 rounded" />
                    </div>
                    <div>
                      <label className="text-[10px] text-[#7a7a7a] block mb-1">中灰度</label>
                      <input type="number" step="1" value={hdrMidGray} onChange={e=>setHdrMidGray(Number(e.target.value))} className="w-full font-mono text-xs border border-[#d1d1d1] focus:border-[#0067c0] focus:outline-none p-1.5 rounded" />
                    </div>
                    <div>
                      <label className="text-[10px] text-[#7a7a7a] block mb-1">最大亮度 (Nits)</label>
                      <input type="number" step="50" value={hdrMaxBrightness} onChange={e=>setHdrMaxBrightness(Number(e.target.value))} className="w-full font-mono text-xs border border-[#d1d1d1] focus:border-[#0067c0] focus:outline-none p-1.5 rounded" />
                    </div>
                  </div>
                </div>

              </div>

              {/* Encoding Config */}
              <div className="bg-white p-5 rounded-xl border border-[#e5e5e5] shadow-sm">
                <h3 className="text-sm font-semibold mb-4 text-[#1a1a1a]">编码器与轨道</h3>
                <div className="grid grid-cols-1 md:grid-cols-3 gap-6 text-xs">
                  <div>
                    <label className="block text-[#7a7a7a] mb-1">视频编码</label>
                    <select 
                      className="w-full p-2 border border-[#d1d1d1] rounded bg-[#f9f9f9] focus:outline-none focus:border-[#0067c0] text-[#1a1a1a]"
                      value={codec}
                      onChange={e => setCodec(e.target.value as Codec)}
                      disabled={mode !== 'vsr'}
                    >
                      <option value="H.264">NVENC H.264</option>
                      <option value="HEVC Main10">NVENC HEVC Main10</option>
                    </select>
                  </div>
                  <div>
                    <label className="block text-[#7a7a7a] mb-1">封装容器</label>
                    <div className="w-full p-2 border border-[#d1d1d1] rounded bg-[#f9f9f9] text-[#1a1a1a] opacity-80 cursor-not-allowed">
                      MPEG-4 (.mp4)
                    </div>
                  </div>
                  <div>
                    <label className="block text-[#7a7a7a] mb-1">音频/字幕</label>
                    <div className="flex items-center gap-4 mt-2">
                      <WinSwitch label="音频" checked={keepAudio} onChange={setKeepAudio} />
                      <WinSwitch label="字幕" checked={keepSubtitles} onChange={setKeepSubtitles} />
                    </div>
                  </div>
                </div>
              </div>
           </div>
        </section>
      </main>

      {/* Bottom Status Bar */}
      <footer className="h-[84px] bg-white border-t border-[#e5e5e5] px-6 py-3 flex items-center gap-8 shadow-[0_-4px_12px_rgba(0,0,0,0.03)] shrink-0 z-20">
        <div className="flex-grow">
          <div className="flex justify-between items-end mb-1.5">
            <div className="flex items-center gap-3">
              <span className={`text-sm font-bold ${isProcessing ? 'text-[#0067c0]' : 'text-[#a1a1a1]'}`}>
                {Math.round(progress)}%
              </span>
              <span className="text-xs font-medium text-[#1a1a1a]">
                状态: {stage}
              </span>
            </div>
          </div>
          <div className="h-1.5 w-full bg-[#eee] rounded-full overflow-hidden">
            <div 
              className="h-full bg-[#0067c0] rounded-full transition-all duration-300 ease-out" 
              style={{ width: `${progress}%` }}
            ></div>
          </div>
        </div>
        
        <div className="flex items-center gap-3 shrink-0">
          <button 
            onClick={handleStartOrCancel}
            className={`px-10 h-11 rounded font-semibold text-sm shadow-md transition-all ${
              isProcessing 
                ? 'bg-white border border-[#d1d1d1] text-[#dc2626] hover:bg-black/5' 
                : 'bg-[#0067c0] text-white hover:bg-[#005bb0] border border-transparent'
            }`}
          >
            {isProcessing ? '取消任务' : '开始转码'}
          </button>
        </div>
      </footer>

    </div>
  );
}
