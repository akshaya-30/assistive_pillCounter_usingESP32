from flask import Flask, request, render_template_string, send_from_directory
import cv2
import os
import uuid
import time
from edge_impulse_linux.image import ImageImpulseRunner

# ================= CONFIG =================
MODEL_PATH = "/home/akshaya/pill_detector/model.eim"   # make sure this file exists and is executable
UPLOAD_DIR = "uploads"
CONF_THRESHOLD = 0.6

os.makedirs(UPLOAD_DIR, exist_ok=True)

app = Flask(__name__)

runner = ImageImpulseRunner(MODEL_PATH)
model_info = runner.init()
print("Model info loaded")

INPUT_W = model_info["model_parameters"]["image_input_width"]
INPUT_H = model_info["model_parameters"]["image_input_height"]

# ================= HTML =================
HTML_PAGE = """
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Pill Counter</title>

<style>
body {
    font-family: Arial, sans-serif;
    background: #f0f2f5;
    text-align: center;
}

.card {
    background: white;
    max-width: 420px;
    margin: 30px auto;
    padding: 20px;
    border-radius: 14px;
    box-shadow: 0 10px 25px rgba(0,0,0,0.15);
}

canvas {
    position: absolute;
    top: 0;
    left: 0;
}

.img-wrapper {
    position: relative;
    display: inline-block;
    margin-top: 15px;
}
</style>
</head>

<body>
<div class="card">
<h2>💊 Assistive Pill Counter</h2>
<p>Edge Impulse • Constrained Object Detection</p>

<form method="POST" action="/predict" enctype="multipart/form-data">
<input type="file" name="image" accept="image/*" capture="environment" required>
<br><br>
<button type="submit">Count Pills</button>
</form>

{% if count is not none %}
<h3>Pills detected: {{ count }}</h3>

<div class="img-wrapper">
    <img id="img" src="/{{ image_path }}" width="320">
    <canvas id="canvas"></canvas>
</div>

<script>
const boxes = {{ boxes | tojson }};
const img = document.getElementById("img");
const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");

img.onload = () => {
    canvas.width = img.width;
    canvas.height = img.height;

    const scaleX = img.width / 96;
    const scaleY = img.height / 96;

    ctx.strokeStyle = "lime";
    ctx.lineWidth = 2;
    ctx.font = "14px Arial";
    ctx.fillStyle = "lime";

    boxes.forEach(b => {
        const x = b.x * scaleX;
        const y = b.y * scaleY;
        const w = b.w * scaleX;
        const h = b.h * scaleY;

        ctx.strokeRect(x, y, w, h);
        ctx.fillText("pill " + b.score.toFixed(2), x, y - 4);
    });
};
</script>
{% endif %}
</div>
</body>
</html>
"""

# ================= ROUTES =================
@app.route("/", methods=["GET"])
def index():
    return render_template_string(HTML_PAGE, count=None)
@app.route("/predict", methods=["POST"])
def predict():
    if "image" not in request.files:
        return "No image uploaded", 400

    img_file = request.files["image"]
    filename = str(int(time.time() * 1000)) + ".jpg"
    img_path = os.path.join(UPLOAD_DIR, filename)
    img_file.save(img_path)

    img = cv2.imread(img_path)
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

    runner = ImageImpulseRunner(MODEL_PATH)
    runner.init()

    try:
        features, _ = runner.get_features_from_image(img)
        result = runner.classify(features)

        boxes = []
        count = 0

        for bb in result["result"]["bounding_boxes"]:
            if bb["value"] > 0.6:
                count += 1
                boxes.append({
                    "x": bb["x"],
                    "y": bb["y"],
                    "w": bb["width"],
                    "h": bb["height"],
                    "score": bb["value"]
                })

    finally:
        runner.stop()

    return render_template_string(
        HTML_PAGE,
        count=count,
        image_path=img_path,
        boxes=boxes
    )


@app.route("/uploads/<path:filename>")
def uploads(filename):
    return send_from_directory(UPLOAD_DIR, filename)


# ================= MAIN =================
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
