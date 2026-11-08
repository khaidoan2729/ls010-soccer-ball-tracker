# ball-detection

Real-time object detection on an **OAK-D Lite** camera using C++, DepthAI (v2 API) and OpenCV.
The camera only captures and streams frames over USB; the neural network (YOLOv8n, ONNX) runs on
the host laptop through OpenCV's DNN module. Detections are drawn as bounding boxes with an FPS
counter.

Tested on: Intel Mac, macOS 12 (Monterey), AppleClang 14.

Everything lives inside this folder. Dependencies are cloned and built into `third_party/`, so
nothing is installed system-wide except CMake.

## Layout

```
ball-detection/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── src/
│   └── main.cpp
├── models/                  # yolov8n.onnx (not committed)
└── third_party/             # built dependencies (not committed)
    ├── depthai-core/
    ├── depthai-install/
    ├── opencv/
    └── opencv-install/
```

## 1. Prerequisites

1. **Command Line Tools** (full Xcode is not needed):
   ```bash
   xcode-select --install
   xcode-select -p        # should print /Library/Developer/CommandLineTools
   ```
   If `xcode-select -p` points at an `Xcode.app` (for example one in the Trash), switch it back:
   ```bash
   sudo xcode-select --switch /Library/Developer/CommandLineTools
   ```
   Also check that `echo $DEVELOPER_DIR` prints nothing.

2. **CMake** and **git**. Homebrew no longer provides prebuilt packages for macOS 12, so use
   either the `.dmg` from cmake.org or MacPorts (`sudo port install cmake`).
   ```bash
   cmake --version
   git --version
   ```

3. **A good USB cable.** Use the cable that came with the camera, plugged directly into the Mac
   (no hub or adapter if you can avoid it).

You do **not** need the MacPorts or Homebrew OpenCV. The MacPorts `opencv4` package is built
without Protobuf and cannot load `.onnx` files, so we build our own below.

## 2. Build OpenCV (with ONNX/DNN support)

Run from the `ball-detection` folder:

```bash
mkdir -p third_party models
git clone --depth 1 --branch 4.9.0 https://github.com/opencv/opencv.git third_party/opencv

cmake -S third_party/opencv -B third_party/opencv/build \
  -D CMAKE_BUILD_TYPE=Release \
  -D CMAKE_INSTALL_PREFIX=$PWD/third_party/opencv-install \
  -D BUILD_LIST=core,imgproc,imgcodecs,videoio,highgui,dnn \
  -D WITH_PROTOBUF=ON -D BUILD_PROTOBUF=ON \
  -D BUILD_EXAMPLES=OFF -D BUILD_TESTS=OFF -D BUILD_PERF_TESTS=OFF \
  -D BUILD_opencv_apps=OFF -D BUILD_SHARED_LIBS=ON

cmake --build third_party/opencv/build --parallel 2
cmake --build third_party/opencv/build --target install
```

In the configure output, check that Protobuf is listed as built (not missing).

## 3. Build depthai-core

depthai-core must be built and installed on its own (it cannot be pulled in with
`add_subdirectory` because of how its Hunter dependency manager works). It must be built against
the OpenCV from step 2 so the program only ever uses one OpenCV.

```bash
git clone --recursive https://github.com/luxonis/depthai-core.git third_party/depthai-core
cd third_party/depthai-core
git checkout v2_stable            # the code in src/main.cpp uses the v2 API; confirm the branch exists
git submodule update --init --recursive

cmake -S . -B build \
  -D CMAKE_INSTALL_PREFIX=$PWD/../depthai-install \
  -D OpenCV_DIR=$PWD/../opencv-install/lib/cmake/opencv4
cmake --build build --parallel 2
cmake --build build --target install
cd ../..
```

This is the slowest step. The first configure downloads many dependencies through Hunter.
Use a low `--parallel` value: a bare `--parallel` can run the machine out of memory. If it still
runs out, use `--parallel 1`. If a build is interrupted, run the same command again.

## 4. Get the model

