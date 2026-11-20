#!/usr/bin/env bash
set -e

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

numJobs=1
extraArgs=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -j)
      if [[ -n "$2" && "$2" =~ ^[0-9]+$ ]]; then
        numJobs="$2"
        shift 2
      else
        numJobs="$(nproc 2>/dev/null || echo 4)"
        shift 1
      fi
      ;;
    -j[0-9]*)
      numJobs="${1#-j}"
      shift 1
      ;;
    *)
      extraArgs+=("$1")
      shift 1
      ;;
  esac
done

cmake -B "${scriptDir}/build" -S "${scriptDir}"
cmake --build "${scriptDir}/build" -j "${numJobs}" "${extraArgs[@]}"
