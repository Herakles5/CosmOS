#include "d3d9_surreal.h"
#include "surreal_gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Direct3D9Impl {
    struct IDirect3D9Vtbl* vtbl;
    ULONG ref_count;
} Direct3D9Impl;

typedef struct Direct3DDevice9Impl {
    struct IDirect3DDevice9Vtbl* vtbl;
    ULONG ref_count;
    D3DPRESENT_PARAMETERS params;
    uint32_t current_fvf;
} Direct3DDevice9Impl;

typedef struct Direct3DTexture9Impl {
    struct IDirect3DTexture9Vtbl* vtbl;
    ULONG ref_count;
    uint32_t width;
    uint32_t height;
    D3DFORMAT format;
    uint32_t* pixels;
} Direct3DTexture9Impl;

static IDirect3DTexture9* g_bound_texture = NULL;

static HRESULT WINAPI tex_QueryInterface(IDirect3DTexture9* me, const void* riid, void** ppvObj) {
    if (!ppvObj) return D3DERR_INVALIDCALL;
    *ppvObj = me;
    return D3D_OK;
}

static ULONG WINAPI tex_AddRef(IDirect3DTexture9* me) {
    Direct3DTexture9Impl* impl = (Direct3DTexture9Impl*)me;
    return ++(impl->ref_count);
}

static ULONG WINAPI tex_Release(IDirect3DTexture9* me) {
    Direct3DTexture9Impl* impl = (Direct3DTexture9Impl*)me;
    if (--(impl->ref_count) == 0) {
        if (impl->pixels) free(impl->pixels);
        free(impl->vtbl);
        free(impl);
        return 0;
    }
    return impl->ref_count;
}

static HRESULT WINAPI tex_GetDevice(IDirect3DTexture9* me, IDirect3DDevice9** ppDevice) {
    return D3D_OK;
}

static HRESULT WINAPI tex_LockRect(IDirect3DTexture9* me, uint32_t Level, D3DLOCKED_RECT* pLockedRect, const D3DRECT* pRect, uint32_t Flags) {
    Direct3DTexture9Impl* impl = (Direct3DTexture9Impl*)me;
    if (!pLockedRect || !impl->pixels) return D3DERR_INVALIDCALL;
    pLockedRect->Pitch = impl->width * 4;
    pLockedRect->pBits = (void*)impl->pixels;
    return D3D_OK;
}

static HRESULT WINAPI tex_UnlockRect(IDirect3DTexture9* me, uint32_t Level) {
    return D3D_OK;
}

static HRESULT WINAPI dev_QueryInterface(IDirect3DDevice9* me, const void* riid, void** ppvObj) {
    if (!ppvObj) return D3DERR_INVALIDCALL;
    *ppvObj = me;
    return D3D_OK;
}

static ULONG WINAPI dev_AddRef(IDirect3DDevice9* me) {
    Direct3DDevice9Impl* impl = (Direct3DDevice9Impl*)me;
    return ++(impl->ref_count);
}

static ULONG WINAPI dev_Release(IDirect3DDevice9* me) {
    Direct3DDevice9Impl* impl = (Direct3DDevice9Impl*)me;
    if (--(impl->ref_count) == 0) {
        free(impl->vtbl);
        free(impl);
        return 0;
    }
    return impl->ref_count;
}

static HRESULT WINAPI dev_TestCooperativeLevel(IDirect3DDevice9* me) { return D3D_OK; }
static uint32_t WINAPI dev_GetAvailableTextureMem(IDirect3DDevice9* me) { return 512 * 1024 * 1024; }
static HRESULT WINAPI dev_EvictManagedResources(IDirect3DDevice9* me) { return D3D_OK; }

static HRESULT WINAPI dev_BeginScene(IDirect3DDevice9* me) { return D3D_OK; }
static HRESULT WINAPI dev_EndScene(IDirect3DDevice9* me) { return D3D_OK; }

