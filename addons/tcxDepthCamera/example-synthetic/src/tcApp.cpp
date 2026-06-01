#include "tcApp.h"

void tcApp::setup() {
    setWindowTitle("tcxDepthCamera - SyntheticDepthCamera point cloud");

    camera = make_shared<SyntheticDepthCamera>(640, 480);
    camera->setThreaded(true);   // grab on a background thread (before setup)
    camera->enableDepth();       // streams are all off by default; enable what we use
    camera->enableColor();
    camera->setup();

    view.setDistance(400.0f);
    view.enableMouseInput();
}

void tcApp::update() {
    camera->update();
    if (camera->isFrameNew()) rebuild();
}

// Rebuild the point-cloud mesh from the latest frame, in display units.
void tcApp::rebuild() {
    cloud = camera->toMesh({.colors = colored, .step = step});
    // tcxDepthCamera world coords are in meters with +Y down. Scale up for the
    // viewer, flip Y up, and pull the wall (~2.5 m) back toward the origin.
    cloud.scale(100.0f, -100.0f, 100.0f);
    cloud.translate(0.0f, 0.0f, -200.0f);
}

void tcApp::draw() {
    clear(0.08f, 0.09f, 0.11f);

    view.begin();
    pushMatrix();
    // Start a quarter-turn in so we open facing the front of the cloud.
    rotateY(getElapsedTimef() * 0.3f + TAU * 0.25f);
    if (!colored) setColor(0.55f, 0.75f, 1.0f);
    cloud.draw();
    popMatrix();
    view.end();

    setColor(1.0f);
    string hud = "SyntheticDepthCamera (no hardware)\n";
    hud += to_string(cloud.getNumVertices()) + " points";
    hud += colored ? "  [colored]" : "  [plain]";
    hud += "\nC: toggle color    1-4: decimation    drag: orbit";
    drawBitmapString(hud, 20, 20);
}

void tcApp::keyPressed(int key) {
    if (key == 'c' || key == 'C') {
        colored = !colored;
    } else if (key >= '1' && key <= '4') {
        step = key - '0';
    }
    rebuild();
}
