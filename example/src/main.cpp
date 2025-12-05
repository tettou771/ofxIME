#include "ofMain.h"
#include "ofApp.h"

int main() {
    ofGLWindowSettings settings;
    settings.setSize(800, 600);
    settings.setGLVersion(3, 2);
    settings.windowMode = OF_WINDOW;
    ofCreateWindow(settings);

    return ofRunApp(new ofApp());
}
