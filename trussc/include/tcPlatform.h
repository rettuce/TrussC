#pragma once

#include <string>
#include <filesystem>

// =============================================================================
// Platform-specific features
// =============================================================================

namespace trussc {

// Forward declaration
class Pixels;

namespace platform {

// Get DPI scale of main display (available before window creation)
// macOS: 1.0 (normal) or 2.0 (Retina)
// Other: 1.0
float getDisplayScaleFactor();

// Change window size (specified in logical size)
// macOS: Uses NSWindow
void setWindowSize(int width, int height);

// Get absolute path of executable
std::string getExecutablePath();

// Get directory containing executable (with trailing /)
std::string getExecutableDir();

// ---------------------------------------------------------------------------
// Window positioning & style
// ---------------------------------------------------------------------------

// Set window position (screen coordinates, origin = top-left of primary display)
void setWindowPosition(int x, int y);

// Set borderless window (removes title bar and standard window chrome)
void setWindowBorderless(bool borderless);

// Set window frame atomically (position + size in one call, top-left origin)
// hideMenuBar: auto-hide menu bar and dock when true
void setWindowFrame(int x, int y, int width, int height, bool hideMenuBar = false);

// Get combined bounds of all screens (origin = top-left of primary display)
struct ScreenBounds {
    int x, y, width, height;
};
ScreenBounds getAllScreensBounds();

// ---------------------------------------------------------------------------
// Screenshot functionality
// ---------------------------------------------------------------------------

// Capture current window and store in Pixels
// Returns true on success, false on failure
bool captureWindow(Pixels& outPixels);

// Capture current window and save to file
// Returns true on success, false on failure
bool saveScreenshot(const std::filesystem::path& path);

} // namespace platform
} // namespace trussc
