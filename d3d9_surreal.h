#ifndef D3D9_SURREAL_H
#define D3D9_SURREAL_H

#include <stdint.h>
#include <stddef.h>

#ifndef WINAPI
#define WINAPI __attribute__((ms_abi))
#endif

#define D3D_SDK_VERSION 32
#define D3D_OK 0
#define D3DERR_INVALIDCALL 0x8876086C
#define D3DERR_NOTAVAILABLE 0x8876086A

typedef int32_t HRESULT;
typedef uint32_t ULONG;

typedef enum D3DDEVTYPE {
    D3DDEVTYPE_HAL = 1,
    D3DDEVTYPE_REF = 2,
    D3DDEVTYPE_SW = 3
} D3DDEVTYPE;

typedef enum D3DFORMAT {
    D3DFMT_UNKNOWN = 0,
    D3DFMT_R8G8B8 = 20,
    D3DFMT_A8R8G8B8 = 21,
    D3DFMT_X8R8G8B8 = 22,
    D3DFMT_D16 = 80,
    D3DFMT_D24S8 = 75
} D3DFORMAT;

typedef enum D3DPRIMITIVETYPE {
    D3DPT_POINTLIST = 1,
    D3DPT_LINELIST = 2,
    D3DPT_LINESTRIP = 3,
    D3DPT_TRIANGLELIST = 4,
    D3DPT_TRIANGLESTRIP = 5,
    D3DPT_TRIANGLEFAN = 6
} D3DPRIMITIVETYPE;

typedef enum D3DTRANSFORMSTATETYPE {
    D3DTS_VIEW = 2,
    D3DTS_PROJECTION = 3,
    D3DTS_WORLD = 256
} D3DTRANSFORMSTATETYPE;

typedef enum D3DRENDERSTATETYPE {
    D3DRS_ZENABLE = 7,
    D3DRS_LIGHTING = 137,
    D3DRS_CULLMODE = 22
} D3DRENDERSTATETYPE;

typedef struct D3DMATRIX {
    float _11, _12, _13, _14;
    float _21, _22, _23, _24;
    float _31, _32, _33, _34;
    float _41, _42, _43, _44;
} D3DMATRIX;

typedef struct D3DRECT {
    int32_t x1, y1, x2, y2;
} D3DRECT;

typedef struct D3DPRESENT_PARAMETERS {
    uint32_t BackBufferWidth;
    uint32_t BackBufferHeight;
    D3DFORMAT BackBufferFormat;
    uint32_t BackBufferCount;
    uint32_t MultiSampleType;
    uint32_t MultiSampleQuality;
    uint32_t SwapEffect;
    void* hDeviceWindow;
    int Windowed;
    int EnableAutoDepthStencil;
    D3DFORMAT AutoDepthStencilFormat;
    uint32_t Flags;
    uint32_t FullScreen_RefreshRateInHz;
    uint32_t PresentationInterval;
} D3DPRESENT_PARAMETERS;

#define D3DFVF_XYZ 0x002
#define D3DFVF_DIFFUSE 0x040
#define D3DFVF_TEX1 0x100

typedef struct D3DLOCKED_RECT {
    int Pitch;
    void* pBits;
} D3DLOCKED_RECT;

struct D3DVertexXYZColor {
    float x, y, z;
    uint32_t color;
};

struct D3DVertexXYZColorTex {
    float x, y, z;
    uint32_t color;
    float u, v;
};

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DTexture9;

struct IDirect3DTexture9Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDirect3DTexture9* me, const void* riid, void** ppvObj);
    ULONG   (WINAPI *AddRef)(IDirect3DTexture9* me);
    ULONG   (WINAPI *Release)(IDirect3DTexture9* me);
    HRESULT (WINAPI *GetDevice)(IDirect3DTexture9* me, IDirect3DDevice9** ppDevice);
    HRESULT (WINAPI *LockRect)(IDirect3DTexture9* me, uint32_t Level, D3DLOCKED_RECT* pLockedRect, const D3DRECT* pRect, uint32_t Flags);
    HRESULT (WINAPI *UnlockRect)(IDirect3DTexture9* me, uint32_t Level);
};

struct IDirect3DTexture9 {
    const struct IDirect3DTexture9Vtbl* lpVtbl;
};

