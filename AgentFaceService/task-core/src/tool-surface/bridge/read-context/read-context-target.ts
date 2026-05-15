import type { ReadContextInput } from './read-context-schemas.js';
import { omitUndefined } from '../bridge-tool-result-utils.js';

export function buildReadContextTarget(input: ReadContextInput) {
  return omitUndefined({
    asset_path: input.target.asset_path,
    asset_type: input.target.asset_type,
    target_type: input.target.target_type,
    target_name: input.target.target_name,
    block_id: input.target.block_id,
  }) as never;
}
