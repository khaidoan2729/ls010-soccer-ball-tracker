// ball-detection: OAK-D Lite + custom YOLOv8 soccer-ball model.
//
// inferenceOnCamera = true  -> the network runs on the OAK-D Lite (needs a .blob)
// inferenceOnCamera = false -> the camera only streams frames; the network runs
//                              on this computer through OpenCV DNN (needs an .onnx)
//
// Change the flag below and rebuild:
//   cmake --build build --parallel 2 && ./build/ball_detection

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include "depthai/depthai.hpp"

const bool inferenceOnCamera = false;

const std::string kOnnxPath = "models/ball_detection_2/model.onnx";  // host inference
const std::string kBlobPath = "models/ball_detection_2/model.blob";  // camera inference

// Class names in the order the model was trained with. One class: "ball".
const std::vector<std::string> kLabels = {"ball"};

// Network input size. Must equal the imgsz used when exporting BOTH the
const int kInput = 640;

const float kConf = 0.1f;     // confidence threshold
const float kNms = 0.5f;      // IoU threshold for non-max suppression
const int kCameraFps = 15;    // camera frame rate
const int kDisplay = 640;     // size of the window (square)


struct Det {
    float x1, y1, x2, y2;
    float score;
    int id;
};

struct FpsCounter {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    int count = 0;
    float fps = 0.f;
    float tick() {
        count++;
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - start).count();
        if (elapsed > 1.0f) {
            fps = count / elapsed;
            count = 0;
            start = now;
        }
        return fps;
    }
};

static std::string labelFor(int id) {
    return (id >= 0 && id < static_cast<int>(kLabels.size())) ? kLabels[id]
                                                                : std::to_string(id);
}

static void drawOverlay(cv::Mat& img, const std::vector<Det>& dets, float fps,
                        const std::string& mode) {
    for (const auto& d : dets) {
        int x1 = static_cast<int>(d.x1 * img.cols);
        int y1 = static_cast<int>(d.y1 * img.rows);
        int x2 = static_cast<int>(d.x2 * img.cols);
        int y2 = static_cast<int>(d.y2 * img.rows);
        std::ostringstream ss;
        ss << labelFor(d.id) << " " << static_cast<int>(d.score * 100) << "%";
        cv::rectangle(img, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 0), 2);
        cv::putText(img, ss.str(), cv::Point(x1 + 4, std::max(y1 - 6, 14)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    }
    std::ostringstream f;
    f << "FPS: " << std::fixed << std::setprecision(1) << fps << "  [" << mode << "]";
    cv::putText(img, f.str(), cv::Point(10, 25), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 0, 255), 2);
}

// Runs the YOLOv8 ONNX model on the host. Expects the standard Ultralytics
// output layout: 1 x (4 + numClasses) x N, rows = cx, cy, w, h, class scores.
static std::vector<Det> detectOnHost(cv::dnn::Net& net, const cv::Mat& frame) {
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, cv::Size(kInput, kInput),
                                          cv::Scalar(), /*swapRB=*/true, /*crop=*/false);
    net.setInput(blob);
    cv::Mat out = net.forward();

    cv::Mat pred = out.reshape(1, out.size[1]);  // (4+nc) x N
    cv::Mat predT;
    cv::transpose(pred, predT);                  // N x (4+nc)
    const int nc = predT.cols - 4;

    static bool warned = false;
    if (!warned && nc != static_cast<int>(kLabels.size())) {
        std::cerr << "Warning: model has " << nc << " classes but kLabels has "
                  << kLabels.size() << std::endl;
        warned = true;
    }

    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> ids;
    for (int i = 0; i < predT.rows; i++) {
        const float* row = predT.ptr<float>(i);
        cv::Mat cls(1, nc, CV_32F, const_cast<float*>(row + 4));
        double conf;
        cv::Point idx;
        cv::minMaxLoc(cls, nullptr, &conf, nullptr, &idx);
        if (conf < kConf) continue;
        float cx = row[0], cy = row[1], w = row[2], h = row[3];
        boxes.emplace_back(static_cast<int>(cx - w / 2), static_cast<int>(cy - h / 2),
                           static_cast<int>(w), static_cast<int>(h));
        scores.push_back(static_cast<float>(conf));
        ids.push_back(idx.x);
    }

    std::vector<int> keep;
    cv::dnn::NMSBoxes(boxes, scores, kConf, kNms, keep);

    std::vector<Det> dets;
    const float s = static_cast<float>(kInput);
    for (int k : keep) {
        const cv::Rect& b = boxes[k];
        dets.push_back({b.x / s, b.y / s, (b.x + b.width) / s, (b.y + b.height) / s,
                        scores[k], ids[k]});
    }
    return dets;
}

