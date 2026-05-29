#pragma once

// =============================================================================
// tcxThreadedDepthCamera.h - Threaded, thread-safe base for depth cameras
// =============================================================================
//
// ThreadedDepthCameraBase<Frame> takes all the error-prone plumbing - the
// background grabber thread, the mutex, the buffer hand-off, and resetting the
// frame-new flags - and implements it ONCE, correctly, so a concrete camera
// addon never writes that code (and so can never forget it).
//
// An implementation only:
//   1. Defines a Frame type holding one frame's worth of data (depth array,
//      color image, IR, ...). Its shape is entirely up to the implementation.
//   2. Implements openDevice() / closeDevice() and captureInto(Frame&).
//   3. Implements the DepthCamera / capability getters by reading front().
//
//   struct OrbbecFrame { std::vector<uint16_t> depth; Pixels color; ... };
//
//   class Orbbec : public ThreadedDepthCameraBase<OrbbecFrame>,
//                  public IColorStream {
//   protected:
//       bool openDevice() override { ... }
//       void closeDevice() override { ... }
//       StreamFreshness captureInto(OrbbecFrame& f) override {
//           // grab from the SDK into f; return what changed
//       }
//   public:
//       float getDistanceAt(int x, int y) const override {
//           return front().depth[y * getWidth() + x] * 0.001f;   // reads FRONT
//       }
//       bool isColorFrameNew() const override { return isStreamNew(Stream::Color); }
//       ...
//   };
//
// Thread-safety model (triple buffer):
//   - The background thread fills the CAPTURE buffer (which only it touches),
//     then briefly locks to promote it to BACK.
//   - update() (main thread) briefly locks to swap BACK -> FRONT.
//   - Getters read FRONT, which is mutated only inside update() on the main
//     thread. So the getters need NO locking and stay cheap - the mutex is held
//     only for the two O(1) pointer swaps, never during slow device I/O or
//     during per-pixel reads.
//
// Threading is optional: with setThreaded(false) (the default), update() simply
// captures inline on the calling thread. Call setThreaded() before setup().
//
// =============================================================================

#include "tcxDepthCameraBase.h"
#include <mutex>
#include <utility>

namespace tcx {

using namespace tc;

template <class Frame>
class ThreadedDepthCameraBase : public DepthCamera, protected tc::Thread {
public:
    ThreadedDepthCameraBase()
        : front_(&buffers_[0]), back_(&buffers_[1]), capture_(&buffers_[2]) {}

    ~ThreadedDepthCameraBase() override {
        // tc::Thread's destructor also stops the thread, but do it here too so
        // the worker is guaranteed stopped before our buffers are destroyed.
        if (isThreadRunning()) {
            stopThread();
            waitForThread(false);
        }
    }

    // -------------------------------------------------------------------------
    // DepthCamera lifecycle (final - the base owns threading)
    // -------------------------------------------------------------------------
    bool setup() final {
        if (!openDevice()) return false;
        if (threaded_) startThread();
        return true;
    }

    void close() final {
        if (isThreadRunning()) {
            stopThread();
            waitForThread(false);
        }
        closeDevice();
    }

    void update() final {
        if (isThreadRunning()) {
            // Threaded: just promote whatever the worker has staged.
            std::lock_guard<std::mutex> lk(mutex_);
            if (backReady_) {
                std::swap(back_, front_);
                freshness_ = pending_;
                pending_.clear();
                backReady_ = false;
            } else {
                freshness_.clear();  // no new frame arrived this tick
            }
        } else {
            // Inline: capture on the calling thread into back, then promote.
            StreamFreshness f = captureInto(*back_);
            if (f.any()) std::swap(back_, front_);
            freshness_ = f;
        }
    }

    bool isFrameNew() const final { return freshness_.depth; }

protected:
    // -------------------------------------------------------------------------
    // Hooks the implementation provides
    // -------------------------------------------------------------------------

    // Open / close the underlying device. Called from setup() / close().
    virtual bool openDevice() = 0;
    virtual void closeDevice() = 0;

    // Grab exactly one frame from the device into dst and report which streams
    // it refreshed. In threaded mode this runs on the background thread and may
    // block on device I/O; in non-threaded mode it runs inline in update().
    virtual StreamFreshness captureInto(Frame& dst) = 0;

    // The current readable frame. Getters (getDistanceAt, getColorPixels, ...)
    // read from here. Safe to read without locking on the same thread that
    // calls update().
    const Frame& front() const { return *front_; }

    // Per-stream freshness for capability delegation, e.g.
    //   bool isColorFrameNew() const override { return isStreamNew(Stream::Color); }
    bool isStreamNew(Stream s) const { return freshness_.get(s); }

private:
    // tc::Thread worker: continuously grab into CAPTURE, then promote to BACK.
    void threadedFunction() override {
        while (isThreadRunning()) {
            StreamFreshness f = captureInto(*capture_);
            std::lock_guard<std::mutex> lk(mutex_);
            std::swap(capture_, back_);  // capture_ now reuses the old back
            pending_ |= f;               // fold in case main hasn't consumed yet
            backReady_ = true;
        }
    }

    Frame buffers_[3];
    Frame* front_;    // read by getters / main thread only
    Frame* back_;     // staging slot, guarded by mutex_
    Frame* capture_;  // written by the worker (or inline update) only

    std::mutex mutex_;
    StreamFreshness pending_{};    // accumulated by worker, guarded by mutex_
    StreamFreshness freshness_{};  // current frame's flags, main thread only
    bool backReady_ = false;       // guarded by mutex_
};

} // namespace tcx
