/*****************************************************************************
 * VLCSnowEffectView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCSnowEffectView.h"

#import "main/VLCMain.h"
#import "shaders/VLCShaderTypes.h"

@interface VLCSnowEffectView ()

@property (readonly) CFTimeInterval startTime;
@property (readonly) id<MTLCommandQueue> commandQueue;
@property (readonly) id<MTLFunction> vertexFunction;
@property (readonly) id<MTLFunction> fragmentFunction;
@property (readonly) id<MTLRenderPipelineState> pipelineState;
@property (readonly) id<MTLBuffer> vertexBuffer;
@property (readonly) id<MTLBuffer> uniformBuffer;
@property (readonly) id<MTLBuffer> snowflakeDataBuffer;
@property (readonly) NSUInteger snowflakeCount;

@end

@implementation VLCSnowEffectView

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder
{
    return [super encodeWithCoder:coder];
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    self.wantsLayer = YES;
    self.layer.backgroundColor = NSColor.clearColor.CGColor;

    VLCMain * const main = VLCMain.sharedInstance;
    const id<MTLDevice> metalDevice = main.metalDevice;
    NSParameterAssert(metalDevice != nil);
    NSParameterAssert(main.metalLibrary != nil);

    _mtkView = [[MTKView alloc] initWithFrame:self.bounds device:metalDevice];
    self.mtkView.delegate = self;
    self.mtkView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.mtkView.clearColor = MTLClearColorMake(0, 0, 0, 0);
    ((CAMetalLayer *)self.mtkView.layer).opaque = NO;
    self.mtkView.layer.backgroundColor = NSColor.clearColor.CGColor;
    [self addSubview:self.mtkView];

    _commandQueue = [metalDevice newCommandQueue];
    NSParameterAssert(self.commandQueue != nil);
    _vertexFunction = [main.metalLibrary newFunctionWithName:@"snowVertexShader"];
    NSParameterAssert(self.vertexFunction != nil);
    _fragmentFunction = [main.metalLibrary newFunctionWithName:@"snowFragmentShader"];
    NSParameterAssert(self.fragmentFunction != nil);

    MTLRenderPipelineDescriptor * const pipelineDescriptor =
        [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.label = @"Snow Pipeline";
    pipelineDescriptor.vertexFunction = self.vertexFunction;
    pipelineDescriptor.fragmentFunction = self.fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = self.mtkView.colorPixelFormat;
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;

    NSError *error;
    _pipelineState =
        [metalDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    NSParameterAssert(error == nil);
    NSParameterAssert(pipelineDescriptor != nil);

    // --- Vertex Buffer Setup ---
    // Example: Define vertices for a single small quad (to be instanced for snowflakes)
    // You might draw points instead, which simplifies this.
    // Format: {x, y, u, v} - position and texture coordinate
    static const VertexIn quadVertices[] = {
        // Triangle 1 (positions from -0.5 to 0.5 for a unit quad centered at origin)
        { .position = {-0.5f,  0.5f} }, // Top-left
        { .position = {-0.5f, -0.5f} }, // Bottom-left
        { .position = { 0.5f, -0.5f} }, // Bottom-right
        // Triangle 2
        { .position = { 0.5f, -0.5f} }, // Bottom-right
        { .position = { 0.5f,  0.5f} }, // Top-right
        { .position = {-0.5f,  0.5f} }  // Top-left
    };
    _vertexBuffer = [metalDevice newBufferWithBytes:quadVertices
                                             length:sizeof(quadVertices)
                                            options:MTLResourceStorageModeShared]; // CPU & GPU
    NSParameterAssert(self.vertexBuffer != nil);

    // --- Uniform Buffer Setup ---
    // Create a buffer large enough for your Uniforms struct
    // The struct definition would typically be in a shared header (.h)
    // included by both Objective-C and your .metal file.
    _uniformBuffer = [metalDevice newBufferWithLength:sizeof(Uniforms) // Assumes Uniforms struct exists
                                              options:MTLResourceStorageModeShared];
    NSParameterAssert(self.uniformBuffer != nil);

    _snowflakeCount = 200; // Or however many you want
    Snowflake * const snowflakesArray =
        (Snowflake *)malloc(sizeof(Snowflake) * self.snowflakeCount);
    for (NSUInteger i = 0; i < self.snowflakeCount; ++i) {
        Snowflake flake;
        flake.initialPosition.x = ((float)arc4random_uniform(2000) / 1000.0f) - 1.0f;
        flake.initialPosition.y = ((float)arc4random_uniform(2000) / 1000.0f) - 1.0f;
        flake.initialPosition.z = (float)arc4random_uniform(1000) / 1000.0f;
        flake.randomSeed = (float)arc4random_uniform(1000) / 1000.0f;
        snowflakesArray[i] = flake;
    }
    _snowflakeDataBuffer = [metalDevice newBufferWithBytes:snowflakesArray // Use self.
                                                    length:sizeof(Snowflake) * _snowflakeCount
                                                   options:MTLResourceStorageModeShared];
    free(snowflakesArray);
    NSParameterAssert(self.snowflakeDataBuffer != nil);

    self.mtkView.paused = NO;
    self.mtkView.enableSetNeedsDisplay = NO;
    _startTime = CACurrentMediaTime();
}

- (BOOL)isOpaque
{
    return NO;
}

// MARK: - MTKViewDelegate

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
    Uniforms * const uniforms = (Uniforms *)self.uniformBuffer.contents;
    uniforms->resolution = (vector_float2){(float)size.width, (float)size.height};
}

- (void)drawInMTKView:(MTKView *)view
{
    // 1. Create Command Buffer
    const id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    commandBuffer.label = @"Snow Frame Command Buffer";

    // 2. Get Render Pass Descriptor (specifies the drawing target)
    MTLRenderPassDescriptor * const renderPassDescriptor = view.currentRenderPassDescriptor;
    if (!renderPassDescriptor) {
        NSLog(@"Failed to get render pass descriptor");
        return; // Skip frame if view isn't ready
    }

    // 3. Create Render Command Encoder
    const id<MTLRenderCommandEncoder> encoder =
        [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    encoder.label = @"Snow Render Encoder";

    // 4. Set Pipeline State
    [encoder setRenderPipelineState:self.pipelineState];

    // 5. Update and Set Uniforms
    Uniforms * const uniforms = (Uniforms *)[self.uniformBuffer contents];
    uniforms->time = (float)(CACurrentMediaTime() - _startTime); // Calculate elapsed time
    uniforms->resolution = (vector_float2){(float)view.drawableSize.width, (float)view.drawableSize.height};

    // Set buffers in indeces matching those defined in the shader
    [encoder setVertexBuffer:self.uniformBuffer offset:0 atIndex:2];
    [encoder setVertexBuffer:self.snowflakeDataBuffer offset:0 atIndex:1];
    [encoder setVertexBuffer:_vertexBuffer offset:0 atIndex:0]; // Assuming buffer(0) for vertices

    [encoder setFragmentBuffer:self.uniformBuffer offset:0 atIndex:0];

    // 6. Draw Call
    // Draw multiple instances of the quad/point, one for each snowflake.
    // The vertex shader uses [[instance_id]] to position each one.
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle // Or MTLPrimitiveTypePoint
                vertexStart:0
                vertexCount:6 // 6 vertices for the quad example
              instanceCount:self.snowflakeCount];

    // 7. End Encoding
    [encoder endEncoding];

    // 8. Present Drawable
    if (view.currentDrawable) {
        [commandBuffer presentDrawable:view.currentDrawable];
    }

    // 9. Commit Command Buffer
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted]; // Optional: Wait for completion (useful for debugging)
}

@end
