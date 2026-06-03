import { useEffect, useRef, useState } from 'react';
import { ApiError, backendClient } from '../api/backendClient';
import type { JobSnapshot, TranscodeRequest } from '../api/types';

const ACTIVE_STATES = new Set(['queued', 'running', 'canceling']);

export interface TranscodeJobState {
  activeJob: JobSnapshot | null;
  submitting: boolean;
  canceling: boolean;
  error: ApiError | null;
  startJob: (request: TranscodeRequest) => Promise<void>;
  cancelJob: () => Promise<void>;
  clearJob: () => void;
}

export function useTranscodeJob(): TranscodeJobState {
  const [activeJob, setActiveJob] = useState<JobSnapshot | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const [canceling, setCanceling] = useState(false);
  const [error, setError] = useState<ApiError | null>(null);
  const activeJobId = useRef<string | null>(null);

  const startJob = async (request: TranscodeRequest) => {
    setSubmitting(true);
    setError(null);
    try {
      const created = await backendClient.createJob(request);
      activeJobId.current = created.id;
      setActiveJob({
        id: created.id,
        state: 'queued',
        stage: 'validating',
        progress: 0,
        framesDone: 0,
        framesTotal: 0,
        fps: 0,
        etaSeconds: 0,
        inputPath: request.inputPath,
        outputPath: request.outputPath,
        warnings: [],
      });
    } catch (nextError) {
      setError(
        nextError instanceof ApiError
          ? nextError
          : new ApiError({
              code: 'job_submit_failed',
              message: 'Job submission failed.',
              details: String(nextError),
              status: 0,
            }),
      );
    } finally {
      setSubmitting(false);
    }
  };

  const cancelJob = async () => {
    if (activeJobId.current === null) {
      return;
    }

    setCanceling(true);
    try {
      await backendClient.cancelJob(activeJobId.current);
    } catch (nextError) {
      if (nextError instanceof ApiError && nextError.status === 409) {
        const snapshot = await backendClient.getJob(activeJobId.current);
        setActiveJob(snapshot);
      } else {
        setError(nextError instanceof ApiError ? nextError : null);
      }
    } finally {
      setCanceling(false);
    }
  };

  const clearJob = () => {
    activeJobId.current = null;
    setActiveJob(null);
    setError(null);
    setCanceling(false);
    setSubmitting(false);
  };

  useEffect(() => {
    if (activeJobId.current === null) {
      return;
    }

    const interval = window.setInterval(async () => {
      if (activeJobId.current === null) {
        return;
      }

      try {
        const snapshot = await backendClient.getJob(activeJobId.current);
        setActiveJob(snapshot);
        if (!ACTIVE_STATES.has(snapshot.state)) {
          activeJobId.current = null;
        }
      } catch (nextError) {
        setError(nextError instanceof ApiError ? nextError : null);
      }
    }, 500);

    return () => window.clearInterval(interval);
  }, [activeJob?.id]);

  return { activeJob, submitting, canceling, error, startJob, cancelJob, clearJob };
}
