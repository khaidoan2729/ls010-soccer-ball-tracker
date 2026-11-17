#pragma once
// YOLO (Ultralytics v8 / 11 export) ONNX model run on the CPU through OpenCV DNN.
// Adapted from ball-detection/src/main.cpp (detectOnHost).
// Expected output layout: 1 x (4 + classes) x N, rows = cx, cy, w, h, class scores,
// in input-image pixels (e.g. 0..640).
#include <algorithm>
#include <string>
#include <vector>

#include <opencv2/dnn.hpp>

#include "detect/detector.h"

namespace ballcore {

class OnnxDetector : public IDetector {
public:
    // input_size must match the export size (imgsz). The confidence threshold is kept low on
    // purpose: the UI shows the confidence and flags weak (<20%) detections itself.
    explicit OnnxDetector(const std::string& model_path, int input_size = 640,
                          float conf_threshold = 0.10f, float nms_threshold = 0.5f)
        : net_(cv::dnn::readNetFromONNX(model_path)), input_(input_size),
          conf_(conf_threshold), nms_(nms_threshold) {
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }

    std::vector<Detection> detect(const Frame& f) override {
        // The frame is stretched to the square input (no letterbox), as in the ball-detection demo.
        cv::Mat blob = cv::dnn::blobFromImage(f.image, 1.0 / 255.0, cv::Size(input_, input_),
                                              cv::Scalar(), /*swapRB=*/true, /*crop=*/false);
        net_.setInput(blob);
        cv::Mat out = net_.forward();                 // 1 x (4+nc) x N
        cv::Mat pred = out.reshape(1, out.size[1]);   // (4+nc) x N
        cv::Mat rows;
        cv::transpose(pred, rows);                    // N x (4+nc)
        const int nc = rows.cols - 4;

        const float sx = f.image.cols / static_cast<float>(input_);
        const float sy = f.image.rows / static_cast<float>(input_);
        std::vector<cv::Rect> boxes;
        std::vector<float> scores;
        for (int i = 0; i < rows.rows; ++i) {
            const float* r = rows.ptr<float>(i);
            float best = 0;
            for (int c = 0; c < nc; ++c) best = std::max(best, r[4 + c]);
            if (best < conf_) continue;
            boxes.emplace_back(static_cast<int>((r[0] - r[2] / 2) * sx),
                               static_cast<int>((r[1] - r[3] / 2) * sy),
                               static_cast<int>(r[2] * sx), static_cast<int>(r[3] * sy));
            scores.push_back(best);
        }

        std::vector<int> keep;
        cv::dnn::NMSBoxes(boxes, scores, conf_, nms_, keep);
        std::vector<Detection> dets;
        for (int k : keep) dets.push_back({cv::Rect2f(boxes[k]), scores[k]});
        return dets;
    }

private:
    cv::dnn::Net net_;
    int input_;
    float conf_, nms_;
};

}  // namespace ballcore
