import React, { useCallback, useEffect, useState } from 'react';
import { Settings } from 'lucide-react';
import { ApiError, backendClient } from './api/backendClient';
import { CapabilityBanner } from './components/CapabilityBanner';
import { ErrorDetails } from './components/ErrorDetails';
import { InputPanel } from './components/InputPanel';
import { SettingsPanel } from './components/SettingsPanel';
import { StatusFooter } from './components/StatusFooter';
import { useBackendStatus } from './hooks/useBackendStatus';
import { useTranscodeJob } from './hooks/useTranscodeJob';
import { conversionCapabilityProblem } from './lib/capabilities';
import { nextOutputDirectoryForInput } from './lib/inputSelection';
import { buildOutputPath, buildTranscodeRequest, defaultSettings, directoryName } from './lib/jobRequest';
import { listenForDroppedInputPath, pickInputVideo, pickOutputDirectory } from './lib/tauriBridge';
import type { ConversionSettings, SelectedInput } from './types';

export default function App() {
  const backend = useBackendStatus();
  const job = useTranscodeJob();
  const [selectedInput, setSelectedInput] = useState<SelectedInput>({ path: '', metadata: null });
  const [inputPathDraft, setInputPathDraft] = useState('');
  const [inputError, setInputError] = useState<ApiError | null>(null);
  const [outputDirectory, setOutputDirectory] = useState('');
  const [settings, setSettings] = useState<ConversionSettings>(defaultSettings);
  const [loadingInput, setLoadingInput] = useState(false);

  useEffect(() => {
    if (settings.mode === 'hdr' || settings.mode === 'both') {
      setSettings((current) => (current.codec === 'hevc' ? current : { ...current, codec: 'hevc' }));
    }
  }, [settings.mode]);

  const resolvedOutputDirectory = outputDirectory || directoryName(selectedInput.path);
  const outputPath = selectedInput.path ? buildOutputPath(selectedInput.path, resolvedOutputDirectory) : '';
  const capabilityProblem = conversionCapabilityProblem(backend.capabilities, settings);
  const disabledReason =
    job.activeJob === null
      ? inputError?.message ??
        capabilityProblem ??
        (backend.status === 'offline'
          ? '后端服务不可用。'
          : loadingInput
            ? '正在探测媒体信息...'
            : selectedInput.metadata === null
              ? '请选择并成功探测输入视频。'
              : resolvedOutputDirectory.length === 0
                ? '请选择输出目录。'
                : null)
      : null;
  const canStart =
    backend.status !== 'offline' &&
    !loadingInput &&
    selectedInput.path.length > 0 &&
    selectedInput.metadata !== null &&
    resolvedOutputDirectory.length > 0 &&
    capabilityProblem === null &&
    inputError === null &&
    job.activeJob === null;

  const loadInput = useCallback(async (path: string) => {
    const trimmedPath = path.trim();
    if (!trimmedPath) {
      return;
    }

    setInputError(null);
    setLoadingInput(true);
    setInputPathDraft(trimmedPath);
    setSelectedInput({ path: '', metadata: null });
    setOutputDirectory((current) => nextOutputDirectoryForInput(trimmedPath, current));

    try {
      const metadata = await backendClient.probeMedia(trimmedPath);
      setSelectedInput({ path: trimmedPath, metadata });
      setInputPathDraft(trimmedPath);
      setInputError(null);
    } catch (error) {
      setSelectedInput({ path: '', metadata: null });
      setInputError(
        error instanceof ApiError
          ? error
          : new ApiError({
              code: 'probe_error',
              message: '无法探测输入视频。',
              details: error instanceof Error ? error.message : String(error),
              status: 0,
            }),
      );
    } finally {
      setLoadingInput(false);
    }
  }, []);

  useEffect(() => {
    let unlisten: (() => void) | null = null;
    let disposed = false;

    void listenForDroppedInputPath((path) => {
      void loadInput(path);
    }).then((cleanup) => {
      if (disposed) {
        cleanup?.();
        return;
      }
      unlisten = cleanup;
    });

    return () => {
      disposed = true;
      unlisten?.();
    };
  }, [loadInput]);

  const chooseInput = async () => {
    const path = await pickInputVideo();
    if (path !== null) {
      await loadInput(path);
    }
  };

  const chooseOutputDirectory = async () => {
    const path = await pickOutputDirectory();
    if (path !== null) {
      setOutputDirectory(path);
    }
  };

  const start = async () => {
    await job.startJob(
      buildTranscodeRequest({
        inputPath: selectedInput.path,
        outputDirectory: resolvedOutputDirectory,
        settings,
      }),
    );
  };

  return (
    <div className="flex h-screen flex-col overflow-hidden border border-[#d1d1d1] bg-[#f3f3f3] font-sans text-[#1a1a1a] select-none">
      <header className="flex h-12 shrink-0 items-center justify-between border-b border-[#e5e5e5] bg-[#f9f9f9] px-4">
        <div className="flex items-center gap-3">
          <div className="flex h-6 w-6 items-center justify-center rounded-sm bg-[#76b900] text-xs font-bold text-white">RTX</div>
          <h1 className="text-sm font-semibold text-[#1d1d1d]">RTX Video Converter</h1>
        </div>
        <div className="flex items-center gap-4">
          <button className="flex items-center gap-1 rounded p-1.5 text-xs text-[#1a1a1a] hover:bg-black/5">
            <Settings className="h-3.5 w-3.5" /> 设置
          </button>
        </div>
      </header>

      <main className="flex flex-1 overflow-hidden">
        <InputPanel
          inputPath={inputPathDraft}
          metadata={selectedInput.metadata}
          outputDirectory={outputDirectory}
          outputPath={outputPath}
          onBrowseFile={() => {
            void chooseInput();
          }}
          onBrowseOutput={() => {
            void chooseOutputDirectory();
          }}
          onDropPath={(path) => {
            void loadInput(path);
          }}
          onInputPathChange={(path) => {
            setInputPathDraft(path);
          }}
          onInputPathCommit={(path) => {
            void loadInput(path);
          }}
          onOutputDirectoryChange={setOutputDirectory}
        />

        <section className="flex grow flex-col overflow-y-auto">
          <div className="space-y-4 p-6">
            <CapabilityBanner capabilities={backend.capabilities} message={loadingInput ? '正在探测媒体信息...' : backend.message} />
            <ErrorDetails error={inputError ?? job.error} />
          </div>
          <SettingsPanel
            settings={settings}
            capabilities={backend.capabilities}
            inputResolution={selectedInput.metadata?.resolution ?? null}
            disabled={job.activeJob !== null}
            onChange={setSettings}
          />
        </section>
      </main>

      <StatusFooter
        job={job.activeJob}
        canStart={canStart}
        submitting={job.submitting || loadingInput}
        canceling={job.canceling}
        disabledReason={disabledReason}
        onStart={() => {
          void start();
        }}
        onCancel={() => {
          void job.cancelJob();
        }}
      />
    </div>
  );
}
