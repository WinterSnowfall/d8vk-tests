#include <map>
#include <array>
#include <iostream>

#include <d3d9.h>
#include <d3d9caps.h>

#include "../common/com.h"
#include "../common/error.h"
#include "../common/str.h"

struct RGBVERTEX {
    FLOAT x, y, z, rhw;
    DWORD colour;
};

#define RGBT_FVF_CODES (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

// D3D9 caps
#ifdef _MSC_VER
#define D3DCAPS2_CANAUTOGENMIPMAP               0x40000000L
#define D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION    0x00000080L
#define D3DCAPS3_COPY_TO_VIDMEM                 0x00000100L
#define D3DCAPS3_COPY_TO_SYSTEMMEM              0x00000200L
#define D3DPMISCCAPS_INDEPENDENTWRITEMASKS      0x00004000L
#define D3DPMISCCAPS_PERSTAGECONSTANT           0x00008000L
#define D3DPMISCCAPS_FOGANDSPECULARALPHA        0x00010000L
#define D3DPMISCCAPS_SEPARATEALPHABLEND         0x00020000L
#define D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS    0x00040000L
#define D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING 0x00080000L
#define D3DPMISCCAPS_FOGVERTEXCLAMPED           0x00100000L

#define D3DPRASTERCAPS_SCISSORTEST              0x01000000L
#define D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS      0x02000000L
#define D3DPRASTERCAPS_DEPTHBIAS                0x04000000L
#define D3DPRASTERCAPS_MULTISAMPLE_TOGGLE       0x08000000L

#define D3DLINECAPS_ANTIALIAS                   0x00000020L
#define D3DSTENCILCAPS_TWOSIDED                 0x00000100L
#define D3DPMISCCAPS_POSTBLENDSRGBCONVERT       0x00200000L
#define D3DPBLENDCAPS_BLENDFACTOR               0x00002000L
#define D3DPBLENDCAPS_SRCCOLOR2                 0x00004000L
#define D3DPBLENDCAPS_INVSRCCOLOR2              0x00008000L

#define D3DVTXPCAPS_TEXGEN_SPHEREMAP            0x00000100L
#define D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER    0x00000200L
#endif

class RGBTriangle {

    public:

        static const UINT WINDOW_WIDTH  = 700;
        static const UINT WINDOW_HEIGHT = 700;

        RGBTriangle(HWND hWnd) : m_hWnd(hWnd) {
            decltype(Direct3DCreate9)* Direct3DCreate9 = nullptr;
            HMODULE hm = LoadLibraryA("d3d9.dll");
            Direct3DCreate9 = (decltype(Direct3DCreate9))GetProcAddress(hm, "Direct3DCreate9");

            // D3D Interface
            m_d3d = Direct3DCreate9(D3D_SDK_VERSION);
            if (m_d3d.ptr() == nullptr)
                throw Error("Failed to create D3D9 interface");

            D3DADAPTER_IDENTIFIER9 adapterId;
            HRESULT status = m_d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterId);
            if (FAILED(status))
                throw Error("Failed to get D3D9 adapter identifier");

            std::cout << format("Using adapter: ", adapterId.Description) << std::endl;

            // DWORD to hex printout
            char vendorID[16];
            char deviceID[16];
            sprintf(vendorID, "VendorId: %04lx", adapterId.VendorId);
            sprintf(deviceID, "DeviceId: %04lx", adapterId.DeviceId);
            std::cout << "  ~ " << vendorID << std::endl;
            std::cout << "  ~ " << deviceID << std::endl;

            std::cout << format("  ~ Driver: ", adapterId.Driver) << std::endl;

            // NVIDIA stores the driver version in the lower half of the lower DWORD
            if (adapterId.VendorId == uint32_t(0x10de)) {
                // Newer drivers will also spill over into the upper half
                DWORD driverVersionHigh = HIWORD(adapterId.DriverVersion.LowPart);
                DWORD driverVersionLow  = LOWORD(adapterId.DriverVersion.LowPart);
                DWORD majorVersion = driverVersionLow / 100;
                // Might want to revisit this cursed logic, but it should work fine for now
                if (driverVersionHigh >= 15 && driverVersionLow < 10000)
                    majorVersion += (driverVersionHigh % 10) * 100;
                DWORD minorVersion = driverVersionLow % 100;
                std::cout << format("  ~ Version: ", majorVersion, ".", minorVersion) << std::endl;
            }
            // for other vendors simply list the entire DriverVersion long int
            else {
                DWORD product = HIWORD(adapterId.DriverVersion.HighPart);
                DWORD version = LOWORD(adapterId.DriverVersion.HighPart);
                DWORD subVersion = HIWORD(adapterId.DriverVersion.LowPart);
                DWORD build = LOWORD(adapterId.DriverVersion.LowPart);
                std::cout << format("  ~ Version: ", product, ".", version, ".",
                    subVersion, ".", build) << std::endl;
            }

