export interface Resolution {
  width: number;
  height: number;
}

export function parseResolution(value: string | null | undefined): Resolution | null {
  const match = value?.match(/(\d+)\s*[xX]\s*(\d+)/);
  if (!match) {
    return null;
  }

  const width = Number(match[1]);
  const height = Number(match[2]);
  return width > 0 && height > 0 ? { width, height } : null;
}

export function evenScaledDimension(value: number, scale: number): number {
  const rounded = Math.max(2, Math.round(value * scale));
  return rounded % 2 === 0 ? rounded : rounded + 1;
}

export function scaleResolution(resolution: Resolution, scale: number): Resolution {
  return {
    width: evenScaledDimension(resolution.width, scale),
    height: evenScaledDimension(resolution.height, scale),
  };
}

export function formatResolution(resolution: Resolution): string {
  return `${resolution.width}x${resolution.height}`;
}
