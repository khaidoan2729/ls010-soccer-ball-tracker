# ballcore

Walking skeleton: every stage exists behind an interface, most as stubs, all wired together.

```bash
scripts/setup_pi.sh              # once (apt packages)
scripts/run.sh [/dev/video0] [8080]
```
Then open `http://soccer-rpi-board.local:8080/`.

## Contracts

| File | Interface | Stub | Takes → gives |
|---|---|---|---|
| `src/types.h` | — | — | all data types passed between stages |
| `src/capture/camera.h` | `ICamera` | `OpenCvCamera` (real) | → `cv::Mat` |
| `src/detect/detector.h` | `IDetector` | `StubDetector` (fake sweeping ball) | `Frame` → `vector<Detection>` |
| `src/track/tracker.h` | `ITracker` | `StubTracker` (best detection, 1 s trail) | `vector<Detection>` → `TrackState` |
| `src/control/pan.h` | `IPanController`, `IServo` | `StubPanController`, `NullServo` | `TrackState` → angle (deg) → servo |
| `src/analytics/analytics.h` | `IAnalytics` | `StubAnalytics` (constants) | `TrackState` → `Analytics` |
| `src/record/recorder.h` | `IRecorder` | `StubRecorder` (flag only) | `Frame` + `PipelineState` → disk |
| `src/server/http_server.h` | — | — | `PipelineState` → JSON, `Frame` → MJPEG |

Swap a stub by writing a new class that implements the interface and changing one line in
`main.cpp`. Times are seconds since start; coordinates are frame pixels.

## HTTP API (contract with `UI/app.js`)

| Route | |
|---|---|
| `GET /` | static files from `UI/` |
| `GET /stream` | MJPEG of the latest frame |
| `GET /api/state` | JSON below |
| `GET /api/pan?angle=DEG` / `?auto=1` | manual pan / back to auto |
| `GET /api/record?on=1` / `?on=0` | start / stop recording |

```json
{
  "t": 12.34,
  "camera": {"ok": true, "width": 1280, "height": 720},
  "fps": {"capture": 30.0, "processing": 29.9},
  "ball": {"x": 600, "y": 410, "w": 36, "h": 36, "confidence": 0.82},
  "trail": [{"t": 11.36, "x": 590, "y": 428}, "..."],
  "analytics": {"speed_mps": 0, "height_m": 0, "zone": "middle",
                "zone_time_s": {"defence": 0, "middle": 0, "attack": 0}},
  "pan": {"angle_deg": 4.5, "manual": false},
  "recording": false
}
```
`ball` is `null` when not visible.