static HRESULT WINAPI dev_Clear(IDirect3DDevice9* me, uint32_t Count, const D3DRECT* pRects, uint32_t Flags, uint32_t Color, float Z, uint32_t Stencil) {
    sglClear(Color);
    sglClearDepth();
    return D3D_OK;
}

static HRESULT WINAPI dev_Present(IDirect3DDevice9* me, const D3DRECT* pSourceRect, const D3DRECT* pDestRect, void* hDestWindowOverride, const void* pDirtyRegion) {
    return D3D_OK;
}

static HRESULT WINAPI dev_SetTransform(IDirect3DDevice9* me, D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;
    if (State == D3DTS_PROJECTION) {
        sglPerspective(60.0f, 4.0f / 3.0f, 0.1f, 100.0f);
    } else if (State == D3DTS_WORLD || State == D3DTS_VIEW) {
        sglLoadIdentity();
        sglTranslatef(pMatrix->_41, pMatrix->_42, pMatrix->_43);
    }
    return D3D_OK;
}

static HRESULT WINAPI dev_SetRenderState(IDirect3DDevice9* me, D3DRENDERSTATETYPE State, uint32_t Value) {
    if (State == D3DRS_ZENABLE) {
        if (Value) sglEnable(SGL_DEPTH_TEST);
        else sglDisable(SGL_DEPTH_TEST);
    } else if (State == D3DRS_LIGHTING) {
        if (Value) sglEnable(SGL_LIGHTING);
        else sglDisable(SGL_LIGHTING);
    }
    return D3D_OK;
}

static HRESULT WINAPI dev_SetFVF(IDirect3DDevice9* me, uint32_t FVF) {
    Direct3DDevice9Impl* impl = (Direct3DDevice9Impl*)me;
    impl->current_fvf = FVF;
    return D3D_OK;
}

static HRESULT WINAPI dev_DrawPrimitiveUP(IDirect3DDevice9* me, D3DPRIMITIVETYPE PrimitiveType, uint32_t PrimitiveCount, const void* pVertexStreamZeroData, uint32_t VertexStreamZeroStride) {
    if (!pVertexStreamZeroData) return D3DERR_INVALIDCALL;

    int mode = SGL_TRIANGLES;
    uint32_t vertex_count = PrimitiveCount * 3;
    if (PrimitiveType == D3DPT_POINTLIST) { mode = SGL_POINTS; vertex_count = PrimitiveCount; }
    else if (PrimitiveType == D3DPT_TRIANGLESTRIP) { vertex_count = PrimitiveCount + 2; }

    sglBegin(mode);
    const uint8_t* ptr = (const uint8_t*)pVertexStreamZeroData;
    for (uint32_t i = 0; i < vertex_count; i++) {
        const D3DVertexXYZColor* v = (const D3DVertexXYZColor*)(ptr + i * VertexStreamZeroStride);
        uint32_t col = v->color;
        uint8_t r = (col >> 16) & 0xFF;
        uint8_t g = (col >> 8) & 0xFF;
        uint8_t b = col & 0xFF;
        sglColor3ub(r, g, b);
        sglVertex3f(v->x, v->y, v->z);
    }
    sglEnd();
    return D3D_OK;
}

