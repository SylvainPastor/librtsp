# ROS 2 usage examples

Two complementary bridges between librtsp and ROS 2:

1. **Server**: republish a ROS 2 image topic as an RTSP endpoint
   (`librtsp::AppSrcSource`).
2. **Client**: consume an RTSP stream and publish it as a ROS 2 image topic
   (`librtsp::CallbackSink`).

---

## Server: ROS 2 image topic → RTSP

Bridge a ROS 2 image topic to an RTSP endpoint using `librtsp::AppSrcSource`.

### Overview

The bridge:

1. Creates a `librtsp::Server` listening on port `8554`.
2. Builds an `AppSrcSource` describing the input format (codec + caps).
3. Mounts the source on the endpoint `/camera` via `Server::add_stream`,
   which returns a `librtsp::Stream` handle.
4. Subscribes to `/camera/image_raw` and forwards each incoming
   `sensor_msgs/Image` message into the appsrc with `push_frame`.

While no RTSP client is connected, `push_frame` returns `false` and the
frame is dropped — the expected behavior for a live source.

### Code

```cpp
#include <librtsp/app_src_source.hpp>
#include <librtsp/server.hpp>
#include <librtsp/stream.hpp>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>

#include <memory>

class Ros2RtspBridge : public rclcpp::Node {
 public:
  Ros2RtspBridge()
      : rclcpp::Node("ros2_rtsp_bridge"),
        server_("0.0.0.0", 8554),
        source_(std::make_shared<librtsp::AppSrcSource>(
            librtsp::AppSrcSource::Config{
                .codec = librtsp::Codec::H264,
                .caps  = "video/x-raw,format=BGR,"
                         "width=640,height=480,framerate=30/1",
                .block_on_full = false,
            })) {
    // Mount at rtsp://<host>:8554/camera and keep the handle.
    stream_ = server_.add_stream("/camera", source_);
    server_.start();

    sub_ = create_subscription<sensor_msgs::msg::Image>(
        "/camera/image_raw", rclcpp::SensorDataQoS(),
        [this](sensor_msgs::msg::Image::ConstSharedPtr msg) {
          const auto pts_ns =
              rclcpp::Time(msg->header.stamp).nanoseconds();
          source_->push_frame(msg->data.data(), msg->data.size(), pts_ns);
        });
  }

 private:
  librtsp::Server                                          server_;
  std::shared_ptr<librtsp::AppSrcSource>                   source_;
  std::shared_ptr<librtsp::Stream>                         stream_;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_;
};
```

### Notes

- **Caps must match the topic encoding.** The example assumes
  `sensor_msgs/Image` with `encoding="bgr8"`. Change the `format=` field
  in the caps string (`RGB`, `I420`, `NV12`, ...) and update `width`,
  `height`, `framerate` to match the actual stream.
- **Backpressure.** `push_frame()` returns `false` when no RTSP client is
  connected, or when the appsrc has signaled `enough-data` and
  `block_on_full=false`. Dropped frames are not retried.
- **Removal.** Call `stream_->remove()` (or `server_.remove_stream("/camera")`)
  to unmount. Keep the `Stream` shared_ptr alive until all active sessions
  have closed; the library connects the `unprepared` signal to the Stream.
- **Resolution changes.** The current `AppSrcSource::Config` does not model
  dynamic resolution. To switch resolution, remove the stream and add a new
  one with a fresh `AppSrcSource`.
- **Logging.** To forward `librtsp` logs into `rclcpp` logging, implement a
  `librtsp::Logger` adapter that calls `RCLCPP_INFO`/`WARN`/`ERROR` and
  register it once at startup with `librtsp::set_logger(...)`.

---

## Client: RTSP to ROS 2 image topic

Consume an RTSP stream and republish the decoded frames on a ROS 2 topic
using `librtsp::CallbackSink`.

### Overview

The bridge:

