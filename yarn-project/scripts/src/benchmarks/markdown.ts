// Generate a markdown file with a table summary of the aggregated benchmarks.
// If a benchmark-base file is available, shows the comparison against base (ie master in a PR).
import { createConsoleLogger } from '@aztec/foundation/log';
import { BENCHMARK_HISTORY_BLOCK_SIZE, Metrics } from '@aztec/types/stats';

import * as fs from 'fs';
import pick from 'lodash.pick';

import { BaseBenchFile, BenchFile } from './paths.js';

// Input file paths
const inputFile = BenchFile;
const baseFile = BaseBenchFile;

const COMMENT_MARK = '<!-- AUTOGENERATED BENCHMARK COMMENT -->';
const S3_URL = 'https://aztec-ci-artifacts.s3.us-east-2.amazonaws.com';

// What % diff should be considered as a warning
const WARNING_DIFF_THRESHOLD = 15;
// When a measurement in ms should be considered "small"
const SMALL_MS_THRESHOLD = 200;
// What % diff should be considered as a warning for "small" ms measurements
const WARNING_DIFF_THRESHOLD_SMALL_MS = 30;

const log = createConsoleLogger();

/** Returns whether the value should be a warning, based on the % difference and absolute value. */
function isWarning(row: string, col: string, value: number, base: number | undefined) {
  if (base === undefined) {
    return false;
  }
  const absPercentDiff = Math.abs(Math.round(((value - base) / base) * 100));
  if ((row.endsWith('_ms') || col.endsWith('_ms')) && value < SMALL_MS_THRESHOLD) {
    return absPercentDiff >= WARNING_DIFF_THRESHOLD_SMALL_MS;
  } else {
    return absPercentDiff > WARNING_DIFF_THRESHOLD;
  }
}

/** Returns summary text for warnings */
function getWarningsSummary(
  data: Record<string, Record<string, number>>,
  base: Record<string, Record<string, number>> | undefined,
) {
  const warnings = getWarnings(data, base);
  if (!base) {
    return 'No base data found for comparison.';
  } else if (warnings.length) {
    return `Metrics with a significant change: \n${warnings.join('\n')}`;
  } else {
    return `No metrics with a significant change found.`;
  }
}

/** Returns a string with the % diff between value and base. */
function formatDiff(value: number, baseValue: number) {
  const percentDiff = Math.round(((value - baseValue) / baseValue) * 100);
  const percentSign = percentDiff > 0 ? '+' : '';
  return `<span title="${formatValue(baseValue)}">${percentSign}${percentDiff}%</span>`;
}

/** Gets a list of warnings. */
function getWarnings(
  data: Record<string, Record<string, number>>,
  base: Record<string, Record<string, number>> | undefined,
) {
  if (!base) {
    return [];
  }
  const warnings: string[] = [];
  for (const row in data) {
    if (row === 'timestamp') {
      continue;
    }
    for (const col in data[row]) {
      const value = data[row][col];
      const baseValue = (base[row] ?? {})[col];
      if (baseValue && isWarning(row, col, value, baseValue)) {
        const diffText = formatDiff(value, baseValue);
        warnings.push(`- **${withDesc(row)}** (${withDesc(col)}): ${formatValue(value)} (${diffText})`);
      }
    }
  }
  return warnings;
}

/** Returns a cell content formatted as string */
function getCell(
  data: Record<string, Record<string, number>>,
  base: Record<string, Record<string, number>> | undefined,
  row: string,
  col: string,
) {
  const value = data[row][col];
  const formattedValue = formatValue(value);
  const baseValue = base ? (base[row] ?? {})[col] : undefined;
  const percentDiff = baseValue ? Math.round(((value - baseValue) / baseValue) * 100) : undefined;
  if (!percentDiff || Math.abs(percentDiff) < 1) {
    return formattedValue;
  }
  if (!isWarning(row, col, value, baseValue)) {
    return `${formattedValue} (${formatDiff(value, baseValue!)})`;
  }
  return `:warning: ${formattedValue} (**${formatDiff(value, baseValue!)}**)`;
}

/** Wraps the metric name in a span with a title with the description, if found. */
function withDesc(name: string) {
  const description = Metrics.find(m => m.name === name)?.description;
  if (!description) {
    return name;
  }
  return `<span title="${description}">${name}</span>`;
}

/** Formats a numeric value for display. */
function formatValue(value: number) {
  if (value < 100) {
    return value.toPrecision(3);
  }
  return value.toLocaleString();
}