static HRESULT WINAPI dev_GetDirect3D(IDirect3DDevice9* me, IDirect3D9** ppD3D9) { return D3D_OK; }
static HRESULT WINAPI dev_GetDeviceCaps(IDirect3DDevice9* me, void* pCaps) { return D3D_OK; }
static HRESULT WINAPI dev_GetDisplayMode(IDirect3DDevice9* me, uint32_t iSwapChain, void* pMode) { return D3D_OK; }
static HRESULT WINAPI dev_GetCreationParameters(IDirect3DDevice9* me, void* pParameters) { return D3D_OK; }
static HRESULT WINAPI dev_SetCursorProperties(IDirect3DDevice9* me, uint32_t XHotSpot, uint32_t YHotSpot, void* pCursorBitmap) { return D3D_OK; }
static void WINAPI dev_SetCursorPosition(IDirect3DDevice9* me, int X, int Y, uint32_t Flags) {}
static int WINAPI dev_ShowCursor(IDirect3DDevice9* me, int bShow) { return 1; }
static HRESULT WINAPI dev_CreateAdditionalSwapChain(IDirect3DDevice9* me, D3DPRESENT_PARAMETERS* pPresentationParameters, void** pSwapChain) { return D3D_OK; }
static HRESULT WINAPI dev_GetSwapChain(IDirect3DDevice9* me, uint32_t iSwapChain, void** pSwapChain) { return D3D_OK; }
static uint32_t WINAPI dev_GetNumberOfSwapChains(IDirect3DDevice9* me) { return 1; }
static HRESULT WINAPI dev_Reset(IDirect3DDevice9* me, D3DPRESENT_PARAMETERS* pPresentationParameters) { return D3D_OK; }
static HRESULT WINAPI dev_GetBackBuffer(IDirect3DDevice9* me, uint32_t iSwapChain, uint32_t iBackBuffer, uint32_t Type, void** ppBackBuffer) { return D3D_OK; }
static HRESULT WINAPI dev_GetRasterStatus(IDirect3DDevice9* me, uint32_t iSwapChain, void* pRasterStatus) { return D3D_OK; }
static HRESULT WINAPI dev_SetDialogBoxMode(IDirect3DDevice9* me, int bEnableDialogs) { return D3D_OK; }
static void WINAPI dev_SetGammaRamp(IDirect3DDevice9* me, uint32_t iSwapChain, uint32_t Flags, const void* pRamp) {}
static void WINAPI dev_GetGammaRamp(IDirect3DDevice9* me, uint32_t iSwapChain, void* pRamp) {}
static HRESULT WINAPI dev_CreateTexture(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, uint32_t Levels, uint32_t Usage, D3DFORMAT Format, uint32_t Pool, void** ppTexture, void* pSharedHandle) {
    if (!ppTexture) return D3DERR_INVALIDCALL;
    Direct3DTexture9Impl* tex = (Direct3DTexture9Impl*)malloc(sizeof(Direct3DTexture9Impl));
    struct IDirect3DTexture9Vtbl* vtbl = (struct IDirect3DTexture9Vtbl*)malloc(sizeof(struct IDirect3DTexture9Vtbl));
    memset(vtbl, 0, sizeof(struct IDirect3DTexture9Vtbl));

    vtbl->QueryInterface = tex_QueryInterface;
    vtbl->AddRef = tex_AddRef;
    vtbl->Release = tex_Release;
    vtbl->GetDevice = tex_GetDevice;
    vtbl->LockRect = tex_LockRect;
    vtbl->UnlockRect = tex_UnlockRect;

    tex->vtbl = vtbl;
    tex->ref_count = 1;
    tex->width = Width;
    tex->height = Height;
    tex->format = Format;
    tex->pixels = (uint32_t*)calloc(Width * Height, sizeof(uint32_t));

    *ppTexture = (void*)tex;
    printf("[D3D9] Created Texture %ux%u format=%u\n", Width, Height, (uint32_t)Format);
    return D3D_OK;
}
static HRESULT WINAPI dev_CreateVolumeTexture(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, uint32_t Depth, uint32_t Levels, uint32_t Usage, D3DFORMAT Format, uint32_t Pool, void** ppVolumeTexture, void* pSharedHandle) { return D3D_OK; }
static HRESULT WINAPI dev_CreateCubeTexture(IDirect3DDevice9* me, uint32_t EdgeLength, uint32_t Levels, uint32_t Usage, D3DFORMAT Format, uint32_t Pool, void** ppCubeTexture, void* pSharedHandle) { return D3D_OK; }
static HRESULT WINAPI dev_CreateVertexBuffer(IDirect3DDevice9* me, uint32_t Length, uint32_t Usage, uint32_t FVF, uint32_t Pool, void** ppVertexBuffer, void* pSharedHandle) { return D3D_OK; }
static HRESULT WINAPI dev_CreateIndexBuffer(IDirect3DDevice9* me, uint32_t Length, uint32_t Usage, D3DFORMAT Format, uint32_t Pool, void** ppIndexBuffer, void* pSharedHandle) { return D3D_OK; }
static HRESULT WINAPI dev_CreateRenderTarget(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, D3DFORMAT Format, uint32_t MultiSample, uint32_t MultisampleQuality, int Lockable, void** ppSurface, void* pSharedHandle) { return D3D_OK; }
static HRESULT WINAPI dev_CreateDepthStencilSurface(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, D3DFORMAT Format, uint32_t MultiSample, uint32_t MultisampleQuality, int Discard, void** ppSurface, void* pSharedHandle) { return D3D_OK; }
static HRESULT WINAPI dev_UpdateSurface(IDirect3DDevice9* me, void* pSourceSurface, const D3DRECT* pSourceRect, void* pDestinationSurface, const void* pDestPoint) { return D3D_OK; }
static HRESULT WINAPI dev_UpdateTexture(IDirect3DDevice9* me, void* pSourceTexture, void* pDestinationTexture) { return D3D_OK; }
static HRESULT WINAPI dev_GetRenderTargetData(IDirect3DDevice9* me, void* pRenderTarget, void* pDestSurface) { return D3D_OK; }
static HRESULT WINAPI dev_GetFrontBufferData(IDirect3DDevice9* me, uint32_t iSwapChain, void* pDestSurface) { return D3D_OK; }
static HRESULT WINAPI dev_StretchRect(IDirect3DDevice9* me, void* pSourceSurface, const D3DRECT* pSourceRect, void* pDestSurface, const D3DRECT* pDestRect, uint32_t Filter) { return D3D_OK; }
static HRESULT WINAPI dev_ColorFill(IDirect3DDevice9* me, void* pSurface, const D3DRECT* pRect, uint32_t color) { return D3D_OK; }
static HRESULT WINAPI dev_CreateOffscreenPlainSurface(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, D3DFORMAT Format, uint32_t Pool, void** ppSurface, void* pSharedHandle) { return D3D_OK; }
static HRESULT WINAPI dev_SetRenderTarget(IDirect3DDevice9* me, uint32_t RenderTargetIndex, void* pRenderTarget) { return D3D_OK; }
static HRESULT WINAPI dev_GetRenderTarget(IDirect3DDevice9* me, uint32_t RenderTargetIndex, void** ppRenderTarget) { return D3D_OK; }
static HRESULT WINAPI dev_SetDepthStencilSurface(IDirect3DDevice9* me, void* pNewZStencil) { return D3D_OK; }
static HRESULT WINAPI dev_GetDepthStencilSurface(IDirect3DDevice9* me, void** ppZStencilSurface) { return D3D_OK; }
static HRESULT WINAPI dev_GetTransform(IDirect3DDevice9* me, D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) { return D3D_OK; }
static HRESULT WINAPI dev_MultiplyTransform(IDirect3DDevice9* me, D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) { return D3D_OK; }
static HRESULT WINAPI dev_SetViewport(IDirect3DDevice9* me, const void* pViewport) { return D3D_OK; }
static HRESULT WINAPI dev_GetViewport(IDirect3DDevice9* me, void* pViewport) { return D3D_OK; }
static HRESULT WINAPI dev_SetMaterial(IDirect3DDevice9* me, const void* pMaterial) { return D3D_OK; }
static HRESULT WINAPI dev_GetMaterial(IDirect3DDevice9* me, void* pMaterial) { return D3D_OK; }
static HRESULT WINAPI dev_SetLight(IDirect3DDevice9* me, uint32_t Index, const void* pLight) { return D3D_OK; }
static HRESULT WINAPI dev_GetLight(IDirect3DDevice9* me, uint32_t Index, void* pLight) { return D3D_OK; }
static HRESULT WINAPI dev_LightEnable(IDirect3DDevice9* me, uint32_t Index, int Enable) { return D3D_OK; }
static HRESULT WINAPI dev_GetLightEnable(IDirect3DDevice9* me, uint32_t Index, int* pEnable) { return D3D_OK; }
static HRESULT WINAPI dev_GetRenderState(IDirect3DDevice9* me, D3DRENDERSTATETYPE State, uint32_t* pValue) { return D3D_OK; }
static HRESULT WINAPI dev_CreateStateBlock(IDirect3DDevice9* me, uint32_t Type, void** ppSB) { return D3D_OK; }
static HRESULT WINAPI dev_BeginStateBlock(IDirect3DDevice9* me) { return D3D_OK; }
static HRESULT WINAPI dev_EndStateBlock(IDirect3DDevice9* me, void** ppSB) { return D3D_OK; }
static HRESULT WINAPI dev_SetClipStatus(IDirect3DDevice9* me, const void* pClipStatus) { return D3D_OK; }
static HRESULT WINAPI dev_GetClipStatus(IDirect3DDevice9* me, void* pClipStatus) { return D3D_OK; }
static HRESULT WINAPI dev_GetTexture(IDirect3DDevice9* me, uint32_t Stage, void** ppTexture) { return D3D_OK; }
static HRESULT WINAPI dev_SetTexture(IDirect3DDevice9* me, uint32_t Stage, void* pTexture) {
    if (Stage == 0) {
        g_bound_texture = (IDirect3DTexture9*)pTexture;
    }
    return D3D_OK;
}
static HRESULT WINAPI dev_GetTextureStageState(IDirect3DDevice9* me, uint32_t Stage, uint32_t Type, uint32_t* pValue) { return D3D_OK; }
static HRESULT WINAPI dev_SetTextureStageState(IDirect3DDevice9* me, uint32_t Stage, uint32_t Type, uint32_t Value) { return D3D_OK; }
static HRESULT WINAPI dev_GetSamplerState(IDirect3DDevice9* me, uint32_t Sampler, uint32_t Type, uint32_t* pValue) { return D3D_OK; }
static HRESULT WINAPI dev_SetSamplerState(IDirect3DDevice9* me, uint32_t Sampler, uint32_t Type, uint32_t Value) { return D3D_OK; }
static HRESULT WINAPI dev_ValidateDevice(IDirect3DDevice9* me, uint32_t* pNumPasses) { return D3D_OK; }
static HRESULT WINAPI dev_SetPaletteEntries(IDirect3DDevice9* me, uint32_t PaletteNumber, const void* pEntries) { return D3D_OK; }
static HRESULT WINAPI dev_GetPaletteEntries(IDirect3DDevice9* me, uint32_t PaletteNumber, void* pEntries) { return D3D_OK; }
static HRESULT WINAPI dev_SetCurrentTexturePalette(IDirect3DDevice9* me, uint32_t PaletteNumber) { return D3D_OK; }
static HRESULT WINAPI dev_GetCurrentTexturePalette(IDirect3DDevice9* me, uint32_t* PaletteNumber) { return D3D_OK; }
static HRESULT WINAPI dev_DrawPrimitive(IDirect3DDevice9* me, D3DPRIMITIVETYPE PrimitiveType, uint32_t StartVertex, uint32_t PrimitiveCount) { return D3D_OK; }
static HRESULT WINAPI dev_DrawIndexedPrimitive(IDirect3DDevice9* me, D3DPRIMITIVETYPE PrimitiveType, int BaseVertexIndex, uint32_t MinVertexIndex, uint32_t NumVertices, uint32_t startIndex, uint32_t primCount) { return D3D_OK; }
static HRESULT WINAPI dev_DrawIndexedPrimitiveUP(IDirect3DDevice9* me, D3DPRIMITIVETYPE PrimitiveType, uint32_t MinVertexIndex, uint32_t NumVertices, uint32_t PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, uint32_t VertexStreamZeroStride) { return D3D_OK; }
static HRESULT WINAPI dev_GetFVF(IDirect3DDevice9* me, uint32_t* pFVF) { return D3D_OK; }

