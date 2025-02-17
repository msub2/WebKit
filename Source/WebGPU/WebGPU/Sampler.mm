/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Sampler.h"

#import "APIConversions.h"
#import "Device.h"
#import <cmath>

namespace WebGPU {

static bool validateCreateSampler(Device&, const WGPUSamplerDescriptor& descriptor)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpusamplerdescriptor

    // FIXME: "device is valid."

    // "descriptor.lodMinClamp is greater than or equal to 0."
    if (std::isnan(descriptor.lodMinClamp) || descriptor.lodMinClamp < 0)
        return false;

    // "descriptor.lodMaxClamp is greater than or equal to descriptor.lodMinClamp."
    if (std::isnan(descriptor.lodMaxClamp) || descriptor.lodMaxClamp < descriptor.lodMinClamp)
        return false;

    // "descriptor.maxAnisotropy is greater than or equal to 1."
    if (descriptor.maxAnisotropy < 1)
        return false;

    // "When descriptor.maxAnisotropy is greater than 1"
    if (descriptor.maxAnisotropy > 1) {
        // "descriptor.magFilter, descriptor.minFilter, and descriptor.mipmapFilter must be equal to "linear"."
        if (descriptor.magFilter != WGPUFilterMode_Linear
            || descriptor.minFilter != WGPUFilterMode_Linear
            || descriptor.mipmapFilter != WGPUFilterMode_Linear)
            return false;
    }

    return true;
}

static std::optional<MTLSamplerAddressMode> addressMode(WGPUAddressMode addressMode)
{
    switch (addressMode) {
    case WGPUAddressMode_Repeat:
        return MTLSamplerAddressModeRepeat;
    case WGPUAddressMode_MirrorRepeat:
        return MTLSamplerAddressModeMirrorRepeat;
    case WGPUAddressMode_ClampToEdge:
        return MTLSamplerAddressModeClampToEdge;
    case WGPUAddressMode_Force32:
        return std::nullopt;
    }
}

static std::optional<MTLSamplerMinMagFilter> minMagFilter(WGPUFilterMode filterMode)
{
    switch (filterMode) {
    case WGPUFilterMode_Nearest:
        return MTLSamplerMinMagFilterNearest;
    case WGPUFilterMode_Linear:
        return MTLSamplerMinMagFilterLinear;
    case WGPUFilterMode_Force32:
        return std::nullopt;
    }
}

static std::optional<MTLSamplerMipFilter> mipFilter(WGPUFilterMode filterMode)
{
    switch (filterMode) {
    case WGPUFilterMode_Nearest:
        return MTLSamplerMipFilterNearest;
    case WGPUFilterMode_Linear:
        return MTLSamplerMipFilterLinear;
    case WGPUFilterMode_Force32:
        return std::nullopt;
    }
}

static std::optional<MTLCompareFunction> compareFunction(WGPUCompareFunction compareFunction)
{
    switch (compareFunction) {
    case WGPUCompareFunction_Undefined:
        return std::nullopt;
    case WGPUCompareFunction_Never:
        return MTLCompareFunctionNever;
    case WGPUCompareFunction_Less:
        return MTLCompareFunctionLess;
    case WGPUCompareFunction_LessEqual:
        return MTLCompareFunctionLessEqual;
    case WGPUCompareFunction_Greater:
        return MTLCompareFunctionGreater;
    case WGPUCompareFunction_GreaterEqual:
        return MTLCompareFunctionGreaterEqual;
    case WGPUCompareFunction_Equal:
        return MTLCompareFunctionEqual;
    case WGPUCompareFunction_NotEqual:
        return MTLCompareFunctionNotEqual;
    case WGPUCompareFunction_Always:
        return MTLCompareFunctionAlways;
    case WGPUCompareFunction_Force32:
        return std::nullopt;
    }
}

RefPtr<Sampler> Device::createSampler(const WGPUSamplerDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return nullptr;

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createsampler

    // "If validating GPUSamplerDescriptor(this, descriptor) returns false:"
    if (!validateCreateSampler(*this, descriptor)) {
        // "Generate a validation error."
        generateAValidationError("Validation failure."_s);

        // "Create a new invalid GPUSampler and return the result."
        return nullptr;
    }

    MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];

    if (auto addressMode = WebGPU::addressMode(descriptor.addressModeU))
        samplerDescriptor.rAddressMode = addressMode.value();
    else
        return nullptr;

    if (auto addressMode = WebGPU::addressMode(descriptor.addressModeV))
        samplerDescriptor.sAddressMode = addressMode.value();
    else
        return nullptr;

    if (auto addressMode = WebGPU::addressMode(descriptor.addressModeW))
        samplerDescriptor.tAddressMode = addressMode.value();
    else
        return nullptr;

    if (auto minMagFilter = WebGPU::minMagFilter(descriptor.magFilter))
        samplerDescriptor.magFilter = minMagFilter.value();
    else
        return nullptr;

    if (auto minMagFilter = WebGPU::minMagFilter(descriptor.minFilter))
        samplerDescriptor.minFilter = minMagFilter.value();
    else
        return nullptr;

    if (auto mipFilter = WebGPU::mipFilter(descriptor.mipmapFilter))
        samplerDescriptor.mipFilter = mipFilter.value();
    else
        return nullptr;

    samplerDescriptor.lodMinClamp = descriptor.lodMinClamp;

    samplerDescriptor.lodMaxClamp = descriptor.lodMaxClamp;

    if (auto compareFunction = WebGPU::compareFunction(descriptor.compare))
        samplerDescriptor.compareFunction = compareFunction.value();
    else
        return nullptr;

    // "The used value of maxAnisotropy will be clamped to the maximum value that the platform supports."
    // https://developer.apple.com/documentation/metal/mtlsamplerdescriptor/1516164-maxanisotropy?language=objc
    // "Values must be between 1 and 16, inclusive."
    samplerDescriptor.maxAnisotropy = std::min<uint16_t>(descriptor.maxAnisotropy, 16);

    samplerDescriptor.label = fromAPI(descriptor.label);

    id<MTLSamplerState> samplerState = [m_device newSamplerStateWithDescriptor:samplerDescriptor];
    if (!samplerState)
        return nullptr;

    // "Let s be a new GPUSampler object."
    // "Set s.[[descriptor]] to descriptor."
    // "Set s.[[isComparison]] to false if the compare attribute of s.[[descriptor]] is null or undefined. Otherwise, set it to true."
    // "Set s.[[isFiltering]] to false if none of minFilter, magFilter, or mipmapFilter has the value of "linear". Otherwise, set it to true."
    // "Return s."

    return Sampler::create(samplerState, descriptor, *this);
}

Sampler::Sampler(id<MTLSamplerState> samplerState, const WGPUSamplerDescriptor& descriptor, Device& device)
    : m_samplerState(samplerState)
    , m_descriptor(descriptor)
    , m_device(device)
{
}

Sampler::~Sampler() = default;

void Sampler::setLabel(String&&)
{
    // MTLRenderPipelineState's labels are read-only.
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuSamplerRelease(WGPUSampler sampler)
{
    WebGPU::fromAPI(sampler).deref();
}

void wgpuSamplerSetLabel(WGPUSampler sampler, const char* label)
{
    WebGPU::fromAPI(sampler).setLabel(WebGPU::fromAPI(label));
}