/** Transposes an object topmost and nested keys. */
function transpose(obj: any) {
  const transposed: any = {};
  for (const outerKey in obj) {
    const innerObj = obj[outerKey];
    for (const innerKey in innerObj) {
      if (!transposed[innerKey]) {
        transposed[innerKey] = {};
      }
      transposed[innerKey][outerKey] = innerObj[innerKey];
    }
  }
  return transposed;
}

/** Returns the base benchmark for comparison, if exists */
function getBaseBenchmark(): Record<string, Record<string, number>> | undefined {
  try {
    return JSON.parse(fs.readFileSync(baseFile, 'utf-8'));
  } catch {
    return undefined;
  }
}

/** Creates a table in md out of the data (rows and cols). */
function getTableContent(
  data: Record<string, Record<string, number>>,
  baseBenchmark: Record<string, Record<string, number>> | undefined,
  groupUnit = '',
  col1Title = 'Metric',
) {
  const rowKeys = Object.keys(data);
  const groups = [...new Set(rowKeys.flatMap(key => Object.keys(data[key])))];
  const makeHeader = (colTitle: string) => `${withDesc(colTitle)} ${groupUnit}`;
  const header = `| ${col1Title} | ${groups.map(makeHeader).join(' | ')} |`;
  const separator = `| - | ${groups.map(() => '-').join(' | ')} |`;
  const makeCell = (row: string, col: string) => getCell(data, baseBenchmark, row, col);
  const rows = rowKeys.map(key => `${withDesc(key)} | ${groups.map(g => makeCell(key, g)).join(' | ')} |`);

  return `
${header}
${separator}
${rows.join('\n')}
  `;
}

/** Creates a md with the benchmark contents. */
export function getMarkdown() {
  const benchmark = JSON.parse(fs.readFileSync(inputFile, 'utf-8'));
  const baseBenchmark = getBaseBenchmark();

  const metricsByBlockSize = Metrics.filter(m => m.groupBy === 'block-size').map(m => m.name);
  const metricsByChainLength = Metrics.filter(m => m.groupBy === 'chain-length').map(m => m.name);
  const metricsByCircuitName = Metrics.filter(m => m.groupBy === 'circuit-name').map(m => m.name);
  const metricsByContractCount = Metrics.filter(m => m.groupBy === 'contract-count').map(m => m.name);

  const baseHash = process.env.BASE_COMMIT_HASH;
  const baseUrl = baseHash && `[\`${baseHash.slice(0, 8)}\`](${S3_URL}/benchmarks-v1/master/${baseHash}.json)`;
  const baseCommitText = baseUrl
    ? `\nValues are compared against data from master at commit ${baseUrl} and shown if the difference exceeds 1%.`
    : '';

  const prNumber = process.env.CIRCLE_PULL_REQUEST && parseInt(process.env.CIRCLE_PULL_REQUEST.split('/')[6]);
  const prSourceDataUrl = prNumber && `${S3_URL}/benchmarks-v1/pulls/${prNumber}.json`;
  const prSourceDataText = prSourceDataUrl
    ? `\nThis benchmark source data is available in JSON format on S3 [here](${prSourceDataUrl}).`
    : '';

  return `
## Benchmark results

${getWarningsSummary(benchmark, baseBenchmark)}

<details>

<summary>Detailed results</summary>

All benchmarks are run on txs on the \`Benchmarking\` contract on the repository. Each tx consists of a batch call  to \`create_note\` and \`increment_balance\`, which guarantees that each tx has a private call, a nested private call, a public call, and a nested public call, as well as an emitted private note, an unencrypted log, and public storage read and write. 
${prSourceDataText}
${baseCommitText}

### L2 block published to L1

Each column represents the number of txs on an L2 block published to L1.
${getTableContent(pick(benchmark, metricsByBlockSize), baseBenchmark, 'txs')}

### L2 chain processing

Each column represents the number of blocks on the L2 chain where each block has ${BENCHMARK_HISTORY_BLOCK_SIZE} txs.
${getTableContent(pick(benchmark, metricsByChainLength), baseBenchmark, 'blocks')}

### Circuits stats

Stats on running time and I/O sizes collected for every circuit run across all benchmarks.
${getTableContent(transpose(pick(benchmark, metricsByCircuitName)), transpose(baseBenchmark), '', 'Circuit')}

### Miscellaneous

Transaction sizes based on how many contracts are deployed in the tx.
${getTableContent(pick(benchmark, metricsByContractCount), baseBenchmark, 'deployed contracts')}

</details>
${COMMENT_MARK}
`;
}

/** Entrypoint */
export function main() {
  log(getMarkdown());
}