static HRESULT WINAPI d3d9_QueryInterface(IDirect3D9* me, const void* riid, void** ppvObj) {
    if (!ppvObj) return D3DERR_INVALIDCALL;
    *ppvObj = me;
    return D3D_OK;
}

static ULONG WINAPI d3d9_AddRef(IDirect3D9* me) {
    Direct3D9Impl* impl = (Direct3D9Impl*)me;
    return ++(impl->ref_count);
}

static ULONG WINAPI d3d9_Release(IDirect3D9* me) {
    Direct3D9Impl* impl = (Direct3D9Impl*)me;
    if (--(impl->ref_count) == 0) {
        free(impl->vtbl);
        free(impl);
        return 0;
    }
    return impl->ref_count;
}

static HRESULT WINAPI d3d9_RegisterSoftwareDevice(IDirect3D9* me, void* pInitializeFunction) { return D3D_OK; }
static uint32_t WINAPI d3d9_GetAdapterCount(IDirect3D9* me) { return 1; }
static HRESULT WINAPI d3d9_GetAdapterIdentifier(IDirect3D9* me, uint32_t Adapter, uint32_t Flags, void* pIdentifier) { return D3D_OK; }
static uint32_t WINAPI d3d9_GetAdapterModeCount(IDirect3D9* me, uint32_t Adapter, D3DFORMAT Format) { return 1; }
static HRESULT WINAPI d3d9_EnumAdapterModes(IDirect3D9* me, uint32_t Adapter, D3DFORMAT Format, uint32_t Mode, void* pMode) { return D3D_OK; }
static HRESULT WINAPI d3d9_GetAdapterDisplayMode(IDirect3D9* me, uint32_t Adapter, void* pMode) { return D3D_OK; }
static HRESULT WINAPI d3d9_CheckDeviceType(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DevType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, int Windowed) { return D3D_OK; }
static HRESULT WINAPI d3d9_CheckDeviceFormat(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, uint32_t Usage, uint32_t RType, D3DFORMAT CheckFormat) { return D3D_OK; }
static HRESULT WINAPI d3d9_CheckDeviceMultiSampleType(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, int Windowed, uint32_t MultiSampleType, uint32_t* pQualityLevels) { return D3D_OK; }
static HRESULT WINAPI d3d9_CheckDepthStencilMatch(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT D3DFormat, D3DFORMAT DepthFormat) { return D3D_OK; }
static HRESULT WINAPI d3d9_CheckDeviceFormatConversion(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) { return D3D_OK; }
static HRESULT WINAPI d3d9_GetDeviceCaps(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, void* pCaps) { return D3D_OK; }
static void* WINAPI d3d9_GetAdapterMonitor(IDirect3D9* me, uint32_t Adapter) { return NULL; }