The model is YOLOv8n exported to ONNX at 320x320. Exporting locally with `pip` fails on Intel
macOS 12 (the `onnx` package tries to compile from source and fails), so export it in
[Google Colab](https://colab.research.google.com):

```python
!pip install ultralytics onnx
!yolo export model=yolov8n.pt format=onnx imgsz=320 opset=12
```

Download `yolov8n.onnx` from the Colab file browser and place it at `models/yolov8n.onnx`:

```bash
mv ~/Downloads/yolov8n.onnx models/
ls -lh models/yolov8n.onnx
```

The `kInput` constant in `src/main.cpp` must match the export size (`imgsz`).

## 5. Build the program

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.10)
project(ball_detection)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(depthai CONFIG REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(ball_detection src/main.cpp)
target_include_directories(ball_detection PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ball_detection PRIVATE depthai::opencv ${OpenCV_LIBS})
```

`.gitignore`:

```
build/
third_party/
models/
.venv/
```

Configure and build (from `ball-detection`):

```bash
cmake -S . -B build \
  -D depthai_DIR=$PWD/third_party/depthai-install/lib/cmake/depthai \
  -D OpenCV_DIR=$PWD/third_party/opencv-install/lib/cmake/opencv4
cmake --build build --parallel 2
```

## 6. Run

Plug in the OAK-D Lite, then:

```bash
./build/ball_detection
```

You should see the live camera feed with green boxes and labels on detected objects, and the FPS
in red at the top left. Press `q` to quit. A different model can be passed as the first argument:
`./build/ball_detection path/to/model.onnx`.

### Day-to-day workflow

After editing `src/main.cpp`:

```bash
cmake --build build --parallel 2 && ./build/ball_detection
```

Re-run the full configure command from step 5 only when you change `CMakeLists.txt` or add new
source files.

## Troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| `Your Xcode (...Trash/Xcode.app) is too outdated` | `xcode-select` points at an old Xcode. Run `sudo xcode-select --switch /Library/Developer/CommandLineTools` and check `echo $DEVELOPER_DIR` is empty. |
| `HunterGate *before* 'project' command` | depthai-core was added with `add_subdirectory`. Build and install it separately (step 3) and use `find_package`. |
| Undefined symbols `cv::imshow` / `cv::waitKey` | The `highgui` module is not linked. Use `find_package(OpenCV)` and link `${OpenCV_LIBS}` as in step 5. |
| `Build OpenCV with Protobuf to import ONNX models` | The OpenCV in use was built without Protobuf. Use the OpenCV from step 2 and make sure both depthai-core and this program were built against it. |
| `Cannot find any device with given deviceInfo` | USB detection. Unplug and replug the camera, use a direct port and a good data cable, and make sure no other process is using it (`pkill ball_detection`). |
| `X_LINK_ERROR` / `Device crashed` after a few seconds | USB or power instability. Keep `dai::UsbSpeed::HIGH` in the code, avoid hubs, and lower the camera FPS. Running the network on the camera (instead of the host) draws more power and crashed the camera at 15 FPS in testing. |
| Build freezes or runs out of memory | Too many parallel compile jobs. Use `--parallel 1` or `--parallel 2`, and quit other apps. |
| `pip install onnx` fails with `to_chars` errors | Intel macOS 12 has no prebuilt wheel and the SDK cannot compile it. Export the model in Colab (step 4). |
| `dyld: Library not loaded: libopencv_...dylib` | The program cannot find the shared OpenCV libraries at run time. The run path needs to point to `third_party/opencv-install/lib`. |

## Notes

- COCO-pretrained YOLOv8n has a `sports ball` class, but a ball in a wide shot is small. For the
  real soccer use case, fine-tune a model on soccer-ball images and use a larger input size.
- The FPS counter shows the end-to-end loop rate on the host. If it is below the camera FPS, the
  CPU is the bottleneck: reduce the export size (for example `imgsz=256`, and update `kInput`) or
  move inference to a separate thread.