struct IDirect3D9Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDirect3D9* me, const void* riid, void** ppvObj);
    ULONG   (WINAPI *AddRef)(IDirect3D9* me);
    ULONG   (WINAPI *Release)(IDirect3D9* me);
    HRESULT (WINAPI *RegisterSoftwareDevice)(IDirect3D9* me, void* pInitializeFunction);
    uint32_t(WINAPI *GetAdapterCount)(IDirect3D9* me);
    HRESULT (WINAPI *GetAdapterIdentifier)(IDirect3D9* me, uint32_t Adapter, uint32_t Flags, void* pIdentifier);
    uint32_t(WINAPI *GetAdapterModeCount)(IDirect3D9* me, uint32_t Adapter, D3DFORMAT Format);
    HRESULT (WINAPI *EnumAdapterModes)(IDirect3D9* me, uint32_t Adapter, D3DFORMAT Format, uint32_t Mode, void* pMode);
    HRESULT (WINAPI *GetAdapterDisplayMode)(IDirect3D9* me, uint32_t Adapter, void* pMode);
    HRESULT (WINAPI *CheckDeviceType)(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DevType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, int Windowed);
    HRESULT (WINAPI *CheckDeviceFormat)(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, uint32_t Usage, uint32_t RType, D3DFORMAT CheckFormat);
    HRESULT (WINAPI *CheckDeviceMultiSampleType)(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, int Windowed, uint32_t MultiSampleType, uint32_t* pQualityLevels);
    HRESULT (WINAPI *CheckDepthStencilMatch)(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT D3DFormat, D3DFORMAT DepthFormat);
    HRESULT (WINAPI *CheckDeviceFormatConversion)(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat);
    HRESULT (WINAPI *GetDeviceCaps)(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, void* pCaps);
    void*   (WINAPI *GetAdapterMonitor)(IDirect3D9* me, uint32_t Adapter);
    HRESULT (WINAPI *CreateDevice)(IDirect3D9* me, uint32_t Adapter, D3DDEVTYPE DeviceType, void* hFocusWindow, uint32_t BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnDeviceInterface);
};

struct IDirect3D9 {
    const struct IDirect3D9Vtbl* lpVtbl;
};

