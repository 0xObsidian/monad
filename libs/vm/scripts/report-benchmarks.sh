#!/usr/bin/env bash
set -euxo pipefail

executable="$1"
shift

output_file="$1"
shift

"$executable"                                   \
  --benchmark_out="$output_file"                \
  --benchmark_out_format=json                   \
  --benchmark_enable_random_interleaving=true 