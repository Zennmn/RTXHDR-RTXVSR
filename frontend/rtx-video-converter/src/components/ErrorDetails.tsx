import type { ApiError } from '../api/backendClient';

export function ErrorDetails({ error }: { error: ApiError | null }) {
  if (error === null) {
    return null;
  }

  return (
    <details className="rounded-lg border border-red-200 bg-red-50 p-3 text-xs text-red-800">
      <summary className="cursor-pointer font-semibold">{error.message}</summary>
      <pre className="mt-2 whitespace-pre-wrap break-words">{error.details || error.code}</pre>
    </details>
  );
}
