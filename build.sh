#!/usr/bin/env bash
set -e

rootDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -f "${rootDir}/.venv/bin/activate" ]; then
  source "${rootDir}/.venv/bin/activate"
fi

if ! command -v yosys >/dev/null 2>&1 && [ -f "${HOME}/oss-cad-suite/environment" ]; then
  source "${HOME}/oss-cad-suite/environment"
fi

if [ -z "${ZEPHYR_SDK_INSTALL_DIR}" ] && [ -d "${HOME}/zephyr-sdk-1.0.1" ]; then
  export ZEPHYR_SDK_INSTALL_DIR="${HOME}/zephyr-sdk-1.0.1"
fi

numJobs="$(nproc 2>/dev/null || echo 4)"
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

cmake -B "${rootDir}/build" -S "${rootDir}"
cmake --build "${rootDir}/build" -j "${numJobs}" "${extraArgs[@]}"

# Create / refresh convenience symlinks in build/
ln -sf "fw/build/zephyr/zephyr.bin" "${rootDir}/build/zephyr.bin"
ln -sf "fw/build/zephyr/zephyr.elf" "${rootDir}/build/zephyr.elf"
ln -sf "CPLD/build/CPLD.bin" "${rootDir}/build/CPLD.bin"
ln -sf "app/build/nexrx-app" "${rootDir}/build/nexrx-app"
ln -sf "twin/build/nexrx-signalgen" "${rootDir}/build/nexrx-signalgen"
