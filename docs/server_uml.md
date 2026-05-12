# Server UML

Class diagram of the RTSP `Server` and its collaborators inside the
`librtsp` namespace. `Server` uses the PIMPL pattern, its public header
exposes only the API and a `unique_ptr<Impl>`; all runtime state lives in
the nested `Server::Impl` defined in the .cpp file. Both classes are
shown so the ownership graph is readable.

```mermaid
classDiagram
    direction LR

    class Server {
        -unique_ptr~Impl~ impl_
        +Server()
        +Server(address, port)
        +start() bool
        +stop()
        +is_started() bool
        +add_stream(endpoint, source) shared_ptr~Stream~
        +remove_stream(endpoint) bool
    }

    class Impl {
        <<Server::Impl>>
        -Server* outer_
        -uint16_t port_
        -string address_
        -GstRTSPServer* server_
        -unique_ptr~EventLoop~ loop_
        -unique_ptr~thread~ thread_
        -atomic~bool~ running_
        -guint source_id_
        -Timer cleanup_timer_
        -mutex client_mutex_
        -uint8_t client_count_
        -mutex streams_mutex_
        -Map~string, Stream~ streams_
        -on_new_client_connected(client)
        -on_client_disconnected(client)
    }

    class Stream {
        -GstRTSPServer* gst_server_
        -Server* owner_
        -string endpoint_
        -shared_ptr~Source~ source_
        -atomic~bool~ mounted_
        +endpoint() string
        +source() shared_ptr~Source~
        +is_active() bool
        +remove()
    }

    class Source {
        <<abstract>>
        +pipeline() string
        +on_media_configured(bin)
        +on_media_destroyed()
    }

    class GStreamerPipelineSource {
        -string launch_
        +pipeline() string
    }

    class TestSource {
        -Codec codec_
        +pipeline() string
    }

    class AppSrcSource {
        -Config cfg_
        -string element_name_
        -GstElement* appsrc_
        -atomic~bool~ enough_data_
        +pipeline() string
        +push_frame(data, size, pts_ns) bool
        +on_media_configured(bin)
        +on_media_destroyed()
    }

    class EventLoop {
        -GMainContext* ctx_
        -GMainLoop* loop_
        +loop()
        +quit()
        +context() GMainContext*
        +create_timeout_ms(ms, cb) Timer
        +create_timeout_s(s, cb) Timer
    }

    class Timer {
        -GSource* src_
        +is_running() bool
        +stop()
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

    %% PIMPL
    Server "1" *-- "1" Impl : pImpl

    %% Composition / aggregation (Impl holds the runtime state)
    Impl "1" *-- "1" EventLoop : owns
    Impl "1" *-- "1" Timer : cleanup
    Impl "1" o-- "*" Stream : tracks

    Stream "*" o-- "1" Source : feeds
    Stream --> Server : back-ref (owner_)

    EventLoop ..> Timer : factory

    %% Inheritance
    Source <|-- GStreamerPipelineSource
    Source <|-- TestSource
    Source <|-- AppSrcSource

    %% Dependencies
    Impl ..> Gstreamer : init() lazily
    Impl ..> Logger : LIBRTSP_INFO/WARN/...
    Stream ..> Logger : LIBRTSP_INFO/WARN/...
    AppSrcSource ..> Logger : LIBRTSP_INFO/WARN/...
```

## Relationships at a glance

- **Server holds a unique_ptr to its Impl**: the public header drops all
  GStreamer / threading / EventLoop / Timer includes. Public methods are
  one-line forwarders to `impl_->…`.
- **Impl owns an EventLoop**: each Server has its own `GMainContext` so
  several servers can coexist without contending for the global-default
  context.
- **Impl owns a cleanup Timer**: periodic `gst_rtsp_session_pool_cleanup`,
  scheduled on the server's own EventLoop context.
- **Impl tracks Streams**: `streams_` keeps a shared_ptr per mounted
  endpoint. The user also gets a shared_ptr from `Server::add_stream`.
- **Stream points back at Server** via `owner_` (the *outer* class, not
  Impl), so `Stream::remove()` calls `Server::detach_stream`, which simply
  forwards to `impl_->detach_stream`.
- **Stream owns a Source** via shared_ptr; the user typically also keeps a
  shared_ptr to the Source to push frames into it (AppSrcSource case).
- **Source is abstract**: `GStreamerPipelineSource`, `TestSource`, and
  `AppSrcSource` are the concrete subclasses shipped today.
- **Gstreamer is a static-only singleton** initialized lazily by Impl or
  explicitly from `main()`.
- **Logger is an abstract sink** registered once via `set_logger(...)`. The
  library reaches it through the `LIBRTSP_*` macros; nothing in the diagram
  *holds* a Logger pointer, they all look it up dynamically.

## Notation

| Arrow | Meaning |
|---|---|
| `A *-- B` | A owns B by composition (B destroyed with A) |
| `A o-- B` | A aggregates B (shared, B may outlive A) |
| `A <\|-- B` | B inherits from A |
| `A ..> B` | A depends on B (uses, doesn't own) |
| `A --> B` | A holds a reference to B |
