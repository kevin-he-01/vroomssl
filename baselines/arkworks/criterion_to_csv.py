#!/usr/bin/env python3
"""
Parse Rust Criterion benchmark output (estimates.json) and emit a single CSV.

Designed for trees like:
  ./arkworks/target/criterion/<bench name>/new/estimates.json

Example:
  ./scripts/criterion_to_csv.py \
    --input ./arkworks/target/criterion \
    --output modmul_criterion.csv
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Optional


SIZE_SUFFIX_RE = re.compile(r"^(?P<name>.*\D)?(?P<size>\d+)$")


@dataclass(frozen=True)
class EstimateRow:
    benchmark: str
    size: Optional[int]
    source_file: str

    mean: Optional[float]
    mean_lb: Optional[float]
    mean_ub: Optional[float]
    mean_se: Optional[float]

    median: Optional[float]
    median_lb: Optional[float]
    median_ub: Optional[float]
    median_se: Optional[float]

    slope: Optional[float]
    slope_lb: Optional[float]
    slope_ub: Optional[float]
    slope_se: Optional[float]

    std_dev: Optional[float]
    std_dev_lb: Optional[float]
    std_dev_ub: Optional[float]
    std_dev_se: Optional[float]

    mad: Optional[float]
    mad_lb: Optional[float]
    mad_ub: Optional[float]
    mad_se: Optional[float]


def _get(d: dict[str, Any], *path: str) -> Any:
    cur: Any = d
    for p in path:
        if not isinstance(cur, dict) or p not in cur:
            return None
        cur = cur[p]
    return cur


def _parse_benchmark_name(dir_name: str) -> tuple[str, Optional[int]]:
    """
    Heuristic: if the directory ends with digits, treat that as `size`.
    E.g. "mul 128" -> ("mul", 128), "foo_bar_1024" -> ("foo_bar_", 1024)
    If it doesn't match, size is None and benchmark is the full dir name.
    """
    s = dir_name.strip()
    m = SIZE_SUFFIX_RE.match(s.replace(" ", ""))
    if not m:
        return (s, None)

    # Keep the human-readable name by stripping the trailing digits from the original.
    size = int(m.group("size"))
    # Find the last run of digits in the original string and strip it.
    stripped = re.sub(r"\d+\s*$", "", s).rstrip()
    if not stripped:
        stripped = s
    return (stripped, size)


def _load_estimates_json(path: Path) -> dict[str, Any]:
    # Criterion's estimates.json is often a single-line JSON file.
    return json.loads(path.read_text(encoding="utf-8"))


def iter_estimate_rows(criterion_dir: Path) -> Iterable[EstimateRow]:
    for estimates_path in sorted(criterion_dir.glob("**/new/estimates.json")):
        # benchmark directory name is the parent of "new/"
        bench_dir = estimates_path.parent.parent
        bench_raw = bench_dir.name
        bench_name, bench_size = _parse_benchmark_name(bench_raw)

        data = _load_estimates_json(estimates_path)

        def est(key: str, field: str) -> Optional[float]:
            v = _get(data, key, field)
            return float(v) if v is not None else None

        def ci(key: str, field: str) -> Optional[float]:
            v = _get(data, key, "confidence_interval", field)
            return float(v) if v is not None else None

        yield EstimateRow(
            benchmark=bench_name,
            size=bench_size,
            source_file=str(estimates_path),
            mean=est("mean", "point_estimate"),
            mean_lb=ci("mean", "lower_bound"),
            mean_ub=ci("mean", "upper_bound"),
            mean_se=est("mean", "standard_error"),
            median=est("median", "point_estimate"),
            median_lb=ci("median", "lower_bound"),
            median_ub=ci("median", "upper_bound"),
            median_se=est("median", "standard_error"),
            slope=est("slope", "point_estimate"),
            slope_lb=ci("slope", "lower_bound"),
            slope_ub=ci("slope", "upper_bound"),
            slope_se=est("slope", "standard_error"),
            std_dev=est("std_dev", "point_estimate"),
            std_dev_lb=ci("std_dev", "lower_bound"),
            std_dev_ub=ci("std_dev", "upper_bound"),
            std_dev_se=est("std_dev", "standard_error"),
            mad=est("median_abs_dev", "point_estimate"),
            mad_lb=ci("median_abs_dev", "lower_bound"),
            mad_ub=ci("median_abs_dev", "upper_bound"),
            mad_se=est("median_abs_dev", "standard_error"),
        )


def _sort_key(row: EstimateRow) -> tuple[str, int, str]:
    return (row.benchmark, row.size if row.size is not None else -1, row.source_file)


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Convert Criterion estimates.json files into one CSV.")
    p.add_argument(
        "--input",
        required=True,
        help="Path to Criterion output directory (e.g. ./arkworks/target/criterion).",
    )
    p.add_argument(
        "--output",
        default="-",
        help="Output CSV path, or '-' for stdout (default).",
    )
    return p.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    criterion_dir = Path(args.input)
    if not criterion_dir.exists() or not criterion_dir.is_dir():
        print(f"error: --input is not a directory: {criterion_dir}", file=sys.stderr)
        return 2

    rows = sorted(iter_estimate_rows(criterion_dir), key=_sort_key)
    if not rows:
        print(f"error: no estimates found under: {criterion_dir}", file=sys.stderr)
        return 2

    fieldnames = [
        "benchmark",
        "size",
        "source_file",
        "mean",
        "mean_lb",
        "mean_ub",
        "mean_se",
        "median",
        "median_lb",
        "median_ub",
        "median_se",
        "slope",
        "slope_lb",
        "slope_ub",
        "slope_se",
        "std_dev",
        "std_dev_lb",
        "std_dev_ub",
        "std_dev_se",
        "median_abs_dev",
        "median_abs_dev_lb",
        "median_abs_dev_ub",
        "median_abs_dev_se",
    ]

    out_fh = sys.stdout if args.output == "-" else open(args.output, "w", encoding="utf-8", newline="")
    try:
        w = csv.DictWriter(out_fh, fieldnames=fieldnames)
        w.writeheader()
        for r in rows:
            w.writerow(
                {
                    "benchmark": r.benchmark,
                    "size": "" if r.size is None else r.size,
                    "source_file": r.source_file,
                    "mean": r.mean,
                    "mean_lb": r.mean_lb,
                    "mean_ub": r.mean_ub,
                    "mean_se": r.mean_se,
                    "median": r.median,
                    "median_lb": r.median_lb,
                    "median_ub": r.median_ub,
                    "median_se": r.median_se,
                    "slope": r.slope,
                    "slope_lb": r.slope_lb,
                    "slope_ub": r.slope_ub,
                    "slope_se": r.slope_se,
                    "std_dev": r.std_dev,
                    "std_dev_lb": r.std_dev_lb,
                    "std_dev_ub": r.std_dev_ub,
                    "std_dev_se": r.std_dev_se,
                    "median_abs_dev": r.mad,
                    "median_abs_dev_lb": r.mad_lb,
                    "median_abs_dev_ub": r.mad_ub,
                    "median_abs_dev_se": r.mad_se,
                }
            )
    except BrokenPipeError:
        # Common when piping to `head`. Avoid a noisy stack trace.
        return 0
    finally:
        if out_fh is not sys.stdout:
            out_fh.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

