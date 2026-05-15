import type { BlueprintHelperToolContext } from '../../types.js';

export async function safeBridgePing(context: BlueprintHelperToolContext): Promise<boolean> {
  try {
    return await context.bridge.ping();
  } catch {
    return false;
  }
}

export async function waitForBridgeUnavailable(context: BlueprintHelperToolContext, timeoutMs: number): Promise<boolean> {
  const startTime = Date.now();
  const sleep = context.sleep ?? ((ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)));
  while (Date.now() - startTime < timeoutMs) {
    await sleep(1000);
    if (!await safeBridgePing(context)) {
      return true;
    }
  }
  return false;
}
