#!/usr/bin/env bash
set -euo pipefail

model="${OFFGRID_OLLAMA_MODEL:-qwen3:4b}"

if ! command -v ollama >/dev/null 2>&1; then
    echo "Ollama is not installed. Install it from https://ollama.com/download" >&2
    exit 1
fi

if ! curl --silent --fail --max-time 2 http://127.0.0.1:11434/api/tags >/dev/null; then
    echo "Ollama is installed but its server is not running."
    echo "Open a second terminal and run: ollama serve"
    exit 2
fi

echo "Installing local model: ${model}"
ollama pull "${model}"
echo "Ready. Launch the Survival Console and choose Ask the local Guide."