struct IDirect3DDevice9Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDirect3DDevice9* me, const void* riid, void** ppvObj);
    ULONG   (WINAPI *AddRef)(IDirect3DDevice9* me);
    ULONG   (WINAPI *Release)(IDirect3DDevice9* me);
    HRESULT (WINAPI *TestCooperativeLevel)(IDirect3DDevice9* me);
    uint32_t(WINAPI *GetAvailableTextureMem)(IDirect3DDevice9* me);
    HRESULT (WINAPI *EvictManagedResources)(IDirect3DDevice9* me);
    HRESULT (WINAPI *GetDirect3D)(IDirect3DDevice9* me, IDirect3D9** ppD3D9);
    HRESULT (WINAPI *GetDeviceCaps)(IDirect3DDevice9* me, void* pCaps);
    HRESULT (WINAPI *GetDisplayMode)(IDirect3DDevice9* me, uint32_t iSwapChain, void* pMode);
    HRESULT (WINAPI *GetCreationParameters)(IDirect3DDevice9* me, void* pParameters);
    HRESULT (WINAPI *SetCursorProperties)(IDirect3DDevice9* me, uint32_t XHotSpot, uint32_t YHotSpot, void* pCursorBitmap);
    void    (WINAPI *SetCursorPosition)(IDirect3DDevice9* me, int X, int Y, uint32_t Flags);
    int     (WINAPI *ShowCursor)(IDirect3DDevice9* me, int bShow);
    HRESULT (WINAPI *CreateAdditionalSwapChain)(IDirect3DDevice9* me, D3DPRESENT_PARAMETERS* pPresentationParameters, void** pSwapChain);
    HRESULT (WINAPI *GetSwapChain)(IDirect3DDevice9* me, uint32_t iSwapChain, void** pSwapChain);
    uint32_t(WINAPI *GetNumberOfSwapChains)(IDirect3DDevice9* me);
    HRESULT (WINAPI *Reset)(IDirect3DDevice9* me, D3DPRESENT_PARAMETERS* pPresentationParameters);
    HRESULT (WINAPI *Present)(IDirect3DDevice9* me, const D3DRECT* pSourceRect, const D3DRECT* pDestRect, void* hDestWindowOverride, const void* pDirtyRegion);
    HRESULT (WINAPI *GetBackBuffer)(IDirect3DDevice9* me, uint32_t iSwapChain, uint32_t iBackBuffer, uint32_t Type, void** ppBackBuffer);
    HRESULT (WINAPI *GetRasterStatus)(IDirect3DDevice9* me, uint32_t iSwapChain, void* pRasterStatus);
    HRESULT (WINAPI *SetDialogBoxMode)(IDirect3DDevice9* me, int bEnableDialogs);
    void    (WINAPI *SetGammaRamp)(IDirect3DDevice9* me, uint32_t iSwapChain, uint32_t Flags, const void* pRamp);
    void    (WINAPI *GetGammaRamp)(IDirect3DDevice9* me, uint32_t iSwapChain, void* pRamp);
    HRESULT (WINAPI *CreateTexture)(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, uint32_t Levels, uint32_t Usage, D3DFORMAT Format, uint32_t Pool, void** ppTexture, void* pSharedHandle);
    HRESULT (WINAPI *CreateVolumeTexture)(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, uint32_t Depth, uint32_t Levels, uint32_t Usage, D3DFORMAT Format, uint32_t Pool, void** ppVolumeTexture, void* pSharedHandle);
    HRESULT (WINAPI *CreateCubeTexture)(IDirect3DDevice9* me, uint32_t EdgeLength, uint32_t Levels, uint32_t Usage, D3DFORMAT Format, uint32_t Pool, void** ppCubeTexture, void* pSharedHandle);
    HRESULT (WINAPI *CreateVertexBuffer)(IDirect3DDevice9* me, uint32_t Length, uint32_t Usage, uint32_t FVF, uint32_t Pool, void** ppVertexBuffer, void* pSharedHandle);
    HRESULT (WINAPI *CreateIndexBuffer)(IDirect3DDevice9* me, uint32_t Length, uint32_t Usage, D3DFORMAT Format, uint32_t Pool, void** ppIndexBuffer, void* pSharedHandle);
    HRESULT (WINAPI *CreateRenderTarget)(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, D3DFORMAT Format, uint32_t MultiSample, uint32_t MultisampleQuality, int Lockable, void** ppSurface, void* pSharedHandle);
    HRESULT (WINAPI *CreateDepthStencilSurface)(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, D3DFORMAT Format, uint32_t MultiSample, uint32_t MultisampleQuality, int Discard, void** ppSurface, void* pSharedHandle);
    HRESULT (WINAPI *UpdateSurface)(IDirect3DDevice9* me, void* pSourceSurface, const D3DRECT* pSourceRect, void* pDestinationSurface, const void* pDestPoint);
    HRESULT (WINAPI *UpdateTexture)(IDirect3DDevice9* me, void* pSourceTexture, void* pDestinationTexture);
    HRESULT (WINAPI *GetRenderTargetData)(IDirect3DDevice9* me, void* pRenderTarget, void* pDestSurface);
    HRESULT (WINAPI *GetFrontBufferData)(IDirect3DDevice9* me, uint32_t iSwapChain, void* pDestSurface);
    HRESULT (WINAPI *StretchRect)(IDirect3DDevice9* me, void* pSourceSurface, const D3DRECT* pSourceRect, void* pDestSurface, const D3DRECT* pDestRect, uint32_t Filter);
    HRESULT (WINAPI *ColorFill)(IDirect3DDevice9* me, void* pSurface, const D3DRECT* pRect, uint32_t color);
    HRESULT (WINAPI *CreateOffscreenPlainSurface)(IDirect3DDevice9* me, uint32_t Width, uint32_t Height, D3DFORMAT Format, uint32_t Pool, void** ppSurface, void* pSharedHandle);
    HRESULT (WINAPI *SetRenderTarget)(IDirect3DDevice9* me, uint32_t RenderTargetIndex, void* pRenderTarget);
    HRESULT (WINAPI *GetRenderTarget)(IDirect3DDevice9* me, uint32_t RenderTargetIndex, void** ppRenderTarget);
    HRESULT (WINAPI *SetDepthStencilSurface)(IDirect3DDevice9* me, void* pNewZStencil);
    HRESULT (WINAPI *GetDepthStencilSurface)(IDirect3DDevice9* me, void** ppZStencilSurface);
    HRESULT (WINAPI *BeginScene)(IDirect3DDevice9* me);
    HRESULT (WINAPI *EndScene)(IDirect3DDevice9* me);
    HRESULT (WINAPI *Clear)(IDirect3DDevice9* me, uint32_t Count, const D3DRECT* pRects, uint32_t Flags, uint32_t Color, float Z, uint32_t Stencil);
    HRESULT (WINAPI *SetTransform)(IDirect3DDevice9* me, D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix);
    HRESULT (WINAPI *GetTransform)(IDirect3DDevice9* me, D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix);
    HRESULT (WINAPI *MultiplyTransform)(IDirect3DDevice9* me, D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix);
    HRESULT (WINAPI *SetViewport)(IDirect3DDevice9* me, const void* pViewport);
    HRESULT (WINAPI *GetViewport)(IDirect3DDevice9* me, void* pViewport);
    HRESULT (WINAPI *SetMaterial)(IDirect3DDevice9* me, const void* pMaterial);
    HRESULT (WINAPI *GetMaterial)(IDirect3DDevice9* me, void* pMaterial);
    HRESULT (WINAPI *SetLight)(IDirect3DDevice9* me, uint32_t Index, const void* pLight);
    HRESULT (WINAPI *GetLight)(IDirect3DDevice9* me, uint32_t Index, void* pLight);
    HRESULT (WINAPI *LightEnable)(IDirect3DDevice9* me, uint32_t Index, int Enable);
    HRESULT (WINAPI *GetLightEnable)(IDirect3DDevice9* me, uint32_t Index, int* pEnable);
    HRESULT (WINAPI *SetRenderState)(IDirect3DDevice9* me, D3DRENDERSTATETYPE State, uint32_t Value);
    HRESULT (WINAPI *GetRenderState)(IDirect3DDevice9* me, D3DRENDERSTATETYPE State, uint32_t* pValue);
    HRESULT (WINAPI *CreateStateBlock)(IDirect3DDevice9* me, uint32_t Type, void** ppSB);
    HRESULT (WINAPI *BeginStateBlock)(IDirect3DDevice9* me);
    HRESULT (WINAPI *EndStateBlock)(IDirect3DDevice9* me, void** ppSB);
    HRESULT (WINAPI *SetClipStatus)(IDirect3DDevice9* me, const void* pClipStatus);
    HRESULT (WINAPI *GetClipStatus)(IDirect3DDevice9* me, void* pClipStatus);
    HRESULT (WINAPI *GetTexture)(IDirect3DDevice9* me, uint32_t Stage, void** ppTexture);
    HRESULT (WINAPI *SetTexture)(IDirect3DDevice9* me, uint32_t Stage, void* pTexture);
    HRESULT (WINAPI *GetTextureStageState)(IDirect3DDevice9* me, uint32_t Stage, uint32_t Type, uint32_t* pValue);
    HRESULT (WINAPI *SetTextureStageState)(IDirect3DDevice9* me, uint32_t Stage, uint32_t Type, uint32_t Value);
    HRESULT (WINAPI *GetSamplerState)(IDirect3DDevice9* me, uint32_t Sampler, uint32_t Type, uint32_t* pValue);
    HRESULT (WINAPI *SetSamplerState)(IDirect3DDevice9* me, uint32_t Sampler, uint32_t Type, uint32_t Value);
    HRESULT (WINAPI *ValidateDevice)(IDirect3DDevice9* me, uint32_t* pNumPasses);
    HRESULT (WINAPI *SetPaletteEntries)(IDirect3DDevice9* me, uint32_t PaletteNumber, const void* pEntries);
    HRESULT (WINAPI *GetPaletteEntries)(IDirect3DDevice9* me, uint32_t PaletteNumber, void* pEntries);
    HRESULT (WINAPI *SetCurrentTexturePalette)(IDirect3DDevice9* me, uint32_t PaletteNumber);
    HRESULT (WINAPI *GetCurrentTexturePalette)(IDirect3DDevice9* me, uint32_t* PaletteNumber);
    HRESULT (WINAPI *DrawPrimitive)(IDirect3DDevice9* me, D3DPRIMITIVETYPE PrimitiveType, uint32_t StartVertex, uint32_t PrimitiveCount);
    HRESULT (WINAPI *DrawIndexedPrimitive)(IDirect3DDevice9* me, D3DPRIMITIVETYPE PrimitiveType, int BaseVertexIndex, uint32_t MinVertexIndex, uint32_t NumVertices, uint32_t startIndex, uint32_t primCount);
    HRESULT (WINAPI *DrawPrimitiveUP)(IDirect3DDevice9* me, D3DPRIMITIVETYPE PrimitiveType, uint32_t PrimitiveCount, const void* pVertexStreamZeroData, uint32_t VertexStreamZeroStride);
    HRESULT (WINAPI *DrawIndexedPrimitiveUP)(IDirect3DDevice9* me, D3DPRIMITIVETYPE PrimitiveType, uint32_t MinVertexIndex, uint32_t NumVertices, uint32_t PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, uint32_t VertexStreamZeroStride);
    HRESULT (WINAPI *SetFVF)(IDirect3DDevice9* me, uint32_t FVF);
    HRESULT (WINAPI *GetFVF)(IDirect3DDevice9* me, uint32_t* pFVF);
};

struct IDirect3DDevice9 {
    const struct IDirect3DDevice9Vtbl* lpVtbl;
};

extern "C" WINAPI IDirect3D9* Direct3DCreate9(uint32_t SDKVersion);

#endif // D3D9_SURREAL_H