int main() {
    std::cout << "Inference on: " << (inferenceOnCamera ? "OAK-D Lite" : "host (laptop)")
              << std::endl;

    dai::Pipeline pipeline;
    auto cam = pipeline.create<dai::node::ColorCamera>();
    auto xoutRgb = pipeline.create<dai::node::XLinkOut>();
    xoutRgb->setStreamName("rgb");

    cam->setResolution(dai::ColorCameraProperties::SensorResolution::THE_1080_P);
    cam->setPreviewSize(kInput, kInput);
    cam->setPreviewKeepAspectRatio(false);
    cam->setInterleaved(false);
    cam->setColorOrder(dai::ColorCameraProperties::ColorOrder::BGR);
    cam->setFps(kCameraFps);

    cv::dnn::Net net;
    if (inferenceOnCamera) {
        auto nn = pipeline.create<dai::node::YoloDetectionNetwork>();
        auto xoutNn = pipeline.create<dai::node::XLinkOut>();
        xoutNn->setStreamName("detections");

        nn->setBlobPath(kBlobPath);
        nn->setNumClasses(static_cast<int>(kLabels.size()));
        nn->setCoordinateSize(4);
        // YOLOv8 is anchor-free. If the blob's .json (from the converter) lists
        // anchors, use those values instead.
        nn->setAnchors(std::vector<float>{});
        nn->setAnchorMasks(std::map<std::string, std::vector<int>>{});
        nn->setConfidenceThreshold(kConf);
        nn->setIouThreshold(kNms);
        nn->setNumInferenceThreads(2);
        nn->input.setBlocking(false);

        cam->preview.link(nn->input);
        nn->passthrough.link(xoutRgb->input);  // the frame that matches the detections
        nn->out.link(xoutNn->input);
    } else {
        net = cv::dnn::readNetFromONNX(kOnnxPath);
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        cam->preview.link(xoutRgb->input);
    }

    // ---------------- Run ----------------
    try {
        dai::Device device(pipeline, dai::UsbSpeed::HIGH);
        // Host mode keeps only the newest frame; camera mode pairs frames with detections.
        auto qRgb = device.getOutputQueue("rgb", inferenceOnCamera ? 4 : 1, false);
        std::shared_ptr<dai::DataOutputQueue> qDet;
        if (inferenceOnCamera) qDet = device.getOutputQueue("detections", 4, false);

        FpsCounter fpsCounter;
        cv::Mat display;

        while (true) {
            auto inRgb = qRgb->get<dai::ImgFrame>();
            cv::Mat frame = inRgb->getCvFrame();

            std::vector<Det> dets;
            if (inferenceOnCamera) {
                auto inDet = qDet->get<dai::ImgDetections>();
                for (const auto& d : inDet->detections)
                    dets.push_back({d.xmin, d.ymin, d.xmax, d.ymax, d.confidence, static_cast<int>(d.label)});
            } else {
                dets = detectOnHost(net, frame);
            }

            cv::resize(frame, display, cv::Size(kDisplay, kDisplay));
            drawOverlay(display, dets, fpsCounter.tick(),
                        inferenceOnCamera ? "camera" : "host");

            cv::imshow("OAK-D Lite ball detection", display);
            if (cv::waitKey(1) == 'q') break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