            // D3D Device
            D3DDISPLAYMODE dm;
            status = m_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm);
            if (FAILED(status))
                throw Error("Failed to get D3D9 adapter display mode");

            ZeroMemory(&m_pp, sizeof(m_pp));

            m_pp.Windowed = TRUE;
            m_pp.hDeviceWindow = m_hWnd;
            // set to D3DSWAPEFFECT_COPY or D3DSWAPEFFECT_FLIP for no VSync
            m_pp.SwapEffect = D3DSWAPEFFECT_COPY;
            // according to D3D9 spec "0 is treated as 1" here
            m_pp.BackBufferCount = 0;
            m_pp.BackBufferWidth = WINDOW_WIDTH;
            m_pp.BackBufferHeight = WINDOW_HEIGHT;
            m_pp.BackBufferFormat = dm.Format;

            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, true);
        }

        // D3D Adapter Display Mode enumeration
        void listAdapterDisplayModes() {
            HRESULT          status;
            D3DDISPLAYMODE   amDM;
            UINT             totalAdapterModeCount = 0;
            UINT             adapterModeCount = 0;

            ZeroMemory(&amDM, sizeof(amDM));

            // these are all the possible adapter display formats, at least in theory
            std::map<D3DFORMAT, char const*> amDMFormats = { {D3DFMT_A1R5G5B5, "D3DFMT_A1R5G5B5"},
                                                             {D3DFMT_A2R10G10B10, "D3DFMT_A2R10G10B10"},
                                                             {D3DFMT_A8R8G8B8, "D3DFMT_A8R8G8B8"},
                                                             {D3DFMT_X8R8G8B8, "D3DFMT_X8R8G8B8"},
                                                             {D3DFMT_X1R5G5B5, "D3DFMT_X1R5G5B5"},
                                                             {D3DFMT_R5G6B5, "D3DFMT_R5G6B5"} };

            std::cout << std::endl << "Enumerating supported adapter display modes:" << std::endl;

            for (auto const& amDMFormat : amDMFormats) {
                adapterModeCount = m_d3d->GetAdapterModeCount(D3DADAPTER_DEFAULT, amDMFormat.first);
                totalAdapterModeCount += adapterModeCount;

                for (UINT i = 0; i < adapterModeCount; i++ ) {
                    status =  m_d3d->EnumAdapterModes(D3DADAPTER_DEFAULT, amDMFormat.first, i, &amDM);

                    if (FAILED(status)) {
                        std::cout << format("    Failed to get adapter display mode ", i) << std::endl;
                    } else {
                        std::cout << format("    ", amDMFormats[amDM.Format], " ", amDM.Width,
                                            " x ", amDM.Height, " @ ", amDM.RefreshRate, " Hz") << std::endl;
                    }
                }
            }

            std::cout << format("Listed a total of ", totalAdapterModeCount, " adapter display modes") << std::endl;
        }

        // D3D Device back buffer formats check
        void listBackBufferFormats(BOOL windowed) {
            resetOrRecreateDevice();

            HRESULT status;
            D3DPRESENT_PARAMETERS bbPP;

            memcpy(&bbPP, &m_pp, sizeof(m_pp));

            // these are all the possible backbuffer formats, at least in theory
            std::map<D3DFORMAT, char const*> bbFormats = { {D3DFMT_A8R8G8B8, "D3DFMT_A8R8G8B8"},
                                                           {D3DFMT_X8R8G8B8, "D3DFMT_X8R8G8B8"},
                                                           {D3DFMT_A1R5G5B5, "D3DFMT_A1R5G5B5"},
                                                           {D3DFMT_X1R5G5B5, "D3DFMT_X1R5G5B5"},
                                                           {D3DFMT_R5G6B5, "D3DFMT_R5G6B5"},
                                                           {D3DFMT_A2R10G10B10, "D3DFMT_A2R10G10B10"} };

            std::map<D3DFORMAT, char const*>::iterator bbFormatIter;

            std::cout << std::endl;
            if (windowed)
                std::cout << "Device back buffer format support (windowed):" << std::endl;
            else
                std::cout << "Device back buffer format support (full screen):" << std::endl;

            for (bbFormatIter = bbFormats.begin(); bbFormatIter != bbFormats.end(); bbFormatIter++) {
                status = m_d3d->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                bbFormatIter->first, bbFormatIter->first,
                                                windowed);
                if (FAILED(status)) {
                    std::cout << format("  - The ", bbFormatIter->second, " format is not supported") << std::endl;
                } else {
                    bbPP.BackBufferFormat = bbFormatIter->first;

                    status = createDeviceWithFlags(&bbPP, D3DCREATE_HARDWARE_VERTEXPROCESSING, false);
                    if (FAILED(status)) {
                        std::cout << format("  ! The ", bbFormatIter->second, " format is supported, device creation FAILED") << std::endl;
                    } else {
                        std::cout << format("  + The ", bbFormatIter->second, " format is supported, device creation SUCCEEDED") << std::endl;
                    }
                }
            }
        }

        // Obscure FOURCC surface format check
        void listObscureFOURCCSurfaceFormats() {
            resetOrRecreateDevice();

            std::map<uint32_t, char const*> sfFormats = { {MAKEFOURCC('A', 'I', '4', '4'), "AI44"},
                                                          {MAKEFOURCC('I', 'A', '4', '4'), "IA44"},
                                                          {MAKEFOURCC('R', '2', 'V', 'B'), "R2VB"},
                                                          {MAKEFOURCC('C', 'O', 'P', 'M'), "COPM"},
                                                          {MAKEFOURCC('S', 'S', 'A', 'A'), "SSAA"},
                                                          // following 2 formats are used by a lot of games
                                                          {MAKEFOURCC('A', 'L', '1', '6'), "AL16"},
                                                          {MAKEFOURCC(' ', 'R', '1', '6'), "R16"},
                                                          // wierd variant of the regular L16 (used by Scrapland)
                                                          {MAKEFOURCC(' ', 'L', '1', '6'), "L16"},
                                                          // undocumented format (used by Scrapland)
                                                          {MAKEFOURCC('A', 'R', '1', '6'), "AR16"} };

            std::map<uint32_t, char const*>::iterator sfFormatIter;

            Com<IDirect3DSurface9> surface;
            Com<IDirect3DTexture9> texture;
            Com<IDirect3DCubeTexture9> cubeTexture;
            Com<IDirect3DVolumeTexture9> volumeTexture;

            std::cout << std::endl << "Obscure FOURCC surface format support:" << std::endl;

            for (sfFormatIter = sfFormats.begin(); sfFormatIter != sfFormats.end(); sfFormatIter++) {
                D3DFORMAT surfaceFormat = (D3DFORMAT) sfFormatIter->first;

                std::cout << format("  ~ ", sfFormatIter->second, ":") << std::endl;

                HRESULT status = m_device->CreateOffscreenPlainSurface(256, 256, surfaceFormat, D3DPOOL_SCRATCH, &surface, NULL);

                if (FAILED(status)) {
                    std::cout << "     - The format is not supported by CreateOffscreenPlainSurface" << std::endl;
                } else {
                    std::cout << "     + The format is supported by CreateOffscreenPlainSurface" << std::endl;
                }

                status = m_device->CreateTexture(256, 256, 1, 0, surfaceFormat, D3DPOOL_DEFAULT, &texture, NULL);

                if (FAILED(status)) {
                    std::cout << "     - The format is not supported by CreateTexture" << std::endl;
                } else {
                    std::cout << "     + The format is supported by CreateTexture" << std::endl;
                }

                status = m_device->CreateCubeTexture(256, 1, 0, surfaceFormat, D3DPOOL_DEFAULT, &cubeTexture, NULL);

                if (FAILED(status)) {
                    std::cout << "     - The format is not supported by CreateCubeTexture" << std::endl;
                } else {
                    std::cout << "     + The format is supported by CreateCubeTexture" << std::endl;
                    cubeTexture = nullptr;
                }

                status = m_device->CreateVolumeTexture(256, 256, 256, 1, 0, surfaceFormat, D3DPOOL_DEFAULT, &volumeTexture, NULL);

                if (FAILED(status)) {
                    std::cout << "     - The format is not supported by CreateVolumeTexture" << std::endl;
                } else {
                    std::cout << "     + The format is supported by CreateVolumeTexture" << std::endl;
                    volumeTexture = nullptr;
                }
            }
        }

        // D3D Device capabilities check
        void listDeviceCapabilities() {
            D3DCAPS9 caps9SWVP;
            D3DCAPS9 caps9HWVP;
            D3DCAPS9 caps9;

            // these are all the possible VS versions
            std::map<DWORD, char const*> vsVersion = { {D3DVS_VERSION(1,1), "1.1"},
                                                       {D3DVS_VERSION(2,0), "2.0"},
                                                       {D3DVS_VERSION(3,0), "3.0"} };

            // these are all the possible PS versions
            std::map<DWORD, char const*> psVersion = { {D3DPS_VERSION(1,1), "1.1"},
                                                       {D3DPS_VERSION(1,2), "1.2"},
                                                       {D3DPS_VERSION(1,3), "1.3"},
                                                       {D3DPS_VERSION(1,4), "1.4"},
                                                       {D3DPS_VERSION(2,0), "2.0"},
                                                       {D3DPS_VERSION(3,0), "3.0"} };

            // get the capabilities from the D3D device in SWVP mode
            createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING, true);
            m_device->GetDeviceCaps(&caps9SWVP);

            // get the capabilities from the D3D device in HWVP mode
            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, true);
            m_device->GetDeviceCaps(&caps9HWVP);

            // get the capabilities from the D3D interface
            m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps9);

            std::cout << std::endl << "Listing device capabilities support:" << std::endl;

            if (caps9.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP)
                std::cout << "  + D3DCAPS2_CANAUTOGENMIPMAP is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS2_CANAUTOGENMIPMAP is not supported" << std::endl;

            if (caps9.Caps3 & D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION)
                std::cout << "  + D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION is not supported" << std::endl;

            if (caps9.Caps3 & D3DCAPS3_COPY_TO_VIDMEM)
                std::cout << "  + D3DCAPS3_COPY_TO_VIDMEM is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS3_COPY_TO_VIDMEM is not supported" << std::endl;

            if (caps9.Caps3 & D3DCAPS3_COPY_TO_SYSTEMMEM)
                std::cout << "  + D3DCAPS3_COPY_TO_SYSTEMMEM is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS3_COPY_TO_SYSTEMMEM is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_INDEPENDENTWRITEMASKS)
                std::cout << "  + D3DPMISCCAPS_INDEPENDENTWRITEMASKS is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_INDEPENDENTWRITEMASKS is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT)
                std::cout << "  + D3DPMISCCAPS_PERSTAGECONSTANT is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_PERSTAGECONSTANT is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_FOGANDSPECULARALPHA)
                std::cout << "  + D3DPMISCCAPS_FOGANDSPECULARALPHA is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_FOGANDSPECULARALPHA is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND)
                std::cout << "  + D3DPMISCCAPS_SEPARATEALPHABLEND is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_SEPARATEALPHABLEND is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS)
                std::cout << "  + D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING)
                std::cout << "  + D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_FOGVERTEXCLAMPED)
                std::cout << "  + D3DPMISCCAPS_FOGVERTEXCLAMPED is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_FOGVERTEXCLAMPED is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_POSTBLENDSRGBCONVERT)
                std::cout << "  + D3DPMISCCAPS_POSTBLENDSRGBCONVERT is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_POSTBLENDSRGBCONVERT is not supported" << std::endl;

            if (caps9.RasterCaps & D3DPRASTERCAPS_SCISSORTEST)
                std::cout << "  + D3DPRASTERCAPS_SCISSORTEST is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_SCISSORTEST is not supported" << std::endl;

            if (caps9.RasterCaps & D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS)
                std::cout << "  + D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS is not supported" << std::endl;

            if (caps9.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS)
                std::cout << "  + D3DPRASTERCAPS_DEPTHBIAS is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_DEPTHBIAS is not supported" << std::endl;

            if (caps9.RasterCaps & D3DPRASTERCAPS_MULTISAMPLE_TOGGLE)
                std::cout << "  + D3DPRASTERCAPS_MULTISAMPLE_TOGGLE is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_MULTISAMPLE_TOGGLE is not supported" << std::endl;

            if (caps9.SrcBlendCaps & D3DPBLENDCAPS_BLENDFACTOR)
                std::cout << "  + D3DPBLENDCAPS_BLENDFACTOR (Src) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_BLENDFACTOR (Src) is not supported" << std::endl;

            if (caps9.SrcBlendCaps & D3DPBLENDCAPS_INVSRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_INVSRCCOLOR2 (Src) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_INVSRCCOLOR2 (Src) is not supported" << std::endl;

            if (caps9.SrcBlendCaps & D3DPBLENDCAPS_SRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_SRCCOLOR2 (Src) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_SRCCOLOR2 (Src) is not supported" << std::endl;

            if (caps9.DestBlendCaps & D3DPBLENDCAPS_BLENDFACTOR)
                std::cout << "  + D3DPBLENDCAPS_BLENDFACTOR (Dest) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_BLENDFACTOR (Dest) is not supported" << std::endl;

            if (caps9.DestBlendCaps & D3DPBLENDCAPS_INVSRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_INVSRCCOLOR2 (Dest) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_INVSRCCOLOR2 (Dest) is not supported" << std::endl;

            if (caps9.DestBlendCaps & D3DPBLENDCAPS_SRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_SRCCOLOR2 (Dest) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_SRCCOLOR2 (Dest) is not supported" << std::endl;

            if (caps9.LineCaps & D3DLINECAPS_ANTIALIAS)
                std::cout << "  + D3DLINECAPS_ANTIALIAS is supported" << std::endl;
            else
                std::cout << "  - D3DLINECAPS_ANTIALIAS is not supported" << std::endl;

            if (caps9.StencilCaps & D3DSTENCILCAPS_TWOSIDED)
                std::cout << "  + D3DSTENCILCAPS_TWOSIDED is supported" << std::endl;
            else
                std::cout << "  - D3DSTENCILCAPS_TWOSIDED is not supported" << std::endl;

            if (caps9.VertexProcessingCaps & D3DVTXPCAPS_TEXGEN_SPHEREMAP)
                std::cout << "  + D3DVTXPCAPS_TEXGEN_SPHEREMAP is supported" << std::endl;
            else
                std::cout << "  - D3DVTXPCAPS_TEXGEN_SPHEREMAP is not supported" << std::endl;

            if (caps9.VertexProcessingCaps & D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER)
                std::cout << "  + D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER is supported" << std::endl;
            else
                std::cout << "  - D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER is not supported" << std::endl;

            std::cout << std::endl << "Listing device capability limits:" << std::endl;

            std::cout << format("  ~ MaxTextureWidth: ", caps9.MaxTextureWidth) << std::endl;
            std::cout << format("  ~ MaxTextureHeight: ", caps9.MaxTextureHeight) << std::endl;
            std::cout << format("  ~ MaxVolumeExtent: ", caps9.MaxVolumeExtent) << std::endl;
            std::cout << format("  ~ MaxTextureRepeat: ", caps9.MaxTextureRepeat) << std::endl;
            std::cout << format("  ~ MaxTextureAspectRatio: ", caps9.MaxTextureAspectRatio) << std::endl;
            std::cout << format("  ~ MaxAnisotropy: ", caps9.MaxAnisotropy) << std::endl;
            std::cout << format("  ~ MaxVertexW: ", caps9.MaxVertexW) << std::endl;
            std::cout << format("  ~ GuardBandLeft: ", caps9.GuardBandLeft) << std::endl;
            std::cout << format("  ~ GuardBandTop: ", caps9.GuardBandTop) << std::endl;
            std::cout << format("  ~ GuardBandRight: ", caps9.GuardBandRight) << std::endl;
            std::cout << format("  ~ GuardBandBottom: ", caps9.GuardBandBottom) << std::endl;
            std::cout << format("  ~ ExtentsAdjust: ", caps9.ExtentsAdjust) << std::endl;
            std::cout << format("  ~ MaxTextureBlendStages: ", caps9.MaxTextureBlendStages) << std::endl;
            std::cout << format("  ~ MaxSimultaneousTextures: ", caps9.MaxSimultaneousTextures) << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxActiveLights: ", caps9.MaxActiveLights, " (I), ", caps9SWVP.MaxActiveLights,
                                " (SWVP), ", caps9HWVP.MaxActiveLights, " (HWVP)") << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxUserClipPlanes: ", caps9.MaxUserClipPlanes, " (I), ", caps9SWVP.MaxUserClipPlanes,
                                " (SWVP), ", caps9HWVP.MaxUserClipPlanes, " (HWVP)") << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxVertexBlendMatrices: ", caps9.MaxVertexBlendMatrices, " (I), ", caps9SWVP.MaxVertexBlendMatrices,
                                " (SWVP), ", caps9HWVP.MaxVertexBlendMatrices, " (HWVP)") << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxVertexBlendMatrixIndex: ", caps9.MaxVertexBlendMatrixIndex, " (I), ", caps9SWVP.MaxVertexBlendMatrixIndex,
                                " (SWVP), ", caps9HWVP.MaxVertexBlendMatrixIndex, " (HWVP)") << std::endl;                         
            std::cout << format("  ~ MaxPointSize: ", caps9.MaxPointSize) << std::endl;
            std::cout << format("  ~ MaxPrimitiveCount: ", caps9.MaxPrimitiveCount) << std::endl;
            std::cout << format("  ~ MaxVertexIndex: ", caps9.MaxVertexIndex) << std::endl;
            std::cout << format("  ~ MaxStreams: ", caps9.MaxStreams) << std::endl;
            std::cout << format("  ~ MaxStreamStride: ", caps9.MaxStreamStride) << std::endl;
            std::cout << format("  ~ VertexShaderVersion: ", vsVersion[caps9.VertexShaderVersion]) << std::endl;
            std::cout << format("  ~ MaxVertexShaderConst: ", caps9.MaxVertexShaderConst) << std::endl;
            std::cout << format("  ~ PixelShaderVersion: ", psVersion[caps9.PixelShaderVersion]) << std::endl;
            // typically FLT_MAX
            std::cout << format("  ~ PixelShader1xMaxValue: ", caps9.PixelShader1xMaxValue) << std::endl;
            std::cout << format("  ~ MaxNpatchTessellationLevel: ", caps9.MaxNpatchTessellationLevel) << std::endl;
            std::cout << format("  ~ NumSimultaneousRTs: ", caps9.NumSimultaneousRTs) << std::endl;
            std::cout << format("  ~ MaxVShaderInstructionsExecuted: ", caps9.MaxVShaderInstructionsExecuted) << std::endl;
            std::cout << format("  ~ MaxPShaderInstructionsExecuted: ", caps9.MaxPShaderInstructionsExecuted) << std::endl;
            std::cout << format("  ~ MaxVertexShader30InstructionSlots: ", caps9.MaxVertexShader30InstructionSlots) << std::endl;
            std::cout << format("  ~ MaxPixelShader30InstructionSlots: ", caps9.MaxPixelShader30InstructionSlots) << std::endl;
        }

        void listVCacheQueryResult() {
            resetOrRecreateDevice();

            Com<IDirect3DQuery9> query;
            D3DDEVINFO_VCACHE vCache;

            HRESULT status = m_device->CreateQuery(D3DQUERYTYPE_VCACHE, &query);
            if (SUCCEEDED(status)) {
                status = query->GetData(&vCache, sizeof(D3DDEVINFO_VCACHE), D3DQUERYTYPE_VCACHE);
            }

            std::cout << std::endl << "Listing VCache query result:" << std::endl;

            switch (status) {
                // NVIDIA will return D3D_OK and some values in vCache
                case D3D_OK:
                    std::cout << "  ~ Response: D3D_OK" << std::endl;
                    break;
                // AMD/ATI and Intel will return D3DERR_NOTAVAILABLE
                case D3DERR_NOTAVAILABLE:
                    std::cout << "  ~ Response: D3DERR_NOTAVAILABLE" << std::endl;
                    break;
                // shouldn't be returned by any vendor
                case D3DERR_INVALIDCALL:
                    std::cout << "  ~ Response: D3DERR_INVALIDCALL" << std::endl;
                    break;
                default:
                    std::cout << "  ~ Response: UNKNOWN" << std::endl;
                    break;
            }

            if (SUCCEEDED(status)) {
                char pattern[] = "\0\0\0\0";
                if (vCache.Pattern == 0) {
                    pattern[0] = '0';
                } else {
                    for (UINT i = 0; i < 4; i++) {
                        pattern[i] = char(vCache.Pattern >> i * 8);
                    }
                }

                std::cout << format("  ~ Pattern: ", pattern) << std::endl;
                std::cout << format("  ~ OptMethod: ", vCache.OptMethod) << std::endl;
                std::cout << format("  ~ CacheSize: ", vCache.CacheSize) << std::endl;
                std::cout << format("  ~ MagicNumber: ", vCache.MagicNumber) << std::endl;
            }
        }

        void startTests() {
            m_totalTests  = 0;
            m_passedTests = 0;

            std::cout << std::endl << "Running D3D9 tests:" << std::endl;
        }

        // BeginScene & Reset test
        void testBeginSceneReset() {
            resetOrRecreateDevice();

            m_totalTests++;

            if (SUCCEEDED(m_device->BeginScene())) {
                HRESULT status = m_device->Reset(&m_pp);
                if (FAILED(status)) {
                    std::cout << "  - The BeginScene & Reset test has failed on Reset()" << std::endl;
                }
                else {
                    // Reset() should have cleared the state so this should work properly
                    if (SUCCEEDED(m_device->BeginScene())) {
                        m_passedTests++;
                        std::cout << "  + The BeginScene & Reset test has passed" << std::endl;
                    } else {
                        std::cout << "  - The BeginScene & Reset test has failed" << std::endl;
                    }
                }
            } else {
                throw Error("Failed to begin D3D9 scene");
            }

            // this call is expected fail in certain scenarios, but it makes no difference
            m_device->EndScene();
        }

        // D3DCREATE_PUREDEVICE only with D3DCREATE_HARDWARE_VERTEXPROCESSING test
        void testPureDeviceOnlyWithHWVP() {
            // native drivers will fail to create a device with D3DCREATE_PUREDEVICE unless
            // it is combined together with D3DCREATE_HARDWARE_VERTEXPROCESSING
            HRESULT statusHWVP = createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, false);
            HRESULT statusSWVP = createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, false);
            HRESULT statusMVP = createDeviceWithFlags(&m_pp, D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, false);

            m_totalTests++;

            if (SUCCEEDED(statusHWVP) && FAILED(statusSWVP) && FAILED(statusMVP)) {
                m_passedTests++;
                std::cout << "  + The PUREDEVICE mode only with HWVP test has passed" << std::endl;
            } else {
                std::cout << "  - The PUREDEVICE mode only with HWVP test has failed" << std::endl;
            }
        }

        // D3DPOOL_DEFAULT allocation & Reset + D3DERR_DEVICENOTRESET state test (2 in 1)
        void testDefaultPoolAllocationReset() {
            resetOrRecreateDevice();

            // create a temporary DS surface
            Com<IDirect3DSurface9> tempDS;
            m_device->CreateDepthStencilSurface(RGBTriangle::WINDOW_WIDTH, RGBTriangle::WINDOW_HEIGHT,
                                                D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, FALSE, &tempDS, NULL);

            m_totalTests++;
            // according to D3D9 docs, I quote: "Reset will fail unless the application releases all resources
            // that are allocated in D3DPOOL_DEFAULT, including those created by the IDirect3DDevice9::CreateRenderTarget
            // and IDirect3DDevice9::CreateDepthStencilSurface methods.", so this call should fail
            HRESULT status = m_device->Reset(&m_pp);
            if (FAILED(status)) {
                m_passedTests++;
                std::cout << "  + The D3DPOOL_DEFAULT allocation & Reset test has passed" << std::endl;

                m_totalTests++;
                // check to see if the device state is D3DERR_DEVICENOTRESET
                status = m_device->TestCooperativeLevel();
                if (status == D3DERR_DEVICENOTRESET) {
                    m_passedTests++;
                    std::cout << "  + The D3DERR_DEVICENOTRESET state test has passed" << std::endl;
                } else {
                    std::cout << "  - The D3DERR_DEVICENOTRESET state test has failed" << std::endl;
                }

            } else {
                std::cout << "  - The D3DPOOL_DEFAULT allocation & Reset test has failed" << std::endl;
                std::cout << "  ~ The D3DERR_DEVICENOTRESET state test did not run" << std::endl;
            }
        }

        // CreateStateBlock & Reset test
        void testCreateStateBlockAndReset() {
            resetOrRecreateDevice();

            // create a temporary state block
            Com<IDirect3DStateBlock9> stateBlock;
            m_device->CreateStateBlock(D3DSBT_ALL, &stateBlock);

            m_totalTests++;
            // D3D9 state blocks don't survive device Reset() calls and should be counted as losable resources
            HRESULT status = m_device->Reset(&m_pp);
            if (FAILED(status)) {
                m_passedTests++;
                std::cout << "  + The CreateStateBlock & Reset test has passed" << std::endl;
            } else {
                std::cout << "  - The CreateStateBlock & Reset test has failed" << std::endl;
            }
        }

        // Cursor HotSpot coordinates test
        void testCursorHotSpotCoordinates() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface9> surface;
            D3DLOCKED_RECT rect;

            std::vector<uint8_t> bitmap(256 * 256 * 4, 1);

            HRESULT status = m_device->CreateOffscreenPlainSurface(256, 256, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surface, NULL);

            if (SUCCEEDED(status)) {
                m_totalTests++;

                surface->LockRect(&rect, NULL, 0);
                uint8_t* data  = reinterpret_cast<uint8_t*>(rect.pBits);
                memcpy(&data[0], &bitmap[0], 256 * 256 * 4);
                surface->UnlockRect();

                // HotSpot coordinates outside of the cursor bitmap will cause this call to fail
                HRESULT statusCursor = m_device->SetCursorProperties(256, 256, surface.ptr());

                if (FAILED(statusCursor)) {
                    m_passedTests++;
                    std::cout << "  + The Cursor HotSpot coordinates test has passed" << std::endl;
                } else {
                    std::cout << "  - The Cursor HotSpot coordinates test has failed" << std::endl;
                }
            } else {
                std::cout << "  ~ The Cursor HotSpot coordinates test has not run" << std::endl;
            }
        }

        // Various CheckDeviceMultiSampleType validation tests
        void testCheckDeviceMultiSampleTypeValidation() {
            resetOrRecreateDevice();

            m_totalTests++;

            // The call will fail with anything above D3DMULTISAMPLE_16_SAMPLES
            HRESULT statusSample = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, FALSE,
                                                                    (D3DMULTISAMPLE_TYPE) ((UINT) D3DMULTISAMPLE_16_SAMPLES * 2), 0);

            // The call will fail with D3DFMT_UNKNOWN
            HRESULT statusUnknown = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_UNKNOWN,
                                                                      FALSE, D3DMULTISAMPLE_NONE, 0);

            if (FAILED(statusSample) && FAILED(statusUnknown)) {
                m_passedTests++;
                std::cout << "  + The CheckDeviceMultiSampleType validation test has passed" << std::endl;
            } else {
                std::cout << "  - The CheckDeviceMultiSampleType validation test has failed" << std::endl;
            }
        }

        // Various CheckDeviceMultiSampleType formats test
        void testCheckDeviceMultiSampleTypeFormats() {
            resetOrRecreateDevice();

            std::map<D3DFORMAT, char const*> sfFormats = { {D3DFMT_D32_LOCKABLE, "D3DFMT_D32_LOCKABLE"},
                                                           {D3DFMT_D32F_LOCKABLE, "D3DFMT_D32F_LOCKABLE"},
                                                           {D3DFMT_D16_LOCKABLE, "D3DFMT_D16_LOCKABLE"},
                                                           {(D3DFORMAT) MAKEFOURCC('I', 'N', 'T', 'Z'), "D3DFMT_INTZ"},
                                                           {D3DFMT_DXT1, "D3DFMT_DXT1"},
                                                           {D3DFMT_DXT2, "D3DFMT_DXT2"},
                                                           {D3DFMT_DXT3, "D3DFMT_DXT3"},
                                                           {D3DFMT_DXT4, "D3DFMT_DXT4"},
                                                           {D3DFMT_DXT5, "D3DFMT_DXT5"} };

            std::map<D3DFORMAT, char const*>::iterator sfFormatIter;

            std::cout << std::endl << "Running CheckDeviceMultiSampleType formats tests:" << std::endl;

            for (sfFormatIter = sfFormats.begin(); sfFormatIter != sfFormats.end(); sfFormatIter++) {
                D3DFORMAT surfaceFormat = sfFormatIter->first;

                m_totalTests++;

                // None of the above textures are supported with anything beside D3DMULTISAMPLE_NONE
                HRESULT status = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, surfaceFormat,
                                                                   FALSE, D3DMULTISAMPLE_2_SAMPLES, 0);

                if (SUCCEEDED(status)) {
                    std::cout << format("  - The ", sfFormatIter->second , " format test has failed") << std::endl;
                } else {
                    m_passedTests++;
                    std::cout << format("  + The ", sfFormatIter->second ," format test has passed") << std::endl;
                }
            }
        }

        // CreateVolumeTexture formats test
        void testCreateVolumeTextureFormats() {
            resetOrRecreateDevice();

            std::map<D3DFORMAT, char const*> texFormats = { {D3DFMT_A8R8G8B8, "D3DFMT_A8R8G8B8"},
                                                            {D3DFMT_DXT1, "D3DFMT_DXT1"},
                                                            {D3DFMT_DXT2, "D3DFMT_DXT2"},
                                                            {D3DFMT_DXT3, "D3DFMT_DXT3"},
                                                            {D3DFMT_DXT4, "D3DFMT_DXT4"},
                                                            {D3DFMT_DXT5, "D3DFMT_DXT5"},
                                                            {(D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '1'), "ATI1"},
                                                            {(D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '2'), "ATI2"},
                                                            {(D3DFORMAT) MAKEFOURCC('Y', 'U', 'Y', '2'), "YUY2"} };

            std::map<D3DFORMAT, char const*>::iterator texFormatIter;

            Com<IDirect3DVolumeTexture9> volumeTexture;

            std::cout << std::endl << "Running CreateVolumeTexture formats tests:" << std::endl;

            for (texFormatIter = texFormats.begin(); texFormatIter != texFormats.end(); texFormatIter++) {
                D3DFORMAT texFormat = texFormatIter->first;

                HRESULT createStatus = m_device->CreateVolumeTexture(256, 256, 256, 1, 0, texFormat, D3DPOOL_DEFAULT, &volumeTexture, NULL);

                if (SUCCEEDED(createStatus)) {
                    D3DLOCKED_BOX lockBox;

                    m_totalTests++;

                    HRESULT lockStatus = volumeTexture->LockBox(0, &lockBox, NULL, 0);

                    if (FAILED(lockStatus)) {
                        m_passedTests++;
                        std::cout << format("  + The ", texFormatIter->second , " format test has passed") << std::endl;
                    } else {
                        std::cout << format("  - The ", texFormatIter->second , " format test has failed") << std::endl;
                    }

                    volumeTexture = nullptr;
                } else {
                    std::cout << format("  ~ The ", texFormatIter->second ," format is not supported") << std::endl;
                }
            }
        }

        // Various surface format tests
        void testSurfaceFormats() {
            resetOrRecreateDevice();

            std::map<D3DFORMAT, char const*> sfFormats = { {D3DFMT_R8G8B8, "D3DFMT_R8G8B8"},
                                                           {D3DFMT_R3G3B2, "D3DFMT_R3G3B2"},
                                                           {D3DFMT_A8R3G3B2, "D3DFMT_A8R3G3B2"},
                                                           {D3DFMT_A8P8, "D3DFMT_A8P8"},
                                                           {D3DFMT_P8, "D3DFMT_P8"},
                                                           {D3DFMT_L6V5U5, "D3DFMT_L6V5U5"},
                                                           {D3DFMT_X8L8V8U8, "D3DFMT_X8L8V8U8"},
                                                           {D3DFMT_A2W10V10U10, "D3DFMT_A2W10V10U10"} };

            std::map<D3DFORMAT, char const*>::iterator sfFormatIter;

            Com<IDirect3DSurface9> surface;
            Com<IDirect3DTexture9> texture;
            Com<IDirect3DCubeTexture9> cubeTexture;
            Com<IDirect3DVolumeTexture9> volumeTexture;

            std::cout << std::endl << "Running surface format tests:" << std::endl;

            for (sfFormatIter = sfFormats.begin(); sfFormatIter != sfFormats.end(); sfFormatIter++) {
                D3DFORMAT surfaceFormat = sfFormatIter->first;

                std::cout << format("  ~ ", sfFormatIter->second, ":") << std::endl;

                // Calls to CreateOffscreenPlainSurface using D3DPOOL_SCRATCH should never fail, even with unsupported formats
                HRESULT status = m_device->CreateOffscreenPlainSurface(256, 256, surfaceFormat, D3DPOOL_SCRATCH, &surface, NULL);

                if (FAILED(status)) {
                    // Apparently, CreateOffscreenPlainSurface fails with D3DFMT_A2W10V10U10 on Windows 98 SE
                    if (surfaceFormat == D3DFMT_A2W10V10U10) {
                        std::cout << "     ~ Format is not supported by CreateOffscreenPlainSurface" << std::endl;
                    } else {
                        m_totalTests++;
                        std::cout << "     - The CreateOffscreenPlainSurface test has failed" << std::endl;
                    }
                } else {
                    m_totalTests++;
                    m_passedTests++;
                    std::cout << "     + The CreateOffscreenPlainSurface test has passed" << std::endl;
                    surface = nullptr;
                }

                status = m_device->CreateTexture(256, 256, 1, 0, surfaceFormat, D3DPOOL_DEFAULT, &texture, NULL);

                if (FAILED(status)) {
                    std::cout << "     ~ The format is not supported by CreateTexture" << std::endl;
                } else {
                    m_totalTests++;
                    m_passedTests++;
                    std::cout << "     + The CreateTexture test has passed" << std::endl;
                    texture = nullptr;
                }

                status = m_device->CreateCubeTexture(256, 1, 0, surfaceFormat, D3DPOOL_DEFAULT, &cubeTexture, NULL);

                if (FAILED(status)) {
                    std::cout << "     ~ The format is not supported by CreateCubeTexture" << std::endl;
                } else {
                    m_totalTests++;
                    m_passedTests++;
                    std::cout << "     + The CreateCubeTexture test has passed" << std::endl;
                    cubeTexture = nullptr;
                }


                status = m_device->CreateVolumeTexture(256, 256, 256, 1, 0, surfaceFormat, D3DPOOL_DEFAULT, &volumeTexture, NULL);

                if (FAILED(status)) {
                    std::cout << "     ~ The format is not supported by CreateVolumeTexture" << std::endl;
                } else {
                    m_totalTests++;
                    m_passedTests++;
                    std::cout << "     + The CreateVolumeTexture test has passed" << std::endl;
                    volumeTexture = nullptr;
                }
            }
        }

        void printTestResults() {
            std::cout << std::endl << format("Passed ", m_passedTests, "/", m_totalTests, " tests") << std::endl;
        }

        void prepare() {
            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, true);

            // don't need any of these for 2D rendering
            HRESULT status = m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D9 render state for D3DRS_ZENABLE");
            status = m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            if (FAILED(status))
                throw Error("Failed to set D3D9 render state for D3DRS_CULLMODE");
            status = m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D9 render state for D3DRS_LIGHTING");

            // Vertex Buffer
            void* vertices = nullptr;

            // tailored for 1024x768 and the appearance of being centered
            std::array<RGBVERTEX, 3> rgbVertices = {{
                { 60.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0),},
                {350.0f,  45.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 255, 0),},
                {640.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255),},
            }};
            const size_t rgbVerticesSize = rgbVertices.size() * sizeof(RGBVERTEX);

            status = m_device->CreateVertexBuffer(rgbVerticesSize, 0, RGBT_FVF_CODES,
                                                  D3DPOOL_DEFAULT, &m_vb, NULL);
            if (FAILED(status))
                throw Error("Failed to create D3D9 vertex buffer");

            status = m_vb->Lock(0, rgbVerticesSize, (void**)&vertices, 0);
            if (FAILED(status))
                throw Error("Failed to lock D3D9 vertex buffer");
            memcpy(vertices, rgbVertices.data(), rgbVerticesSize);
            status = m_vb->Unlock();
            if (FAILED(status))
                throw Error("Failed to unlock D3D9 vertex buffer");
        }

        void render() {
            HRESULT status = m_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            if (FAILED(status))
                throw Error("Failed to clear D3D9 viewport");
            if (SUCCEEDED(m_device->BeginScene())) {
                status = m_device->SetStreamSource(0, m_vb.ptr(), 0, sizeof(RGBVERTEX));
                if (FAILED(status))
                    throw Error("Failed to set D3D9 stream source");
                status = m_device->SetFVF(RGBT_FVF_CODES);
                if (FAILED(status))
                    throw Error("Failed to set D3D9 vertex shader");
                status = m_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                if (FAILED(status))
                    throw Error("Failed to draw D3D9 triangle list");
                if (SUCCEEDED(m_device->EndScene())) {
                    status = m_device->Present(NULL, NULL, NULL, NULL);
                    if (FAILED(status))
                        throw Error("Failed to present");
                } else {
                    throw Error("Failed to end D3D9 scene");
                }
            } else {
                throw Error("Failed to begin D3D9 scene");
            }
        }

    private:

        HRESULT createDeviceWithFlags(D3DPRESENT_PARAMETERS* presentParams,
                                      DWORD behaviorFlags,
                                      bool throwErrorOnFail) {
            if (m_d3d == nullptr)
                throw Error("The D3D9 interface hasn't been initialized");

            if (m_device != nullptr)
                m_device = nullptr;

            HRESULT status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                                 behaviorFlags, presentParams, &m_device);
            if (throwErrorOnFail && FAILED(status))
                throw Error("Failed to create D3D9 device");

            return status;
        }

        HRESULT resetOrRecreateDevice() {
            if (m_d3d == nullptr)
                throw Error("The D3D9 interface hasn't been initialized");

            HRESULT status = D3D_OK;

            // return early if the call to Reset() works
            if (m_device != nullptr) {
                if(SUCCEEDED(m_device->Reset(&m_pp)))
                    return status;
                // prepare to clear the device otherwise
                m_device = nullptr;
            }

            status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                         D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                         &m_pp, &m_device);
            if (FAILED(status))
                throw Error("Failed to create D3D9 device");

            return status;
        }

        HWND                          m_hWnd;

        Com<IDirect3D9>               m_d3d;
        Com<IDirect3DDevice9>         m_device;
        Com<IDirect3DVertexBuffer9>   m_vb;

        D3DPRESENT_PARAMETERS         m_pp;

        UINT                          m_totalTests;
        UINT                          m_passedTests;
};