1. Creates a `librtsp::Client` pointed at an RTSP URL.
2. Plugs a `CallbackSink` configured for `BGR` output, the library inserts
   `decodebin ! videoconvert ! video/x-raw,format=BGR ! appsink`.
3. On every decoded frame, the callback fills a `sensor_msgs/Image` and
   publishes it on `/camera/image_raw`.

The callback fires on librtsp's worker thread (the private GMainContext
loop). `rclcpp::Publisher::publish` is thread-safe, so calling it from that
thread is fine.

### Code

```cpp
#include <librtsp/callback_sink.hpp>
#include <librtsp/client.hpp>
#include <librtsp/frame.hpp>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>

#include <memory>
#include <string>

class RtspToRos2 : public rclcpp::Node {
 public:
  RtspToRos2()
      : rclcpp::Node("rtsp_to_ros2") {
    pub_ = create_publisher<sensor_msgs::msg::Image>(
        "/camera/image_raw", rclcpp::SensorDataQoS());

    // CallbackSink defaults: output_format="BGR", max_buffers=1, drop=true.
    sink_ = std::make_shared<librtsp::CallbackSink>(
        librtsp::CallbackSink::Config{},
        [this](const librtsp::Frame& f) { publish_frame(f); });

    librtsp::Client::Config cfg;
    cfg.url        = "rtsp://192.168.0.42:8554/cam";
    cfg.latency_ms = 100;
    cfg.transport  = librtsp::Client::Transport::Auto;

    client_ = std::make_unique<librtsp::Client>(cfg);
    client_->set_sink(sink_);
    client_->start();
  }

  ~RtspToRos2() override {
    if (client_) {
      client_->stop();
    }
  }

 private:
  void publish_frame(const librtsp::Frame& f) {
    sensor_msgs::msg::Image msg;
    msg.header.stamp    = now();
    msg.header.frame_id = "camera";
    msg.width           = f.width;
    msg.height          = f.height;
    msg.encoding        = "bgr8";          // matches CallbackSink::Config::output_format
    msg.is_bigendian    = 0;
    msg.step            = f.width * 3;
    msg.data.assign(f.data, f.data + f.size);   // f.data is valid for this call only
    pub_->publish(std::move(msg));
  }

  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_;
  std::shared_ptr<librtsp::CallbackSink>                sink_;
  std::unique_ptr<librtsp::Client>                      client_;
};
```

### Notes

- **Frame ownership.** Inside the callback, `f.data` points into a mapped
  `GstBuffer` that is unmapped immediately after the callback returns. The
  example copies into `msg.data` (a `std::vector<uint8_t>`). If you need to
  defer publication or process on another thread, call `f.retain()` to get
  a `FrameHandle` that owns its own reference and mapping.
- **Encoding.** `CallbackSink::Config::output_format` and
  `sensor_msgs::msg::Image::encoding` must match (`BGR` ↔ `bgr8`,
  `RGB` ↔ `rgb8`, `GRAY8` ↔ `mono8`, ...). Other formats may need
  conversion by the consumer.
- **Timestamp.** The example uses the node clock (`now()`). The RTP PTS is
  available as `f.pts_ns`, but it is relative to the GStreamer clock origin,
  not necessarily wall time. Pick the one that fits your downstream
  consumers.
- **QoS.** `SensorDataQoS()` (best-effort, history KEEP_LAST, depth 5) is a
  good default for live streams; raise the depth if subscribers fall behind.
- **Reconnects.** If the RTSP server goes away, the GStreamer pipeline
  posts an error on the bus (logged via `librtsp::Logger`). The current
  library does not auto-reconnect — drive that from your node (poll
  `client_->is_started()` or react to your logger and call
  `client_->stop(); client_->start();`).
- **Stream info.** Call `client_->stream_info()` to get the detected codec
  once the first RTP pad is added. Width/height/framerate are present in
  every `Frame` via `f.width` / `f.height`.
