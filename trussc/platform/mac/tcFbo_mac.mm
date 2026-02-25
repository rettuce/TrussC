// =============================================================================
// tcFbo_mac.mm - FBO pixel readback (macOS / Metal)
// =============================================================================

#include "TrussC.h"

#ifdef __APPLE__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

// Access sokol's internal command buffer.
// After sg_end_pass(), the render encoder is finished but the command buffer
// is still open (committed only in sg_commit/present). We encode a blit into
// the same command buffer, then commit+wait so the GPU executes both the
// FBO render pass AND the blit before we read back pixels.
//
// This temporarily commits the command buffer mid-frame.  sokol will lazily
// create a new one when the next sg_begin_pass() is called (see
// _sg_mtl_begin_pass: "if (nil == _sg.mtl.cmd_buffer)").

extern "C" {
    // Defined in sokol_gfx.h (Metal backend internals)
    // _sg is the global state, _sg.mtl.cmd_buffer is id<MTLCommandBuffer>
    // We access it through the public query helpers where possible.
}

namespace trussc {

bool Fbo::readPixelsPlatform(unsigned char* pixels) const {
    if (!allocated_ || !pixels) return false;

    // Get sokol's command queue and create a SEPARATE command buffer.
    // We commit sokol's command buffer first (to flush the FBO render pass),
    // then enqueue our blit on a fresh command buffer.
    id<MTLCommandQueue> cmdQueue = (__bridge id<MTLCommandQueue>)sg_mtl_command_queue();
    if (!cmdQueue) {
        logError() << "[FBO] Failed to get Metal command queue";
        return false;
    }

    // Get source texture (FBO color attachment)
    sg_mtl_image_info info = sg_mtl_query_image_info(colorTexture_.getImage());
    id<MTLTexture> srcTexture = (__bridge id<MTLTexture>)info.tex[info.active_slot];
    if (!srcTexture) {
        logError() << "[FBO] Failed to get source MTLTexture";
        return false;
    }

    // Force sokol to commit its current command buffer.
    // This submits all pending GPU work (including the FBO render pass).
    // sokol will create a new command buffer on the next sg_begin_pass().
    sg_commit();

    // Now create a new command buffer for the blit (on the same queue,
    // so it executes AFTER sokol's just-committed render commands).
    id<MTLDevice> device = cmdQueue.device;

    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                    width:width_
                                                                                   height:height_
                                                                                mipmapped:NO];
    desc.storageMode = MTLStorageModeShared;
    desc.usage = MTLTextureUsageShaderRead;

    id<MTLTexture> dstTexture = [device newTextureWithDescriptor:desc];
    if (!dstTexture) {
        logError() << "[FBO] Failed to create staging texture";
        return false;
    }

    id<MTLCommandBuffer> cmdBuffer = [cmdQueue commandBuffer];
    id<MTLBlitCommandEncoder> blitEncoder = [cmdBuffer blitCommandEncoder];

    [blitEncoder copyFromTexture:srcTexture
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(width_, height_, 1)
                       toTexture:dstTexture
                destinationSlice:0
                destinationLevel:0
               destinationOrigin:MTLOriginMake(0, 0, 0)];

    [blitEncoder endEncoding];
    [cmdBuffer commit];
    [cmdBuffer waitUntilCompleted];

    // Read pixels from the shared staging texture
    MTLRegion region = MTLRegionMake2D(0, 0, width_, height_);
    NSUInteger bytesPerRow = width_ * 4;  // RGBA8

    [dstTexture getBytes:pixels
             bytesPerRow:bytesPerRow
              fromRegion:region
             mipmapLevel:0];

    return true;
}

} // namespace trussc

#endif // __APPLE__
