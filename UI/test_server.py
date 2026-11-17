# Lightweight TEST server for working on the web page without the Pi / ballcore.
# Serves this folder (index.html, app.js, styles.css) and streams the local webcam at /stream.
# It does not provide /api/state, so the status label shows "Server offline" and there is no
# overlay. The real server is ballcore (core/), which serves these same files.
from flask import Flask, Response
import cv2

app = Flask(__name__, static_folder='.', static_url_path='')

camera = cv2.VideoCapture(0)

def generate_frames():
    while True:
        success, frame = camera.read()

        if not success:
            break

        _, buffer = cv2.imencode('.jpg', frame)
        frame = buffer.tobytes()

        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

@app.route('/')
def index():
    return app.send_static_file('index.html')

@app.route('/stream')
def stream():
    return Response(
        generate_frames(),
        mimetype='multipart/x-mixed-replace; boundary=frame'
    )

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8000, threaded=True)
