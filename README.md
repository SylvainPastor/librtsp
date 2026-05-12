# librtsp

A C++ RTSP client and server library built on top of GStreamer.

`librtsp` wraps the [GStreamer](https://gstreamer.freedesktop.org/) RTSP
stack (`gstreamer-1.0`, `gstreamer-app-1.0`,
`gstreamer-rtsp-server-1.0`) behind a small, modern C++ API so that
applications can publish and consume RTSP streams without dealing
directly with the GObject / GLib boilerplate.

> **Status:** early development. The public API is not stable yet.

## Features

- **RTSP server** with a lifecycle managed in its own thread:
  - configurable listening address and port (default `0.0.0.0:554`)
  - source hierarchy: raw GStreamer launch lines (`GStreamerPipelineSource`),
    synthetic test pattern (`TestSource`), and `AppSrcSource` for pushing
    frames from outside GStreamer (ROS 2 / OpenCV / custom producers)
  - dynamic stream mount/unmount via `Server::add_stream` /
    `remove_stream`
  - client connection / disconnection tracking
  - automatic cleanup of inactive sessions
- **RTSP client** connecting to one URL:
  - configurable URL, jitterbuffer latency, transport (`Auto` / `Tcp` /
    `Udp`)
  - sink hierarchy: raw GStreamer launch lines
    (`GStreamerPipelineSink`) and `CallbackSink` for per-frame delivery
    to user code (with `Frame::retain()` for use beyond the callback)
  - connection state machine (`Idle` / `Connecting` / `Connected` /
    `Error`) with thread-safe `subscribe_state` / `unsubscribe_state`
  - `StreamInfo` polling (codec) and detailed network error capture via
    `Gstreamer::drain_recent_errors()`
- **Common runtime**:
  - private `GMainContext` per `EventLoop` — multiple servers / clients
    coexist in the same process without contending for the
    global-default context
  - pluggable `Logger` sink (bridge to ROS 2 / spdlog / std::cerr) and
    `LIBRTSP_*` stream-style log macros
  - GStreamer init wrapped in a static `Gstreamer` singleton

## Roadmap

Not yet implemented:

- Password-protected access (RTSP Basic / Digest authentication, both
  server and client sides)
- Secure RTSP over TLS (`rtsps://`)

## Object model

Class diagrams of the two halves of the library:

- [`docs/server_uml.md`](docs/server_uml.md): `Server`, `Stream`, the
  `Source` hierarchy (`GStreamerPipelineSource`, `TestSource`,
  `AppSrcSource`) and their collaborators.
- [`docs/client_uml.md`](docs/client_uml.md): `Client`, the `Sink`
  hierarchy (`GStreamerPipelineSink`, `CallbackSink`), and the
  `Frame` / `FrameHandle` pair.

Both are Mermaid class diagrams; GitHub and most IDEs render them inline.
Run `make docs-uml` to export them to SVG (or `UML_OUTPUT_FORMAT=png`).

## Examples

End-to-end demos that show the public API in use:

- [`examples/test_pattern_server.cpp`](examples/test_pattern_server.cpp):
  spins up a `Server` listening on `0.0.0.0:8554` and mounts a
  `TestSource` (videotestsrc => H264) at `/test`.
- [`examples/test_pattern_client.cpp`](examples/test_pattern_client.cpp):
  connects to an RTSP URL, plugs a `CallbackSink` to receive decoded BGR
  frames, prints per-second FPS / state stats, and demonstrates the
  state-change subscription API.

Build and run:

```sh
make build
make run-test-pattern-server                                # terminal 1
make run-test-pattern-client                                # terminal 2
make run-test-pattern-client URL=rtsp://10.0.0.1:8554/test  # custom URL
```

Both targets support `GST_LEVEL=<n>` for GStreamer debug verbosity and
`VALGRIND=1` to run under valgrind.

## ROS 2 integration

Recipes for bridging librtsp with ROS 2 image topics — both directions are
covered:

- [`docs/ros2_usage_example.md`](docs/ros2_usage_example.md)
  - **Server**: republish a `sensor_msgs/Image` topic as an RTSP endpoint
    via `AppSrcSource` (subscribe + `push_frame`).
  - **Client**: consume a remote RTSP stream and publish the decoded
    frames as a `sensor_msgs/Image` topic via `CallbackSink`.
