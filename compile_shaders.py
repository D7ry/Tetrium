import os
import subprocess

# compile all shader files in "shaders" folder into spir-v binaries
shaders_path = os.path.join(os.getcwd(), "shaders")

print("Compiling all shaders in {}".format(shaders_path))
for root, dirs, files in os.walk(shaders_path):
    for file in files:
        if file.endswith(".vert") or file.endswith(".frag"):
            print("Compiling shader: " + file)
            subprocess.call(["glslc", os.path.join(root, file), "-o", os.path.join(root, file + ".spv")])
