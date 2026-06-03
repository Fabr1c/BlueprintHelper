import {
  createMetricsCollector,
  type MetricsCollector,
} from './metrics-collector.js';
import {
  createMetricsStore,
  type MetricsStore,
} from './metrics-store.js';
import {
  NOOP_METRICS_SINK,
  type MetricsEvent,
  type MetricsEventSink,
} from './metrics-types.js';

export interface CreateMetricsServiceOptions {
  root: string;
  disabled?: boolean;
  now?: Date | (() => Date);
  staleAfterMs?: number;
}

export interface CreateNoopMetricsServiceOptions {
  now?: Date | (() => Date);
}

export interface MetricsService extends MetricsEventSink {
  enabled: boolean;
  collector: MetricsCollector;
  store?: MetricsStore;
}

export function createMetricsService(options: CreateMetricsServiceOptions): MetricsService {
  if (options.disabled === true) {
    return createNoopMetricsService({ now: options.now });
  }

  const store = createMetricsStore({
    root: options.root,
    now: options.now,
  });
  const sink: MetricsEventSink = {
    async record(event) {
      try {
        await store.record(event);
        await store.upsertEpisodeAttempt(event);
        await store.closeStaleEpisodes(options.staleAfterMs === undefined
          ? undefined
          : { staleAfterMs: options.staleAfterMs });
      } catch {
        return;
      }
    },
  };

  return {
    enabled: true,
    store,
    collector: createMetricsCollector({
      sink,
      now: options.now,
    }),
    record(event: MetricsEvent) {
      return sink.record(event);
    },
  };
}

export function createNoopMetricsService(options: CreateNoopMetricsServiceOptions = {}): MetricsService {
  return {
    enabled: false,
    collector: createMetricsCollector({
      sink: NOOP_METRICS_SINK,
      now: options.now,
    }),
    record() {
      return undefined;
    },
  };
}

export const NOOP_METRICS_SERVICE = createNoopMetricsService();
