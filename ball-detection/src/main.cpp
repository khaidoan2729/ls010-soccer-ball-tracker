#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include "depthai/depthai.hpp"

static const std::vector<std::string> kClasses = {
    "person","bicycle","car","motorcycle","airplane","bus","train","truck","boat",
    "traffic light","fire hydrant","stop sign","parking meter","bench","bird","cat",
    "dog","horse","sheep","cow","elephant","bear","zebra","giraffe","backpack",
    "umbrella","handbag","tie","suitcase","frisbee","skis","snowboard","sports ball",
    "kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket",
    "bottle","wine glass","cup","fork","knife","spoon","bowl","banana","apple",
    "sandwich","orange","broccoli","carrot","hot dog","pizza","donut","cake","chair",
    "couch","potted plant","bed","dining table","toilet","tv","laptop","mouse",
    "remote","keyboard","cell phone","microwave","oven","toaster","sink",
    "refrigerator","book","clock","vase","scissors","teddy bear","hair drier",
    "toothbrush"};

int main(int argc, char** argv) {
    std::string modelPath = argc > 1 ? argv[1] : "models/yolov8n.onnx";
    const int kInput = 320;            // must match the imgsz used for export
    const float kConf = 0.4f, kNms = 0.5f;

    cv::dnn::Net net = cv::dnn::readNetFromONNX(modelPath);
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    // Camera only: no neural network on the device
    dai::Pipeline pipeline;
    auto cam = pipeline.create<dai::node::ColorCamera>();
    auto xout = pipeline.create<dai::node::XLinkOut>();
    xout->setStreamName("rgb");
    cam->setPreviewSize(640, 480);
    cam->setInterleaved(false);
    cam->setColorOrder(dai::ColorCameraProperties::ColorOrder::BGR);
    cam->setFps(20);
    cam->preview.link(xout->input);

    try {
        dai::Device device(pipeline, dai::UsbSpeed::HIGH);
        auto q = device.getOutputQueue("rgb", 1, false);  // keep only the newest frame

        auto start = std::chrono::steady_clock::now();
        int counter = 0;
        float fps = 0.f;

        while (true) {
            cv::Mat frame = q->get<dai::ImgFrame>()->getCvFrame();

            // Inference on the host (frame is stretched to the square input)
            cv::Mat blob = cv::dnn::blobFromImage(
                frame, 1.0 / 255.0, cv::Size(kInput, kInput), cv::Scalar(), true, false);
            net.setInput(blob);
            cv::Mat out = net.forward();               // 1 x 84 x N

            cv::Mat pred = out.reshape(1, out.size[1]); // 84 x N
            cv::transpose(pred, pred);                  // N x 84

            std::vector<cv::Rect> boxes;
            std::vector<float> scores;
            std::vector<int> ids;
            float sx = frame.cols / static_cast<float>(kInput);
            float sy = frame.rows / static_cast<float>(kInput);

            for (int i = 0; i < pred.rows; i++) {
                const float* row = pred.ptr<float>(i);
                cv::Mat cls(1, 80, CV_32F, const_cast<float*>(row + 4));
                double conf; cv::Point idx;
                cv::minMaxLoc(cls, nullptr, &conf, nullptr, &idx);
                if (conf < kConf) continue;
                float cx = row[0], cy = row[1], w = row[2], h = row[3];
                boxes.emplace_back(static_cast<int>((cx - w / 2) * sx),
                                   static_cast<int>((cy - h / 2) * sy),
                                   static_cast<int>(w * sx), static_cast<int>(h * sy));
                scores.push_back(static_cast<float>(conf));
                ids.push_back(idx.x);
            }

            std::vector<int> keep;
            cv::dnn::NMSBoxes(boxes, scores, kConf, kNms, keep);
            for (int k : keep) {
                std::ostringstream ss;
                ss << kClasses[ids[k]] << " " << static_cast<int>(scores[k] * 100) << "%";
                cv::rectangle(frame, boxes[k], cv::Scalar(0, 255, 0), 2);
                cv::putText(frame, ss.str(), cv::Point(boxes[k].x + 4, std::max(boxes[k].y - 6, 14)),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
            }

            counter++;
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - start).count();
            if (elapsed > 1.0f) { fps = counter / elapsed; counter = 0; start = now; }
            std::ostringstream f;
            f << "FPS: " << std::fixed << std::setprecision(1) << fps;
            cv::putText(frame, f.str(), cv::Point(10, 25),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);

            cv::imshow("OAK-D Lite (host inference)", frame);
            if (cv::waitKey(1) == 'q') break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