static HRESULT WINAPI d3d9_CreateDevice(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, void* hFocusWindow, uint32_t BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnDeviceInterface) {
    if (!ppReturnDeviceInterface) return D3DERR_INVALIDCALL;

    Direct3DDevice9Impl* dev = (Direct3DDevice9Impl*)malloc(sizeof(Direct3DDevice9Impl));
    struct IDirect3DDevice9Vtbl* vtbl = (struct IDirect3DDevice9Vtbl*)malloc(sizeof(struct IDirect3DDevice9Vtbl));
    memset(vtbl, 0, sizeof(struct IDirect3DDevice9Vtbl));

    vtbl->QueryInterface = dev_QueryInterface;
    vtbl->AddRef = dev_AddRef;
    vtbl->Release = dev_Release;
    vtbl->TestCooperativeLevel = dev_TestCooperativeLevel;
    vtbl->GetAvailableTextureMem = dev_GetAvailableTextureMem;
    vtbl->EvictManagedResources = dev_EvictManagedResources;
    vtbl->GetDirect3D = dev_GetDirect3D;
    vtbl->GetDeviceCaps = dev_GetDeviceCaps;
    vtbl->GetDisplayMode = dev_GetDisplayMode;
    vtbl->GetCreationParameters = dev_GetCreationParameters;
    vtbl->SetCursorProperties = dev_SetCursorProperties;
    vtbl->SetCursorPosition = dev_SetCursorPosition;
    vtbl->ShowCursor = dev_ShowCursor;
    vtbl->CreateAdditionalSwapChain = dev_CreateAdditionalSwapChain;
    vtbl->GetSwapChain = dev_GetSwapChain;
    vtbl->GetNumberOfSwapChains = dev_GetNumberOfSwapChains;
    vtbl->Reset = dev_Reset;
    vtbl->Present = dev_Present;
    vtbl->GetBackBuffer = dev_GetBackBuffer;
    vtbl->GetRasterStatus = dev_GetRasterStatus;
    vtbl->SetDialogBoxMode = dev_SetDialogBoxMode;
    vtbl->SetGammaRamp = dev_SetGammaRamp;
    vtbl->GetGammaRamp = dev_GetGammaRamp;
    vtbl->CreateTexture = dev_CreateTexture;
    vtbl->CreateVolumeTexture = dev_CreateVolumeTexture;
    vtbl->CreateCubeTexture = dev_CreateCubeTexture;
    vtbl->CreateVertexBuffer = dev_CreateVertexBuffer;
    vtbl->CreateIndexBuffer = dev_CreateIndexBuffer;
    vtbl->CreateRenderTarget = dev_CreateRenderTarget;
    vtbl->CreateDepthStencilSurface = dev_CreateDepthStencilSurface;
    vtbl->UpdateSurface = dev_UpdateSurface;
    vtbl->UpdateTexture = dev_UpdateTexture;
    vtbl->GetRenderTargetData = dev_GetRenderTargetData;
    vtbl->GetFrontBufferData = dev_GetFrontBufferData;
    vtbl->StretchRect = dev_StretchRect;
    vtbl->ColorFill = dev_ColorFill;
    vtbl->CreateOffscreenPlainSurface = dev_CreateOffscreenPlainSurface;
    vtbl->SetRenderTarget = dev_SetRenderTarget;
    vtbl->GetRenderTarget = dev_GetRenderTarget;
    vtbl->SetDepthStencilSurface = dev_SetDepthStencilSurface;
    vtbl->GetDepthStencilSurface = dev_GetDepthStencilSurface;
    vtbl->BeginScene = dev_BeginScene;
    vtbl->EndScene = dev_EndScene;
    vtbl->Clear = dev_Clear;
    vtbl->SetTransform = dev_SetTransform;
    vtbl->GetTransform = dev_GetTransform;
    vtbl->MultiplyTransform = dev_MultiplyTransform;
    vtbl->SetViewport = dev_SetViewport;
    vtbl->GetViewport = dev_GetViewport;
    vtbl->SetMaterial = dev_SetMaterial;
    vtbl->GetMaterial = dev_GetMaterial;
    vtbl->SetLight = dev_SetLight;
    vtbl->GetLight = dev_GetLight;
    vtbl->LightEnable = dev_LightEnable;
    vtbl->GetLightEnable = dev_GetLightEnable;
    vtbl->SetRenderState = dev_SetRenderState;
    vtbl->GetRenderState = dev_GetRenderState;
    vtbl->CreateStateBlock = dev_CreateStateBlock;
    vtbl->BeginStateBlock = dev_BeginStateBlock;
    vtbl->EndStateBlock = dev_EndStateBlock;
    vtbl->SetClipStatus = dev_SetClipStatus;
    vtbl->GetClipStatus = dev_GetClipStatus;
    vtbl->GetTexture = dev_GetTexture;
    vtbl->SetTexture = dev_SetTexture;
    vtbl->GetTextureStageState = dev_GetTextureStageState;
    vtbl->SetTextureStageState = dev_SetTextureStageState;
    vtbl->GetSamplerState = dev_GetSamplerState;
    vtbl->SetSamplerState = dev_SetSamplerState;
    vtbl->ValidateDevice = dev_ValidateDevice;
    vtbl->SetPaletteEntries = dev_SetPaletteEntries;
    vtbl->GetPaletteEntries = dev_GetPaletteEntries;
    vtbl->SetCurrentTexturePalette = dev_SetCurrentTexturePalette;
    vtbl->GetCurrentTexturePalette = dev_GetCurrentTexturePalette;
    vtbl->DrawPrimitive = dev_DrawPrimitive;
    vtbl->DrawIndexedPrimitive = dev_DrawIndexedPrimitive;
    vtbl->DrawPrimitiveUP = dev_DrawPrimitiveUP;
    vtbl->DrawIndexedPrimitiveUP = dev_DrawIndexedPrimitiveUP;
    vtbl->SetFVF = dev_SetFVF;
    vtbl->GetFVF = dev_GetFVF;

    dev->vtbl = vtbl;
    dev->ref_count = 1;
    if (pPresentationParameters) dev->params = *pPresentationParameters;
    dev->current_fvf = 0;

    *ppReturnDeviceInterface = (IDirect3DDevice9*)dev;
    printf("[D3D9] Created Direct3DDevice9 hooked to SurrealGL!\n");
    return D3D_OK;
}

