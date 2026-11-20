#!/usr/bin/env bash
set -e

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

extraArgs=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -j)
      if [[ -n "$2" && "$2" =~ ^[0-9]+$ ]]; then
        shift 2
      else
        shift 1
      fi
      ;;
    -j[0-9]*)
      shift 1
      ;;
    *)
      extraArgs+=("$1")
      shift 1
      ;;
  esac
done

if ! command -v yosys >/dev/null 2>&1 && [ -f "${HOME}/oss-cad-suite/environment" ]; then
  source "${HOME}/oss-cad-suite/environment"
fi

cmake -B "${scriptDir}/build" -S "${scriptDir}"
cmake --build "${scriptDir}/build" --target bitstream "${extraArgs[@]}"
