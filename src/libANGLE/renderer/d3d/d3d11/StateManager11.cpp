//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StateManager11.cpp: Defines a class for caching D3D11 state

#include "libANGLE/renderer/d3d/d3d11/StateManager11.h"

#include "common/BitSetIterator.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"

namespace rx
{

StateManager11::StateManager11()
    : mBlendStateIsDirty(false),
      mCurBlendColor(0, 0, 0, 0),
      mCurSampleMask(0),
      mDepthStencilStateIsDirty(false),
      mCurStencilRef(0),
      mCurStencilBackRef(0),
      mCurStencilSize(0),
      mRasterizerStateIsDirty(false),
      mScissorStateIsDirty(false),
      mCurScissorEnabled(false),
      mCurScissorRect(),
      mViewportStateIsDirty(false),
      mCurViewport(),
      mCurNear(0.0f),
      mCurFar(0.0f),
      mViewportBounds(),
      mCurPresentPathFastEnabled(false),
      mCurPresentPathFastColorBufferHeight(0),
      mRenderer11DeviceCaps(nullptr),
      mDeviceContext(nullptr),
      mStateCache(nullptr)
{
    mCurBlendState.blend                 = false;
    mCurBlendState.sourceBlendRGB        = GL_ONE;
    mCurBlendState.destBlendRGB          = GL_ZERO;
    mCurBlendState.sourceBlendAlpha      = GL_ONE;
    mCurBlendState.destBlendAlpha        = GL_ZERO;
    mCurBlendState.blendEquationRGB      = GL_FUNC_ADD;
    mCurBlendState.blendEquationAlpha    = GL_FUNC_ADD;
    mCurBlendState.colorMaskRed          = true;
    mCurBlendState.colorMaskBlue         = true;
    mCurBlendState.colorMaskGreen        = true;
    mCurBlendState.colorMaskAlpha        = true;
    mCurBlendState.sampleAlphaToCoverage = false;
    mCurBlendState.dither                = false;

    mCurDepthStencilState.depthTest                = false;
    mCurDepthStencilState.depthFunc                = GL_LESS;
    mCurDepthStencilState.depthMask                = true;
    mCurDepthStencilState.stencilTest              = false;
    mCurDepthStencilState.stencilMask              = true;
    mCurDepthStencilState.stencilFail              = GL_KEEP;
    mCurDepthStencilState.stencilPassDepthFail     = GL_KEEP;
    mCurDepthStencilState.stencilPassDepthPass     = GL_KEEP;
    mCurDepthStencilState.stencilWritemask         = static_cast<GLuint>(-1);
    mCurDepthStencilState.stencilBackFunc          = GL_ALWAYS;
    mCurDepthStencilState.stencilBackMask          = static_cast<GLuint>(-1);
    mCurDepthStencilState.stencilBackFail          = GL_KEEP;
    mCurDepthStencilState.stencilBackPassDepthFail = GL_KEEP;
    mCurDepthStencilState.stencilBackPassDepthPass = GL_KEEP;
    mCurDepthStencilState.stencilBackWritemask     = static_cast<GLuint>(-1);

    mCurRasterState.rasterizerDiscard   = false;
    mCurRasterState.cullFace            = false;
    mCurRasterState.cullMode            = GL_BACK;
    mCurRasterState.frontFace           = GL_CCW;
    mCurRasterState.polygonOffsetFill   = false;
    mCurRasterState.polygonOffsetFactor = 0.0f;
    mCurRasterState.polygonOffsetUnits  = 0.0f;
    mCurRasterState.pointDrawMode       = false;
    mCurRasterState.multiSample         = false;
}

StateManager11::~StateManager11()
{
}

void StateManager11::initialize(ID3D11DeviceContext *deviceContext,
                                RenderStateCache *stateCache,
                                Renderer11DeviceCaps *renderer11DeviceCaps)
{
    mDeviceContext        = deviceContext;
    mStateCache           = stateCache;
    mRenderer11DeviceCaps = renderer11DeviceCaps;
}

void StateManager11::updateStencilSizeIfChanged(bool depthStencilInitialized,
                                                unsigned int stencilSize)
{
    if (!depthStencilInitialized || stencilSize != mCurStencilSize)
    {
        mCurStencilSize           = stencilSize;
        mDepthStencilStateIsDirty = true;
    }
}

void StateManager11::setViewportBounds(const int width, const int height)
{
    if (mRenderer11DeviceCaps->featureLevel <= D3D_FEATURE_LEVEL_9_3 &&
        (mViewportBounds.width != width || mViewportBounds.height != height))
    {
        mViewportBounds       = gl::Extents(width, height, 1);
        mViewportStateIsDirty = true;
    }
}

void StateManager11::updatePresentPath(bool presentPathFastActive,
                                       const gl::FramebufferAttachment *framebufferAttachment)
{
    const int colorBufferHeight =
        framebufferAttachment ? framebufferAttachment->getSize().height : 0;

    if ((mCurPresentPathFastEnabled != presentPathFastActive) ||
        (presentPathFastActive && (colorBufferHeight != mCurPresentPathFastColorBufferHeight)))
    {
        mCurPresentPathFastEnabled           = presentPathFastActive;
        mCurPresentPathFastColorBufferHeight = colorBufferHeight;
        mViewportStateIsDirty                = true;  // Viewport may need to be vertically inverted
        mScissorStateIsDirty                 = true;  // Scissor rect may need to be vertically inverted
        mRasterizerStateIsDirty              = true;  // Cull Mode may need to be inverted
    }
}

void StateManager11::syncState(const gl::State &state, const gl::State::DirtyBits &dirtyBits)
{
    if (!dirtyBits.any())
    {
        return;
    }

    for (unsigned int dirtyBit : angle::IterateBitSet(dirtyBits))
    {
        switch (dirtyBit)
        {
            case gl::State::DIRTY_BIT_BLEND_EQUATIONS:
            {
                const gl::BlendState &blendState = state.getBlendState();
                if (blendState.blendEquationRGB != mCurBlendState.blendEquationRGB ||
                    blendState.blendEquationAlpha != mCurBlendState.blendEquationAlpha)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_BLEND_FUNCS:
            {
                const gl::BlendState &blendState = state.getBlendState();
                if (blendState.sourceBlendRGB != mCurBlendState.sourceBlendRGB ||
                    blendState.destBlendRGB != mCurBlendState.destBlendRGB ||
                    blendState.sourceBlendAlpha != mCurBlendState.sourceBlendAlpha ||
                    blendState.destBlendAlpha != mCurBlendState.destBlendAlpha)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_BLEND_ENABLED:
                if (state.getBlendState().blend != mCurBlendState.blend)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED:
                if (state.getBlendState().sampleAlphaToCoverage !=
                    mCurBlendState.sampleAlphaToCoverage)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DITHER_ENABLED:
                if (state.getBlendState().dither != mCurBlendState.dither)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_COLOR_MASK:
            {
                const gl::BlendState &blendState = state.getBlendState();
                if (blendState.colorMaskRed != mCurBlendState.colorMaskRed ||
                    blendState.colorMaskGreen != mCurBlendState.colorMaskGreen ||
                    blendState.colorMaskBlue != mCurBlendState.colorMaskBlue ||
                    blendState.colorMaskAlpha != mCurBlendState.colorMaskAlpha)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_BLEND_COLOR:
                if (state.getBlendColor() != mCurBlendColor)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DEPTH_MASK:
                if (state.getDepthStencilState().depthMask != mCurDepthStencilState.depthMask)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DEPTH_TEST_ENABLED:
                if (state.getDepthStencilState().depthTest != mCurDepthStencilState.depthTest)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DEPTH_FUNC:
                if (state.getDepthStencilState().depthFunc != mCurDepthStencilState.depthFunc)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED:
                if (state.getDepthStencilState().stencilTest != mCurDepthStencilState.stencilTest)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_FRONT:
            {
                const gl::DepthStencilState &depthStencil = state.getDepthStencilState();
                if (depthStencil.stencilFunc != mCurDepthStencilState.stencilFunc ||
                    depthStencil.stencilMask != mCurDepthStencilState.stencilMask ||
                    state.getStencilRef() != mCurStencilRef)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_BACK:
            {
                const gl::DepthStencilState &depthStencil = state.getDepthStencilState();
                if (depthStencil.stencilBackFunc != mCurDepthStencilState.stencilBackFunc ||
                    depthStencil.stencilBackMask != mCurDepthStencilState.stencilBackMask ||
                    state.getStencilBackRef() != mCurStencilBackRef)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT:
                if (state.getDepthStencilState().stencilWritemask !=
                    mCurDepthStencilState.stencilWritemask)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_BACK:
                if (state.getDepthStencilState().stencilBackWritemask !=
                    mCurDepthStencilState.stencilBackWritemask)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_FRONT:
            {
                const gl::DepthStencilState &depthStencil = state.getDepthStencilState();
                if (depthStencil.stencilFail != mCurDepthStencilState.stencilFail ||
                    depthStencil.stencilPassDepthFail !=
                        mCurDepthStencilState.stencilPassDepthFail ||
                    depthStencil.stencilPassDepthPass != mCurDepthStencilState.stencilPassDepthPass)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_OPS_BACK:
            {
                const gl::DepthStencilState &depthStencil = state.getDepthStencilState();
                if (depthStencil.stencilBackFail != mCurDepthStencilState.stencilBackFail ||
                    depthStencil.stencilBackPassDepthFail !=
                        mCurDepthStencilState.stencilBackPassDepthFail ||
                    depthStencil.stencilBackPassDepthPass !=
                        mCurDepthStencilState.stencilBackPassDepthPass)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_CULL_FACE_ENABLED:
                if (state.getRasterizerState().cullFace != mCurRasterState.cullFace)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_CULL_FACE:
                if (state.getRasterizerState().cullMode != mCurRasterState.cullMode)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_FRONT_FACE:
                if (state.getRasterizerState().frontFace != mCurRasterState.frontFace)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED:
                if (state.getRasterizerState().polygonOffsetFill !=
                    mCurRasterState.polygonOffsetFill)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET:
            {
                const gl::RasterizerState &rasterState = state.getRasterizerState();
                if (rasterState.polygonOffsetFactor != mCurRasterState.polygonOffsetFactor ||
                    rasterState.polygonOffsetUnits != mCurRasterState.polygonOffsetUnits)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED:
                if (state.getRasterizerState().rasterizerDiscard !=
                    mCurRasterState.rasterizerDiscard)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_SCISSOR:
                if (state.getScissor() != mCurScissorRect)
                {
                    mScissorStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_SCISSOR_TEST_ENABLED:
                if (state.isScissorTestEnabled() != mCurScissorEnabled)
                {
                    mScissorStateIsDirty = true;
                    // Rasterizer state update needs mCurScissorsEnabled and updates when it changes
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DEPTH_RANGE:
                if (state.getNearPlane() != mCurNear || state.getFarPlane() != mCurFar)
                {
                    mViewportStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_VIEWPORT:
                if (state.getViewport() != mCurViewport)
                {
                    mViewportStateIsDirty = true;
                }
                break;
            default:
                break;
        }
    }
}

gl::Error StateManager11::setBlendState(const gl::Framebuffer *framebuffer,
                                        const gl::BlendState &blendState,
                                        const gl::ColorF &blendColor,
                                        unsigned int sampleMask)
{
    if (!mBlendStateIsDirty && sampleMask == mCurSampleMask)
    {
        return gl::Error(GL_NO_ERROR);
    }

    ID3D11BlendState *dxBlendState = nullptr;
    gl::Error error = mStateCache->getBlendState(framebuffer, blendState, &dxBlendState);
    if (error.isError())
    {
        return error;
    }

    ASSERT(dxBlendState != nullptr);

    float blendColors[4] = {0.0f};
    if (blendState.sourceBlendRGB != GL_CONSTANT_ALPHA &&
        blendState.sourceBlendRGB != GL_ONE_MINUS_CONSTANT_ALPHA &&
        blendState.destBlendRGB != GL_CONSTANT_ALPHA &&
        blendState.destBlendRGB != GL_ONE_MINUS_CONSTANT_ALPHA)
    {
        blendColors[0] = blendColor.red;
        blendColors[1] = blendColor.green;
        blendColors[2] = blendColor.blue;
        blendColors[3] = blendColor.alpha;
    }
    else
    {
        blendColors[0] = blendColor.alpha;
        blendColors[1] = blendColor.alpha;
        blendColors[2] = blendColor.alpha;
        blendColors[3] = blendColor.alpha;
    }

    mDeviceContext->OMSetBlendState(dxBlendState, blendColors, sampleMask);

    mCurBlendState = blendState;
    mCurBlendColor = blendColor;
    mCurSampleMask = sampleMask;

    mBlendStateIsDirty = false;

    return error;
}

gl::Error StateManager11::setDepthStencilState(const gl::State &glState)
{
    const auto &fbo = *glState.getDrawFramebuffer();

    // Disable the depth test/depth write if we are using a stencil-only attachment.
    // This is because ANGLE emulates stencil-only with D24S8 on D3D11 - we should neither read
    // nor write to the unused depth part of this emulated texture.
    bool disableDepth = (!fbo.hasDepth() && fbo.hasStencil());

    // Similarly we disable the stencil portion of the DS attachment if the app only binds depth.
    bool disableStencil = (fbo.hasDepth() && !fbo.hasStencil());

    // CurDisableDepth/Stencil are reset automatically after we call forceSetDepthStencilState.
    if (!mDepthStencilStateIsDirty && mCurDisableDepth.valid() &&
        disableDepth == mCurDisableDepth.value() && mCurDisableStencil.valid() &&
        disableStencil == mCurDisableStencil.value())
    {
        return gl::Error(GL_NO_ERROR);
    }

    const auto &depthStencilState = glState.getDepthStencilState();
    int stencilRef                = glState.getStencilRef();
    int stencilBackRef            = glState.getStencilBackRef();

    // get the maximum size of the stencil ref
    unsigned int maxStencil = 0;
    if (depthStencilState.stencilTest && mCurStencilSize > 0)
    {
        maxStencil = (1 << mCurStencilSize) - 1;
    }
    ASSERT((depthStencilState.stencilWritemask & maxStencil) ==
           (depthStencilState.stencilBackWritemask & maxStencil));
    ASSERT(stencilRef == stencilBackRef);
    ASSERT((depthStencilState.stencilMask & maxStencil) ==
           (depthStencilState.stencilBackMask & maxStencil));

    ID3D11DepthStencilState *dxDepthStencilState = NULL;
    gl::Error error = mStateCache->getDepthStencilState(depthStencilState, disableDepth,
                                                        disableStencil, &dxDepthStencilState);
    if (error.isError())
    {
        return error;
    }

    ASSERT(dxDepthStencilState);

    // Max D3D11 stencil reference value is 0xFF,
    // corresponding to the max 8 bits in a stencil buffer
    // GL specifies we should clamp the ref value to the
    // nearest bit depth when doing stencil ops
    static_assert(D3D11_DEFAULT_STENCIL_READ_MASK == 0xFF,
                  "Unexpected value of D3D11_DEFAULT_STENCIL_READ_MASK");
    static_assert(D3D11_DEFAULT_STENCIL_WRITE_MASK == 0xFF,
                  "Unexpected value of D3D11_DEFAULT_STENCIL_WRITE_MASK");
    UINT dxStencilRef = std::min<UINT>(stencilRef, 0xFFu);

    mDeviceContext->OMSetDepthStencilState(dxDepthStencilState, dxStencilRef);

    mCurDepthStencilState = depthStencilState;
    mCurStencilRef        = stencilRef;
    mCurStencilBackRef    = stencilBackRef;
    mCurDisableDepth      = disableDepth;
    mCurDisableStencil    = disableStencil;

    mDepthStencilStateIsDirty = false;

    return gl::Error(GL_NO_ERROR);
}

gl::Error StateManager11::setRasterizerState(const gl::RasterizerState &rasterState)
{
    if (!mRasterizerStateIsDirty)
    {
        return gl::Error(GL_NO_ERROR);
    }

    ID3D11RasterizerState *dxRasterState = nullptr;
    gl::Error error(GL_NO_ERROR);

    if (mCurPresentPathFastEnabled)
    {
        gl::RasterizerState modifiedRasterState = rasterState;

        // If prseent path fast is active then we need invert the front face state.
        // This ensures that both gl_FrontFacing is correct, and front/back culling
        // is performed correctly.
        if (modifiedRasterState.frontFace == GL_CCW)
        {
            modifiedRasterState.frontFace = GL_CW;
        }
        else
        {
            ASSERT(modifiedRasterState.frontFace == GL_CW);
            modifiedRasterState.frontFace = GL_CCW;
        }

        error = mStateCache->getRasterizerState(modifiedRasterState, mCurScissorEnabled,
                                                &dxRasterState);
    }
    else
    {
        error = mStateCache->getRasterizerState(rasterState, mCurScissorEnabled, &dxRasterState);
    }

    if (error.isError())
    {
        return error;
    }

    mDeviceContext->RSSetState(dxRasterState);

    mCurRasterState         = rasterState;
    mRasterizerStateIsDirty = false;

    return error;
}

void StateManager11::setScissorRectangle(const gl::Rectangle &scissor, bool enabled)
{
    if (!mScissorStateIsDirty)
        return;

    int modifiedScissorY = scissor.y;
    if (mCurPresentPathFastEnabled)
    {
        modifiedScissorY = mCurPresentPathFastColorBufferHeight - scissor.height - scissor.y;
    }

    if (enabled)
    {
        D3D11_RECT rect;
        rect.left   = std::max(0, scissor.x);
        rect.top    = std::max(0, modifiedScissorY);
        rect.right  = scissor.x + std::max(0, scissor.width);
        rect.bottom = modifiedScissorY + std::max(0, scissor.height);

        mDeviceContext->RSSetScissorRects(1, &rect);
    }

    mCurScissorRect      = scissor;
    mCurScissorEnabled   = enabled;
    mScissorStateIsDirty = false;
}

void StateManager11::setViewport(const gl::Caps *caps,
                                 const gl::Rectangle &viewport,
                                 float zNear,
                                 float zFar)
{
    if (!mViewportStateIsDirty)
        return;

#ifdef ANGLE_ENABLE_WINDOWS_HOLOGRAPHIC
    // Temporary bug fix for holographic swap chains: Since we call this multiple
    // times per OpenGL draw call using different viewports, and the draw state does 
    // not need to be updated (in general) during that time, always ensure the 
    // viewport setting for ANGLE's fast present mode matches the viewport height.
    mCurPresentPathFastColorBufferHeight = viewport.height;
#endif

    float actualZNear = gl::clamp01(zNear);
    float actualZFar  = gl::clamp01(zFar);

    int dxMaxViewportBoundsX = static_cast<int>(caps->maxViewportWidth);
    int dxMaxViewportBoundsY = static_cast<int>(caps->maxViewportHeight);
    int dxMinViewportBoundsX = -dxMaxViewportBoundsX;
    int dxMinViewportBoundsY = -dxMaxViewportBoundsY;

    if (mRenderer11DeviceCaps->featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        // Feature Level 9 viewports shouldn't exceed the dimensions of the rendertarget.
        dxMaxViewportBoundsX = static_cast<int>(mViewportBounds.width);
        dxMaxViewportBoundsY = static_cast<int>(mViewportBounds.height);
        dxMinViewportBoundsX = 0;
        dxMinViewportBoundsY = 0;
    }

    int dxViewportTopLeftX = gl::clamp(viewport.x, dxMinViewportBoundsX, dxMaxViewportBoundsX);
    int dxViewportTopLeftY = gl::clamp(viewport.y, dxMinViewportBoundsY, dxMaxViewportBoundsY);
    int dxViewportWidth    = gl::clamp(viewport.width, 0, dxMaxViewportBoundsX - dxViewportTopLeftX);
    int dxViewportHeight   = gl::clamp(viewport.height, 0, dxMaxViewportBoundsY - dxViewportTopLeftY);

    D3D11_VIEWPORT dxViewport;
    dxViewport.TopLeftX = static_cast<float>(dxViewportTopLeftX);

    if (mCurPresentPathFastEnabled)
    {
        // When present path fast is active and we're rendering to framebuffer 0, we must invert
        // the viewport in Y-axis.
        // NOTE: We delay the inversion until right before the call to RSSetViewports, and leave
        // dxViewportTopLeftY unchanged. This allows us to calculate viewAdjust below using the
        // unaltered dxViewportTopLeftY value.
        dxViewport.TopLeftY = static_cast<float>(mCurPresentPathFastColorBufferHeight -
                                                 dxViewportTopLeftY - dxViewportHeight);
    }
    else
    {
        dxViewport.TopLeftY = static_cast<float>(dxViewportTopLeftY);
    }

    dxViewport.Width    = static_cast<float>(dxViewportWidth);
    dxViewport.Height   = static_cast<float>(dxViewportHeight);
    dxViewport.MinDepth = actualZNear;
    dxViewport.MaxDepth = actualZFar;

    mDeviceContext->RSSetViewports(1, &dxViewport);

    mCurViewport = viewport;
    mCurNear     = actualZNear;
    mCurFar      = actualZFar;

    // On Feature Level 9_*, we must emulate large and/or negative viewports in the shaders
    // using viewAdjust (like the D3D9 renderer).
    if (mRenderer11DeviceCaps->featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        mVertexConstants.viewAdjust[0] = static_cast<float>((viewport.width - dxViewportWidth) +
                                                            2 * (viewport.x - dxViewportTopLeftX)) /
                                         dxViewport.Width;
        mVertexConstants.viewAdjust[1] = static_cast<float>((viewport.height - dxViewportHeight) +
                                                            2 * (viewport.y - dxViewportTopLeftY)) /
                                         dxViewport.Height;
        mVertexConstants.viewAdjust[2] = static_cast<float>(viewport.width) / dxViewport.Width;
        mVertexConstants.viewAdjust[3] = static_cast<float>(viewport.height) / dxViewport.Height;
    }

    mPixelConstants.viewCoords[0] = viewport.width * 0.5f;
    mPixelConstants.viewCoords[1] = viewport.height * 0.5f;
    mPixelConstants.viewCoords[2] = viewport.x + (viewport.width * 0.5f);
    mPixelConstants.viewCoords[3] = viewport.y + (viewport.height * 0.5f);

    // Instanced pointsprite emulation requires ViewCoords to be defined in the
    // the vertex shader.
    mVertexConstants.viewCoords[0] = mPixelConstants.viewCoords[0];
    mVertexConstants.viewCoords[1] = mPixelConstants.viewCoords[1];
    mVertexConstants.viewCoords[2] = mPixelConstants.viewCoords[2];
    mVertexConstants.viewCoords[3] = mPixelConstants.viewCoords[3];

    mPixelConstants.depthFront[0] = (actualZFar - actualZNear) * 0.5f;
    mPixelConstants.depthFront[1] = (actualZNear + actualZFar) * 0.5f;

    mVertexConstants.depthRange[0] = actualZNear;
    mVertexConstants.depthRange[1] = actualZFar;
    mVertexConstants.depthRange[2] = actualZFar - actualZNear;

    mPixelConstants.depthRange[0] = actualZNear;
    mPixelConstants.depthRange[1] = actualZFar;
    mPixelConstants.depthRange[2] = actualZFar - actualZNear;

    mPixelConstants.viewScale[0] = 1.0f;
    mPixelConstants.viewScale[1] = mCurPresentPathFastEnabled ? 1.0f : -1.0f;
    mPixelConstants.viewScale[2] = 1.0f;
    mPixelConstants.viewScale[3] = 1.0f;

    mVertexConstants.viewScale[0] = mPixelConstants.viewScale[0];
    mVertexConstants.viewScale[1] = mPixelConstants.viewScale[1];
    mVertexConstants.viewScale[2] = mPixelConstants.viewScale[2];
    mVertexConstants.viewScale[3] = mPixelConstants.viewScale[3];

    mViewportStateIsDirty = false;
}

}  // namespace rx
