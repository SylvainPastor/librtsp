# librtsp

A C++ RTSP client and server library built on top of GStreamer.

`librtsp` wraps the [GStreamer](https://gstreamer.freedesktop.org/) RTSP
stack (`gstreamer-1.0`, `gstreamer-app-1.0`,
`gstreamer-rtsp-server-1.0`) behind a small, modern C++ API so that
applications can publish and consume RTSP streams without dealing
directly with the GObject / GLib boilerplate.

> **Status:** early development. The public API is not stable yet.

## Features

- RTSP **server** with a lifecycle managed in its own thread:
  - configurable listening address and port (default `0.0.0.0:554`),
  - dynamic creation of streams from arbitrary GStreamer launch
    pipelines, mounted on user-defined endpoints,
  - client connection / disconnection callbacks,
  - automatic cleanup of inactive sessions.
- RTSP **client** (planned): consume remote RTSP sources through the
  same library surface.
- Internal event loop wrapper (`EventLoop`) that encapsulates the
  GLib main loop required by the GStreamer RTSP server.
