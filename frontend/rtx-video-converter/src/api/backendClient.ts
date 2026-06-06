import type {
  BackendErrorBody,
  CapabilityResponse,
  CreateJobResponse,
  HealthResponse,
  JobSnapshot,
  MediaProbeResponse,
  TranscodeRequest,
} from './types';

export class ApiError extends Error {
  readonly code: string;
  readonly details: string;
  readonly status: number;

  constructor({ code, message, details, status }: { code: string; message: string; details: string; status: number }) {
    super(message);
    this.name = 'ApiError';
    this.code = code;
    this.details = details;
    this.status = status;
  }
}

async function readJson(response: Response): Promise<unknown> {
  const text = await response.text();
  if (text.length === 0) {
    return {};
  }

  try {
    return JSON.parse(text) as unknown;
  } catch (error) {
    throw new ApiError({
      code: 'invalid_response_json',
      message: 'Backend returned invalid JSON.',
      details: error instanceof Error ? error.message : String(error),
      status: response.status,
    });
  }
}

function isErrorBody(value: unknown): value is BackendErrorBody {
  return (
    typeof value === 'object' &&
    value !== null &&
    'error' in value &&
    typeof (value as BackendErrorBody).error?.code === 'string'
  );
}

export class BackendClient {
  private authToken = '';

  constructor(private baseUrl = import.meta.env.VITE_BACKEND_BASE_URL ?? 'http://127.0.0.1:49321') {}

  setBaseUrl(baseUrl: string): void {
    this.baseUrl = baseUrl;
  }

  setAuthToken(token: string): void {
    this.authToken = token;
  }

  async getHealth(): Promise<HealthResponse> {
    return this.request('/api/health');
  }

  async getCapabilities(): Promise<CapabilityResponse> {
    return this.request('/api/capabilities');
  }

  async probeMedia(inputPath: string): Promise<MediaProbeResponse> {
    return this.request('/api/media/probe', {
      method: 'POST',
      body: JSON.stringify({ inputPath }),
    });
  }

  async createJob(request: TranscodeRequest): Promise<CreateJobResponse> {
    return this.request('/api/jobs', {
      method: 'POST',
      body: JSON.stringify(request),
    });
  }

  async getJob(id: string): Promise<JobSnapshot> {
    return this.request(`/api/jobs/${encodeURIComponent(id)}`);
  }

  async cancelJob(id: string): Promise<{ accepted: boolean }> {
    return this.request(`/api/jobs/${encodeURIComponent(id)}/cancel`, {
      method: 'POST',
    });
  }

  private async request<T>(path: string, init: RequestInit = {}): Promise<T> {
    let response: Response;
    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
      ...((init.headers as Record<string, string> | undefined) ?? {}),
    };
    if (this.authToken.length > 0) {
      headers['X-VSR-Token'] = this.authToken;
    }
    try {
      response = await fetch(`${this.baseUrl}${path}`, {
        ...init,
        headers,
      });
    } catch (error) {
      throw new ApiError({
        code: 'network_error',
        message: 'Backend is not reachable.',
        details: error instanceof Error ? error.message : String(error),
        status: 0,
      });
    }

    const json = await readJson(response);
    if (!response.ok) {
      if (isErrorBody(json)) {
        throw new ApiError({ ...json.error, status: response.status });
      }
      throw new ApiError({
        code: 'http_error',
        message: `Backend returned HTTP ${response.status}.`,
        details: JSON.stringify(json),
        status: response.status,
      });
    }

    return json as T;
  }
}

export const backendClient = new BackendClient();