LRESULT WINAPI WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch(message) {
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

int main(int, char**) {
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L,
                     GetModuleHandle(NULL), NULL, LoadCursor(nullptr, IDC_ARROW), NULL, NULL,
                     "D3D9_Triangle", NULL};
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow("D3D9_Triangle", "D3D9 Triangle - Blisto Modern Testing Edition",
                              WS_OVERLAPPEDWINDOW, 50, 50,
                              RGBTriangle::WINDOW_WIDTH, RGBTriangle::WINDOW_HEIGHT,
                              GetDesktopWindow(), NULL, wc.hInstance, NULL);

    MSG msg;

    try {
        RGBTriangle rgbTriangle(hWnd);

        // list various D3D Device stats
        rgbTriangle.listAdapterDisplayModes();
        rgbTriangle.listBackBufferFormats(FALSE);
        rgbTriangle.listBackBufferFormats(TRUE);
        rgbTriangle.listObscureFOURCCSurfaceFormats();
        rgbTriangle.listDeviceCapabilities();
        rgbTriangle.listVCacheQueryResult();

        // run D3D Device tests
        rgbTriangle.startTests();
        rgbTriangle.testBeginSceneReset();
        rgbTriangle.testPureDeviceOnlyWithHWVP();
        rgbTriangle.testDefaultPoolAllocationReset();
        rgbTriangle.testCreateStateBlockAndReset();
        rgbTriangle.testCursorHotSpotCoordinates();
        rgbTriangle.testCheckDeviceMultiSampleTypeValidation();
        rgbTriangle.testCheckDeviceMultiSampleTypeFormats();
        rgbTriangle.testCreateVolumeTextureFormats();
        rgbTriangle.testSurfaceFormats();
        rgbTriangle.printTestResults();

        // D3D9 triangle
        rgbTriangle.prepare();

        ShowWindow(hWnd, SW_SHOWDEFAULT);
        UpdateWindow(hWnd);

        while (true) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    UnregisterClass("D3D9_Triangle", wc.hInstance);
                    return msg.wParam;
                }
            } else {
                rgbTriangle.render();
            }
        }

    } catch (const Error& e) {
        std::cerr << e.message() << std::endl;
        UnregisterClass("D3D9_Triangle", wc.hInstance);
        return msg.wParam;
    }
}
