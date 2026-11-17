#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "latest.h"
#include "types.h"

namespace httplib { class Server; }

namespace ballcore {

// What the server may ask of the rest of the program. Keeps the server decoupled from stages.
struct ServerHooks {
    std::function<PipelineState()> state;
    std::function<void(std::optional<double>)> set_pan;  // angle = manual, nullopt = auto
    std::function<bool(bool)> set_recording;              // returns new recording state
};

// GET /            static files (UI/)
// GET /stream      MJPEG of the latest frame
// GET /api/state   PipelineState as JSON
// GET /api/pan?angle=DEG | ?auto=1
// GET /api/record?on=1 | ?on=0
class HttpServer {
public:
    HttpServer(std::string web_root, Latest<Frame>& frames, ServerHooks hooks);
    ~HttpServer();
    bool start(const std::string& host, int port);
    void stop();

private:
    std::string web_root_;
    Latest<Frame>& frames_;
    ServerHooks hooks_;
    std::unique_ptr<httplib::Server> srv_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace ballcore
