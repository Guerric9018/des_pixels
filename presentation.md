# Mid-Term Presentation: Interactive Depixelization of Pixel Art

## Slide 1 — Project Overview

**Title:** *Interactive Depixelization of Pixel Art*  
**Team members:** Yaël Bodineau , Guerric David , Alexis Dobrzynski

**Goal of the project:**  
Convert low-resolution pixel art into editable, smooth vector graphics, while preserving user-intended shapes.

**Why it's needed:**  
Pixel art is inherently ambiguous. Automatic algorithms often fail to guess the artist’s true intent.  
Our approach includes user interaction to resolve these ambiguities.

---

## Slide 2 — Project Structure & Files

**Current Architecture:**

1. Load input image (pixel art)
2. Cluster similar pixels
3. Detect boundaries and generate nodes
4. *(Spring simulation, UI and vector output: coming next)*

**Files and Responsibilities:**

| File            | Description                                                                |
|-----------------|-----------------------------------------------------------------------------|
| `main.cpp`      | Application entry point: loads image, calls clustering and node generation |
| `clusters.cpp`  | Handles color clustering and graph construction                            |
| `data.hpp`      | Defines structures for pixels, clusters, and nodes                         |
| `stb_image.h`   | External library to load images into memory                                |

---

## Slide 3 — What We've Done So Far

**Implemented:**

- Pixel color clustering using a similarity graph
- Boundary detection
- Node generation (corner nodes and edge nodes)

**Visualization:**  
→ Add images showing:
- The original pixel art
- Nodes generated on top of it (optional overlay)

---

## Slide 4 — What’s Next

We still need to implement the core vectorization pipeline:

- Spring simulation  
  Relax node positions using forces (origin, neighbor, area)  
  → produces smooth and natural shapes

- User interface for interaction  
  Tools to set corners (sharp/smooth), adjust node forces

- Bezier curve fitting & SVG output  
  Replace polylines by editable vector curves (Bezier), layered output

- Evaluation and visual comparisons  
  Compare with other methods (e.g. Inkscape, DPA)

---

## Slide 5 — Roadmap & Final Goals

**Current status:**  
Input and node generation done

**Next milestones:**

| Date         | Task                           |
|--------------|--------------------------------|
| Week 1–2     | Spring system implementation   |
| Week 3       | UI interaction                 |
| Week 4       | Bezier fitting, SVG output     |
| Final week   | Testing, comparisons, polishing|

**Final Deliverables:**
- Fully working interactive tool
- Visual comparison with state of the art
- Final presentation & report
