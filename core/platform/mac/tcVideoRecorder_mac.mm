// =============================================================================
// tcVideoRecorder_mac.mm - macOS AVFoundation (AVAssetWriter) implementation
// =============================================================================

#ifdef __APPLE__

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>

#include "TrussC.h"
#include <cmath>

namespace trussc {

// ---------------------------------------------------------------------------
// Platform state (ARC manages the strong ObjC members of this C++ struct)
// ---------------------------------------------------------------------------
struct VideoRecorderPlatformData {
    AVAssetWriter* writer = nil;
    AVAssetWriterInput* input = nil;
    AVAssetWriterInputPixelBufferAdaptor* adaptor = nil;
    int width = 0;
    int height = 0;
    double fps = 30.0;
    int64_t frameIndex = 0;
    bool failed = false;
};

// ---------------------------------------------------------------------------
// openPlatform - create the H.264 writer and begin a session
// ---------------------------------------------------------------------------
bool VideoRecorder::openPlatform(const std::string& fullPath, int w, int h,
                                 float fps, int bitrate) {
    @autoreleasepool {
        NSString* nsPath = [NSString stringWithUTF8String:fullPath.c_str()];
        NSURL* url = [NSURL fileURLWithPath:nsPath];

        // Overwrite any existing file (AVAssetWriter refuses an existing path).
        [[NSFileManager defaultManager] removeItemAtURL:url error:nil];

        NSError* err = nil;
        AVAssetWriter* writer =
            [[AVAssetWriter alloc] initWithURL:url
                                      fileType:AVFileTypeMPEG4
                                         error:&err];
        if (!writer) {
            logError("VideoRecorder")
                << "AVAssetWriter init failed: "
                << (err ? err.localizedDescription.UTF8String : "unknown");
            return false;
        }

        // ~0.1 bits per pixel per second is a good default for clean demos.
        int br = (bitrate > 0)
                     ? bitrate
                     : (int)((double)w * h * (fps > 0 ? fps : 30.0) * 0.1);

        NSDictionary* compression = @{
            AVVideoAverageBitRateKey: @(br),
        };
        NSDictionary* settings = @{
            AVVideoCodecKey: AVVideoCodecTypeH264,
            AVVideoWidthKey: @(w),
            AVVideoHeightKey: @(h),
            AVVideoCompressionPropertiesKey: compression,
        };
        AVAssetWriterInput* input =
            [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                               outputSettings:settings];
        input.expectsMediaDataInRealTime = NO;

        NSDictionary* sourceAttrs = @{
            (NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
            (NSString*)kCVPixelBufferWidthKey: @(w),
            (NSString*)kCVPixelBufferHeightKey: @(h),
        };
        AVAssetWriterInputPixelBufferAdaptor* adaptor =
            [AVAssetWriterInputPixelBufferAdaptor
                assetWriterInputPixelBufferAdaptorWithAssetWriterInput:input
                                            sourcePixelBufferAttributes:sourceAttrs];

        if (![writer canAddInput:input]) {
            logError("VideoRecorder") << "writer cannot add video input";
            return false;
        }
        [writer addInput:input];

        if (![writer startWriting]) {
            logError("VideoRecorder")
                << "startWriting failed: "
                << (writer.error ? writer.error.localizedDescription.UTF8String
                                 : "unknown");
            return false;
        }
        [writer startSessionAtSourceTime:kCMTimeZero];

        auto* pd = new VideoRecorderPlatformData();
        pd->writer = writer;
        pd->input = input;
        pd->adaptor = adaptor;
        pd->width = w;
        pd->height = h;
        pd->fps = (fps > 0) ? fps : 30.0;
        platform_ = pd;
        return true;
    }
}

// ---------------------------------------------------------------------------
// appendPlatform - encode one RGBA8 (top-down) frame
// ---------------------------------------------------------------------------
bool VideoRecorder::appendPlatform(const unsigned char* rgba, double timeSec) {
    VideoRecorderPlatformData* pd = platform_;
    if (!pd || !pd->writer || pd->failed) return false;

    @autoreleasepool {
        // Offline writer: briefly wait for the input to accept more data.
        int spins = 0;
        while (!pd->input.readyForMoreMediaData && spins < 5000) {
            [NSThread sleepForTimeInterval:0.001];
            ++spins;
        }
        if (!pd->input.readyForMoreMediaData) {
            logError("VideoRecorder") << "input not ready (timeout)";
            pd->failed = true;
            return false;
        }

        CVPixelBufferRef pb = NULL;
        CVPixelBufferPoolRef pool = pd->adaptor.pixelBufferPool;
        if (pool) {
            CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &pb);
        }
        if (!pb) {
            // Fallback if the pool is unavailable.
            NSDictionary* attrs = @{
                (NSString*)kCVPixelBufferCGImageCompatibilityKey: @YES,
                (NSString*)kCVPixelBufferCGBitmapContextCompatibilityKey: @YES,
            };
            CVPixelBufferCreate(kCFAllocatorDefault, pd->width, pd->height,
                                kCVPixelFormatType_32BGRA,
                                (__bridge CFDictionaryRef)attrs, &pb);
        }
        if (!pb) {
            logError("VideoRecorder") << "could not create pixel buffer";
            pd->failed = true;
            return false;
        }

        CVPixelBufferLockBaseAddress(pb, 0);
        uint8_t* dst = (uint8_t*)CVPixelBufferGetBaseAddress(pb);
        size_t dstStride = CVPixelBufferGetBytesPerRow(pb);
        const int w = pd->width;
        const int h = pd->height;
        // RGBA -> BGRA, row by row (respect destination stride/alignment).
        for (int y = 0; y < h; ++y) {
            const unsigned char* srow = rgba + (size_t)y * w * 4;
            uint8_t* drow = dst + (size_t)y * dstStride;
            for (int x = 0; x < w; ++x) {
                drow[x * 4 + 0] = srow[x * 4 + 2];  // B
                drow[x * 4 + 1] = srow[x * 4 + 1];  // G
                drow[x * 4 + 2] = srow[x * 4 + 0];  // R
                drow[x * 4 + 3] = srow[x * 4 + 3];  // A
            }
        }
        CVPixelBufferUnlockBaseAddress(pb, 0);

        // Presentation time is decided by the caller (fixed-rate for manual
        // frames, real elapsed time for auto-capture). 600 = common timescale.
        CMTime t = CMTimeMakeWithSeconds(timeSec, 600);
        BOOL ok = [pd->adaptor appendPixelBuffer:pb withPresentationTime:t];
        CVPixelBufferRelease(pb);

        if (!ok) {
            logError("VideoRecorder")
                << "appendPixelBuffer failed: "
                << (pd->writer.error
                        ? pd->writer.error.localizedDescription.UTF8String
                        : "unknown");
            pd->failed = true;
            return false;
        }
        ++pd->frameIndex;
        return true;
    }
}

// ---------------------------------------------------------------------------
// closePlatform - mark finished and flush the file synchronously
// ---------------------------------------------------------------------------
void VideoRecorder::closePlatform() {
    VideoRecorderPlatformData* pd = platform_;
    if (!pd) return;

    @autoreleasepool {
        if (pd->writer && pd->writer.status == AVAssetWriterStatusWriting) {
            [pd->input markAsFinished];
            dispatch_semaphore_t sem = dispatch_semaphore_create(0);
            [pd->writer finishWritingWithCompletionHandler:^{
                dispatch_semaphore_signal(sem);
            }];
            dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        }
        pd->writer = nil;
        pd->input = nil;
        pd->adaptor = nil;
    }

    delete pd;
    platform_ = nullptr;
}

} // namespace trussc

#endif // __APPLE__