extern "C" WINAPI IDirect3D9* Direct3DCreate9(uint32_t SDKVersion) {
    Direct3D9Impl* d3d = (Direct3D9Impl*)malloc(sizeof(Direct3D9Impl));
    struct IDirect3D9Vtbl* vtbl = (struct IDirect3D9Vtbl*)malloc(sizeof(struct IDirect3D9Vtbl));
    memset(vtbl, 0, sizeof(struct IDirect3D9Vtbl));

    vtbl->QueryInterface = d3d9_QueryInterface;
    vtbl->AddRef = d3d9_AddRef;
    vtbl->Release = d3d9_Release;
    vtbl->RegisterSoftwareDevice = d3d9_RegisterSoftwareDevice;
    vtbl->GetAdapterCount = d3d9_GetAdapterCount;
    vtbl->GetAdapterIdentifier = d3d9_GetAdapterIdentifier;
    vtbl->GetAdapterModeCount = d3d9_GetAdapterModeCount;
    vtbl->EnumAdapterModes = d3d9_EnumAdapterModes;
    vtbl->GetAdapterDisplayMode = d3d9_GetAdapterDisplayMode;
    vtbl->CheckDeviceType = d3d9_CheckDeviceType;
    vtbl->CheckDeviceFormat = d3d9_CheckDeviceFormat;
    vtbl->CheckDeviceMultiSampleType = d3d9_CheckDeviceMultiSampleType;
    vtbl->CheckDepthStencilMatch = d3d9_CheckDepthStencilMatch;
    vtbl->CheckDeviceFormatConversion = d3d9_CheckDeviceFormatConversion;
    vtbl->GetDeviceCaps = d3d9_GetDeviceCaps;
    vtbl->GetAdapterMonitor = d3d9_GetAdapterMonitor;
    vtbl->CreateDevice = d3d9_CreateDevice;

    d3d->vtbl = vtbl;
    d3d->ref_count = 1;

    printf("[D3D9] Direct3DCreate9 (SDK v%u) called -> IDirect3D9 initialized.\n", SDKVersion);
    return (IDirect3D9*)d3d;
}
