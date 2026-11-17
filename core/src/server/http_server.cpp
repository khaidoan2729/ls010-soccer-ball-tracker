#include "server/http_server.h"

#include <iostream>

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <opencv2/imgcodecs.hpp>

namespace ballcore {

// ---- /api/state wire format (contract with UI/app.js; see core/README.md) ----
static nlohmann::json to_json(const PipelineState& s) {
    nlohmann::json trail = nlohmann::json::array();
    for (const auto& p : s.track.trail) trail.push_back({{"t", p.t}, {"x", p.x}, {"y", p.y}});

    nlohmann::json ball = nullptr;
    if (s.track.visible) {
        const auto& b = s.track.ball.box;
        ball = {{"x", b.x}, {"y", b.y}, {"w", b.width}, {"h", b.height},
                {"confidence", s.track.ball.confidence}};
    }
    return {
        {"t", s.t},
        {"camera", {{"ok", s.camera_ok}, {"width", s.frame_width}, {"height", s.frame_height}}},
        {"fps", {{"capture", s.capture_fps}, {"processing", s.processing_fps}}},
        {"ball", ball},
        {"trail", trail},
        {"analytics", {{"speed_mps", s.analytics.speed_mps},
                       {"height_m", s.analytics.height_m},
                       {"zone", s.analytics.zone},
                       {"zone_time_s", s.analytics.zone_time_s}}},
        {"pan", {{"angle_deg", s.pan_deg}, {"manual", s.pan_manual}}},
        {"recording", s.recording},
    };
}

HttpServer::HttpServer(std::string web_root, Latest<Frame>& frames, ServerHooks hooks)
    : web_root_(std::move(web_root)), frames_(frames), hooks_(std::move(hooks)),
      srv_(std::make_unique<httplib::Server>()) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start(const std::string& host, int port) {
    auto& srv = *srv_;
    if (!srv.set_mount_point("/", web_root_))
        std::cerr << "warning: web root not found: " << web_root_ << "\n";

    auto reply = [](httplib::Response& res, const nlohmann::json& j) {
        res.set_header("Cache-Control", "no-store");
        res.set_content(j.dump(), "application/json");
    };

    srv.Get("/api/state", [this, reply](const httplib::Request&, httplib::Response& res) {
        reply(res, to_json(hooks_.state()));
    });

    srv.Get("/api/pan", [this, reply](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("angle")) hooks_.set_pan(std::stod(req.get_param_value("angle")));
        else hooks_.set_pan(std::nullopt);
        reply(res, to_json(hooks_.state())["pan"]);
    });

    srv.Get("/api/record", [this, reply](const httplib::Request& req, httplib::Response& res) {
        bool on = hooks_.set_recording(req.get_param_value("on") == "1");
        reply(res, {{"recording", on}});
    });

    // MJPEG: each viewer encodes the newest frame in its own server thread, so a slow viewer
    // only drops its own frames. (Optimisation for later: encode once, share between viewers.)
    srv.Get("/stream", [this](const httplib::Request&, httplib::Response& res) {
        res.set_chunked_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [this, seq = uint64_t{0}](size_t, httplib::DataSink& sink) mutable {
                Frame f;
                while (running_) {
                    if (!frames_.wait_newer(seq, f, 0.5)) continue;
                    std::vector<unsigned char> jpg;
                    cv::imencode(".jpg", f.image, jpg, {cv::IMWRITE_JPEG_QUALITY, 80});
                    std::string head = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                       std::to_string(jpg.size()) + "\r\n\r\n";
                    return sink.write(head.data(), head.size()) &&
                           sink.write(reinterpret_cast<const char*>(jpg.data()), jpg.size()) &&
                           sink.write("\r\n", 2);
                }
                return false;
            });
    });

    if (!srv.bind_to_port(host.c_str(), port)) {
        std::cerr << "error: cannot bind " << host << ":" << port << "\n";
        return false;
    }
    running_ = true;
    thread_ = std::thread([this] { srv_->listen_after_bind(); });
    return true;
}

void HttpServer::stop() {
    if (!running_.exchange(false)) return;
    srv_->stop();
    if (thread_.joinable()) thread_.join();
}

}  // namespace ballcore
