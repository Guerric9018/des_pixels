# Depixelization of Pixel Art through Spring Simulation

This project implements a method for depixelizing pixel art using spring-based simulation. It allows users to interactively adjust parameters, generate a mesh, and export the final result as an SVG file.

## Build Instructions

1. Clone the repository and navigate to the project root directory.
2. Run the following command to build the project:

```bash
make
```

## Usage

Once the build is complete, you can run the application using:

```bash
./main <input_png_file> <output_svg_file>
```

Example:

```bash
./main input.png output.svg
```

## Application Workflow

1. **Play with Settings**:
   Use the available UI controls to adjust parameters.

2. **Rebuild Clusters**:
   If you change the `delta_c` value, click "Rebuild Clusters" to update them accordingly.

3. **Build Mesh**:
   Once clusters are correct, build the mesh over the image.

4. **Apply Force**:
   After building the mesh, apply the spring simulation to smooth the structure.

5. **Export SVG**:
   After the simulation is applied, the output SVG file will be generated automatically at the path provided when launching the program.

## Requirements

To build and run this project, make sure the following dependencies are installed on your system:

* **C++ Compiler** with C++11 support
* **Make** utility
* **GLFW** – for window and input handling (`libglfw3-dev`)
* **OpenGL** – for rendering (`libgl1-mesa-dev`)
* **X11** – core X11 libraries (`libx11-dev`)
* **Xrandr** – X Resize and Rotate extension (`libxrandr-dev`)
* **Xi** – X Input extension (`libxi-dev`)

### Install on Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential libglfw3-dev libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev
```

## Notes

* Ensure the input file is a valid `.png` image.
* The output will be written to the specified `.svg` file once the process is complete.