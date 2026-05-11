#!/usr/bin/env bash
# View the RTSP stream served by test_pattern_server and report frame rate.
#
# Uses gst-launch-1.0 with decodebin (auto H264/H265) and fpsdisplaysink.
# fpsdisplaysink periodically updates its "last-message" property with
# rendered/dropped/current/average FPS, which gst-launch prints via -v.
#
# Examples:
#   ./scripts/view_test_pattern.sh
#   HOST=10.0.0.1 PORT=8555 ./scripts/view_test_pattern.sh
#   HEADLESS=1 ./scripts/view_test_pattern.sh
#   VERBOSE=1 ./scripts/view_test_pattern.sh

set -euo pipefail

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8554}
ENDPOINT=${ENDPOINT:-/test}
HEADLESS=${HEADLESS:-0}
LATENCY_MS=${LATENCY_MS:-100}
VERBOSE=${VERBOSE:-0}

URL="rtsp://${HOST}:${PORT}${ENDPOINT}"

usage() {
    cat <<EOF
Usage: $0 [-h|--help]

View the test_pattern_server RTSP stream and report frame rate.

Environment variables:
  HOST=<addr>         Server address          (default: 127.0.0.1)
  PORT=<port>         Server port             (default: 8554)
  ENDPOINT=<path>     Stream path             (default: /test)
  LATENCY_MS=<ms>     RTSP jitterbuffer in ms (default: 100)
  HEADLESS=1          Drop the video window, FPS only
  VERBOSE=1           Don't filter gst-launch output
EOF
}

case "${1:-}" in
    -h|--help) usage; exit 0 ;;
esac

if ! command -v gst-launch-1.0 >/dev/null; then
    echo "Error: gst-launch-1.0 not found. Install gstreamer1.0-tools." >&2
    exit 1
fi

if [[ "${HEADLESS}" == "1" ]]; then
    VIDEO_SINK="fakesink"
    TEXT_OVERLAY="false"
else
    VIDEO_SINK="autovideosink"
    TEXT_OVERLAY="true"
fi

echo "Connecting to ${URL}"

run_pipeline() {
    gst-launch-1.0 -v \
        rtspsrc location="${URL}" latency="${LATENCY_MS}" ! \
        decodebin ! \
        videoconvert ! \
        fpsdisplaysink \
            video-sink="${VIDEO_SINK}" \
            text-overlay="${TEXT_OVERLAY}" \
            sync=false
}

if [[ "${VERBOSE}" == "1" ]]; then
    run_pipeline
else
    # Keep just the lines that matter: the periodic FPS update plus any
    # warnings / errors / EOS notifications.
    run_pipeline 2>&1 \
        | grep --line-buffered -E "last-message|EOS|ERROR|WARNING"
fi
