# Client UML

Class diagram of the RTSP `Client` and its collaborators inside the
`librtsp` namespace. Only the public surface and the major private members
are shown; minor helpers and accessors are omitted for readability.

```mermaid
classDiagram
    direction LR

    class Client {
        -unique_ptr~Impl~ impl_
        +Client()
        +Client(Config)
        +set_sink(sink)
        +start() bool
        +stop()
        +is_started() bool
        +stream_info() optional~StreamInfo~
    }

    class Config {
        <<struct>>
        +string url
        +uint32_t latency_ms
        +Transport transport
    }

    class Transport {
        <<enumeration>>
        Auto
        Tcp
        Udp
    }

    class StreamInfo {
        <<struct>>
        +string codec
        +int width
        +int height
        +double framerate
    }

    class Sink {
        <<abstract>>
        +pipeline() string
        +on_media_configured(bin)
        +on_media_destroyed()
    }

    class GStreamerPipelineSink {
        -string launch_
        +pipeline() string
    }

    class CallbackSink {
        -Config cfg_
        -Callback cb_
        -string element_name_
        -GstElement* appsink_
        +pipeline() string
        +on_media_configured(bin)
        +on_media_destroyed()
    }

    class Frame {
        <<struct>>
        +data: const uint8_t*
        +size: size_t
        +width: int
        +height: int
        +format: string
        +pts_ns: int64_t
        +retain() FrameHandle
    }

    class FrameHandle {
        -GstSample* sample_
        -GstMapInfo map_
        -bool mapped_
        +valid() bool
        +data() const uint8_t*
        +size() size_t
        +width() int
        +height() int
        +format() const string&
        +pts_ns() int64_t
    }

    class EventLoop {
        -GMainContext* ctx_
        -GMainLoop* loop_
        +loop()
        +quit()
        +context() GMainContext*
    }

    class Gstreamer {
        <<static>>
        +init()
        +init(argc, argv)
        +is_initialized() bool
    }

    class Logger {
        <<abstract>>
        +log(level, file, line, message)
    }

    %% Composition / aggregation
    Client "1" *-- "1" EventLoop : owns (in Impl)
    Client "1" o-- "1" Sink : feeds

    %% Nested / used value types
    Client ..> Config : value
    Client ..> Transport : value
    Client ..> StreamInfo : produces

    %% Sink hierarchy
    Sink <|-- GStreamerPipelineSink
    Sink <|-- CallbackSink

    %% Frame flow
    CallbackSink ..> Frame : delivers per sample
    Frame ..> FrameHandle : retain()

    %% Dependencies
    Client ..> Gstreamer : init() lazily
    Client ..> Logger : LIBRTSP_INFO/WARN/...
    CallbackSink ..> Logger : LIBRTSP_INFO/WARN/...
```

## Relationships at a glance

- **Client owns an EventLoop** (inside its `Impl`): each Client has its own
  private `GMainContext` so several clients can coexist without contending
  for the global-default context. A dedicated worker thread iterates that
  loop while the client is started.
- **Client holds a Sink** by `shared_ptr`: the user constructs the Sink,
  hands it to `Client::set_sink`, and may retain a reference (typical for
  `CallbackSink`, where the lambda captures state the user wants to keep
  around).
- **Sink is abstract**: `GStreamerPipelineSink` (raw launch passthrough)
  and `CallbackSink` (decoded frames to a user lambda) are the concrete
  subclasses shipped today.
- **CallbackSink delivers Frames** per appsink sample. A `Frame` is a
  short-lived view valid only during the user callback.
- **`Frame::retain()` upgrades to `FrameHandle`** — the FrameHandle owns a
  ref on the underlying `GstSample` and keeps its own buffer mapping, so
  the bytes stay valid until the handle is destroyed.
- **Gstreamer is a static-only singleton** initialized lazily by Client or
  explicitly from `main()`.
- **Logger is an abstract sink** registered once via `set_logger(...)`. The
  library reaches it through the `LIBRTSP_*` macros; nothing in the diagram
  *holds* a Logger pointer — they all look it up dynamically.

## Frame lifecycle

```
appsink "new-sample" signal
    │
    └─> on_new_sample_cb pulls GstSample
            │
            └─> CallbackSink::dispatch(sample)
                    │
                    ├─> map buffer, build Frame view
                    ├─> cb_(frame)                      <= user callback runs here
                    │       │
                    │       └─ optional: frame.retain() => FrameHandle
                    └─> unmap buffer, sample unrefd (unless retained)
```

## Notation

| Arrow | Meaning |
|---|---|
| `A *-- B` | A owns B by composition (B destroyed with A) |
| `A o-- B` | A aggregates B (shared, B may outlive A) |
| `A <\|-- B` | B inherits from A |
| `A ..> B` | A depends on B (uses, doesn't own) |
