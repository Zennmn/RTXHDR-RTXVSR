import { directoryName } from './jobRequest';

export function nextOutputDirectoryForInput(inputPath: string, currentOutputDirectory: string): string {
  if (currentOutputDirectory.trim().length > 0) {
    return currentOutputDirectory;
  }

  return directoryName(inputPath);
}
