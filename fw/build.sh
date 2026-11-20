#!/usr/bin/env bash
set -e

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
rootDir="$(cd "${scriptDir}/.." && pwd)"

if [ -f "${rootDir}/.venv/bin/activate" ]; then
  source "${rootDir}/.venv/bin/activate"
fi

if [ -z "${ZEPHYR_SDK_INSTALL_DIR}" ] && [ -d "${HOME}/zephyr-sdk-1.0.1" ]; then
  export ZEPHYR_SDK_INSTALL_DIR="${HOME}/zephyr-sdk-1.0.1"
fi

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

west build -b nexrx --shield display_uc1609 -d "${scriptDir}/build" "${scriptDir}" --build-opt="-j${numJobs}" "${extraArgs[@]}"
