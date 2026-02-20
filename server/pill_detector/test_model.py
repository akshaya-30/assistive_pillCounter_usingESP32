from edge_impulse_linux.image import ImageImpulseRunner

runner = ImageImpulseRunner("model.eim")

with runner:
    print("✅ Model loaded successfully")
