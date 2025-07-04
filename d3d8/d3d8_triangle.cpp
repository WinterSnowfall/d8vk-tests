#include <map>
#include <array>
#include <iostream>

#include <d3d8.h>
#include <d3d9caps.h>

#include "../common/bit.h"
#include "../common/com.h"
#include "../common/error.h"
#include "../common/str.h"

struct RGBVERTEX {
    FLOAT x, y, z, rhw;
    DWORD colour;
};

#define RGBT_FVF_CODES (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
#define PTRCLEARED(ptr) (ptr == nullptr)

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

// these don't seem to be included in any of the
// headers that should allegedly include them...
#define D3DDEVINFOID_VCACHE                     4

typedef struct _D3DDEVINFO_VCACHE {
  DWORD Pattern;
  DWORD OptMethod;
  DWORD CacheSize;
  DWORD MagicNumber;
} D3DDEVINFO_VCACHE, *LPD3DDEVINFO_VCACHE;

class RGBTriangle {

    public:

        static const UINT WINDOW_WIDTH  = 700;
        static const UINT WINDOW_HEIGHT = 700;

        RGBTriangle(HWND hWnd) : m_hWnd(hWnd) {
            decltype(Direct3DCreate8)* Direct3DCreate8 = nullptr;
            HMODULE hm = LoadLibraryA("d3d8.dll");
            Direct3DCreate8 = (decltype(Direct3DCreate8))GetProcAddress(hm, "Direct3DCreate8");

            // D3D Interface
            m_d3d = Direct3DCreate8(D3D_SDK_VERSION);
            if (m_d3d.ptr() == nullptr)
                throw Error("Failed to create D3D8 interface");

            D3DADAPTER_IDENTIFIER8 adapterId;
            HRESULT status = m_d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterId);
            if (FAILED(status))
                throw Error("Failed to get D3D8 adapter identifier");

            m_vendorID = adapterId.VendorId;

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
            if (m_vendorID == uint32_t(0x10de)) {
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
                throw Error("Failed to get D3D8 adapter display mode");

            ZeroMemory(&m_pp, sizeof(m_pp));

            m_pp.Windowed = TRUE;
            m_pp.hDeviceWindow = m_hWnd;
            // set to D3DSWAPEFFECT_COPY or D3DSWAPEFFECT_FLIP for no VSync
            m_pp.SwapEffect = D3DSWAPEFFECT_COPY_VSYNC;
            // according to D3D8 spec "0 is treated as 1" here
            m_pp.BackBufferCount = 0;
            m_pp.BackBufferWidth = WINDOW_WIDTH;
            m_pp.BackBufferHeight = WINDOW_HEIGHT;
            m_pp.BackBufferFormat = dm.Format;

            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);
        }

        // D3D Adapter Display Mode enumeration
        void listAdapterDisplayModes() {
            HRESULT          status;
            D3DDISPLAYMODE   amDM;
            UINT             adapterModeCount = 0;

            ZeroMemory(&amDM, sizeof(amDM));

            adapterModeCount = m_d3d->GetAdapterModeCount(D3DADAPTER_DEFAULT);
            if (adapterModeCount == 0)
                throw Error("Failed to query for D3D8 adapter display modes");

            // these are all the possible adapter display formats, at least in theory
            std::map<D3DFORMAT, char const*> amDMFormats = { {D3DFMT_A8R8G8B8, "D3DFMT_A8R8G8B8"},
                                                             {D3DFMT_X8R8G8B8, "D3DFMT_X8R8G8B8"},
                                                             {D3DFMT_A1R5G5B5, "D3DFMT_A1R5G5B5"},
                                                             {D3DFMT_X1R5G5B5, "D3DFMT_X1R5G5B5"},
                                                             {D3DFMT_R5G6B5, "D3DFMT_R5G6B5"} };

            std::cout << std::endl << "Enumerating supported adapter display modes:" << std::endl;

            for (UINT i = 0; i < adapterModeCount; i++) {
                status =  m_d3d->EnumAdapterModes(D3DADAPTER_DEFAULT, i, &amDM);
                if (FAILED(status)) {
                    std::cout << format("    Failed to get adapter display mode ", i) << std::endl;
                } else {
                    std::cout << format("    ", amDMFormats[amDM.Format], " ", amDM.Width,
                                        " x ", amDM.Height, " @ ", amDM.RefreshRate, " Hz") << std::endl;
                }
            }

            std::cout << format("Listed a total of ", adapterModeCount, " adapter display modes") << std::endl;
        }

        // D3D Device back buffer formats check
        void listBackBufferFormats(BOOL windowed) {
            HRESULT status;
            D3DPRESENT_PARAMETERS bbPP;

            memcpy(&bbPP, &m_pp, sizeof(m_pp));

            // these are all the possible adapter format / backbuffer format pairs, at least in theory
            std::map<std::pair<D3DFORMAT, D3DFORMAT>, char const*> bbFormats = {
                {{D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8}, "D3DFMT_X8R8G8B8"},
                {{D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8}, "D3DFMT_A8R8G8B8"},
                {{D3DFMT_X1R5G5B5, D3DFMT_X1R5G5B5}, "D3DFMT_X1R5G5B5"},
                {{D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5}, "D3DFMT_A1R5G5B5"},
                {{D3DFMT_R5G6B5, D3DFMT_R5G6B5}, "D3DFMT_R5G6B5"},
                {{D3DFMT_R5G6B5, D3DFMT_UNKNOWN}, "D3DFMT_UNKNOWN"}
            };

            std::map<std::pair<D3DFORMAT, D3DFORMAT>, char const*>::iterator bbFormatIter;

            std::cout << std::endl;
            if (windowed)
                std::cout << "Device back buffer format support (windowed):" << std::endl;
            else
                std::cout << "Device back buffer format support (full screen):" << std::endl;

            for (bbFormatIter = bbFormats.begin(); bbFormatIter != bbFormats.end(); ++bbFormatIter) {
                status = m_d3d->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                bbFormatIter->first.first, bbFormatIter->first.second,
                                                windowed);
                if (FAILED(status)) {
                    std::cout << format("  - The ", bbFormatIter->second, " format is not supported") << std::endl;
                } else {
                    bbPP.BackBufferFormat = bbFormatIter->first.second;

                    status = createDeviceWithFlags(&bbPP, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);
                    if (FAILED(status)) {
                        std::cout << format("  ! The ", bbFormatIter->second, " format is supported, device creation FAILED") << std::endl;
                    } else {
                        std::cout << format("  + The ", bbFormatIter->second, " format is supported, device creation SUCCEEDED") << std::endl;
                    }
                }
            }
        }

        // D3D Device surface/texture formats check
        void listSurfaceFormats(DWORD Usage) {
            resetOrRecreateDevice();

            // D3DFMT_UNKNOWN will fail everywhere, so start with the next format
            std::map<D3DFORMAT, char const*> formats = { {D3DFMT_R8G8B8,        "D3DFMT_R8G8B8"},
                                                         {D3DFMT_A8R8G8B8,      "D3DFMT_A8R8G8B8"},
                                                         {D3DFMT_X8R8G8B8,      "D3DFMT_X8R8G8B8"},
                                                         {D3DFMT_R5G6B5,        "D3DFMT_R5G6B5"},
                                                         {D3DFMT_X1R5G5B5,      "D3DFMT_X1R5G5B5"},
                                                         {D3DFMT_A1R5G5B5,      "D3DFMT_A1R5G5B5"},
                                                         {D3DFMT_A4R4G4B4,      "D3DFMT_A4R4G4B4"},
                                                         {D3DFMT_R3G3B2,        "D3DFMT_R3G3B2"},
                                                         {D3DFMT_A8,            "D3DFMT_A8"},
                                                         {D3DFMT_A8R3G3B2,      "D3DFMT_A8R3G3B2"},
                                                         {D3DFMT_X4R4G4B4,      "D3DFMT_X4R4G4B4"},
                                                         {D3DFMT_A2B10G10R10,   "D3DFMT_A2B10G10R10"},
                                                         // D3DFMT_A8B8G8R8 and D3DFMT_X8B8G8R8 are not supported by D3D8
                                                         {(D3DFORMAT) 32,       "D3DFMT_A8B8G8R8"},
                                                         {(D3DFORMAT) 33,       "D3DFMT_X8B8G8R8"},
                                                         {D3DFMT_G16R16,        "D3DFMT_G16R16"},
                                                         // D3DFMT_A2R10G10B10 and D3DFMT_A16B16G16R16 are not supported by D3D8
                                                         {(D3DFORMAT) 35,       "D3DFMT_A2R10G10B10"},
                                                         {(D3DFORMAT) 36,       "D3DFMT_A16B16G16R16"},
                                                         {D3DFMT_A8P8,          "D3DFMT_A8P8"},
                                                         {D3DFMT_P8,            "D3DFMT_P8"},
                                                         {D3DFMT_L8,            "D3DFMT_L8"},
                                                         {D3DFMT_A8L8,          "D3DFMT_A8L8"},
                                                         {D3DFMT_A4L4,          "D3DFMT_A4L4"},
                                                         {D3DFMT_V8U8,          "D3DFMT_V8U8"},
                                                         {D3DFMT_L6V5U5,        "D3DFMT_L6V5U5"},
                                                         {D3DFMT_X8L8V8U8,      "D3DFMT_X8L8V8U8"},
                                                         {D3DFMT_Q8W8V8U8,      "D3DFMT_Q8W8V8U8"},
                                                         {D3DFMT_V16U16,        "D3DFMT_V16U16"},
                                                         {D3DFMT_W11V11U10,     "D3DFMT_W11V11U10"},
                                                         {D3DFMT_A2W10V10U10,   "D3DFMT_A2W10V10U10"},
                                                         {D3DFMT_UYVY,          "D3DFMT_UYVY"},
                                                         {D3DFMT_YUY2,          "D3DFMT_YUY2"},
                                                         {D3DFMT_DXT1,          "D3DFMT_DXT1"},
                                                         {D3DFMT_DXT2,          "D3DFMT_DXT2"},
                                                         {D3DFMT_DXT3,          "D3DFMT_DXT3"},
                                                         {D3DFMT_DXT4,          "D3DFMT_DXT4"},
                                                         {D3DFMT_DXT5,          "D3DFMT_DXT5"},
                                                         // D3DFMT_MULTI2_ARGB8, D3DFMT_G8R8_G8B8 and D3DFMT_R8G8_B8G8
                                                         // are not supported by d3d8
                                                         {(D3DFORMAT) MAKEFOURCC('M', 'E', 'T', '1'), "D3DFMT_MULTI2_ARGB8"},
                                                         {(D3DFORMAT) MAKEFOURCC('G', 'R', 'G', 'B'), "D3DFMT_G8R8_G8B8"},
                                                         {(D3DFORMAT) MAKEFOURCC('R', 'G', 'B', 'G'), "D3DFMT_R8G8_B8G8"},
                                                         {D3DFMT_D16_LOCKABLE,  "D3DFMT_D16_LOCKABLE"},
                                                         {D3DFMT_D32,           "D3DFMT_D32"},
                                                         {D3DFMT_D15S1,         "D3DFMT_D15S1"},
                                                         {D3DFMT_D24S8,         "D3DFMT_D24S8"},
                                                         {D3DFMT_D24X8,         "D3DFMT_D24X8"},
                                                         {D3DFMT_D24X4S4,       "D3DFMT_D24X4S4"},
                                                         {D3DFMT_D16,           "D3DFMT_D16"},
                                                         // None of the below numbered formats are supported by D3D8
                                                         {(D3DFORMAT) 81,       "D3DFMT_L16"},
                                                         {(D3DFORMAT) 82,       "D3DFMT_D32F_LOCKABLE"},
                                                         {(D3DFORMAT) 83,       "D3DFMT_D24FS8"},
                                                         {(D3DFORMAT) 84,       "D3DFMT_D32_LOCKABLE"},
                                                         {(D3DFORMAT) 85,       "D3DFMT_S8_LOCKABLE"},
                                                         {(D3DFORMAT) 110,      "D3DFMT_Q16W16V16U16"},
                                                         {(D3DFORMAT) 111,      "D3DFMT_R16F"},
                                                         {(D3DFORMAT) 112,      "D3DFMT_G16R16F"},
                                                         {(D3DFORMAT) 113,      "D3DFMT_A16B16G16R16F"},
                                                         {(D3DFORMAT) 114,      "D3DFMT_R32F"},
                                                         {(D3DFORMAT) 115,      "D3DFMT_G32R32F"},
                                                         {(D3DFORMAT) 116,      "D3DFMT_A32B32G32R32F"},
                                                         {(D3DFORMAT) 117,      "D3DFMT_CxV8U8"},
                                                         {(D3DFORMAT) 118,      "D3DFMT_A1"},
                                                         {(D3DFORMAT) 119,      "D3DFMT_A2B10G10R10_XR_BIAS"},
                                                         // Consacrated (enum) formats end here
                                                         {(D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '1'), "D3DFMT_ATI1"},
                                                         {(D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '2'), "D3DFMT_ATI2"},
                                                         {(D3DFORMAT) MAKEFOURCC('Y', 'U', 'Y', '2'), "D3DFMT_YUY2"},
                                                         {(D3DFORMAT) MAKEFOURCC('D', 'F', '1', '6'), "D3DFMT_DF16"},
                                                         {(D3DFORMAT) MAKEFOURCC('D', 'F', '2', '4'), "D3DFMT_DF24"},
                                                         {(D3DFORMAT) MAKEFOURCC('I', 'N', 'T', 'Z'), "D3DFMT_INTZ"},
                                                         {(D3DFORMAT) MAKEFOURCC('N', 'U', 'L', 'L'), "D3DFMT_NULL"},
                                                         // Nobody seems to support these, but they are queried
                                                         {(D3DFORMAT) MAKEFOURCC('N', 'V', 'H', 'S'), "D3DFMT_NVHS"},
                                                         {(D3DFORMAT) MAKEFOURCC('N', 'V', 'H', 'U'), "D3DFMT_NVHU"},
                                                         {(D3DFORMAT) MAKEFOURCC('E', 'X', 'T', '1'), "D3DFMT_EXT1"},
                                                         {(D3DFORMAT) MAKEFOURCC('F', 'X', 'T', '1'), "D3DFMT_FXT1"},
                                                         {(D3DFORMAT) MAKEFOURCC('G', 'X', 'T', '1'), "D3DFMT_GXT1"},
                                                         {(D3DFORMAT) MAKEFOURCC('H', 'X', 'T', '1'), "D3DFMT_HXT1"},
                                                         {(D3DFORMAT) MAKEFOURCC('A', 'L', '1', '6'), "D3DFMT_AL16"},
                                                         {(D3DFORMAT) MAKEFOURCC('A', 'R', '1', '6'), "D3DFMT_AL16"},
                                                         {(D3DFORMAT) MAKEFOURCC(' ', 'R', '1', '6'), "D3DFMT_R16"},
                                                         // L16 already exists as a dedicated format, but some
                                                         // games also query the below FOURCC for some reason...
                                                         {(D3DFORMAT) MAKEFOURCC(' ', 'L', '1', '6'), "D3DFMT_L16_FOURCC"} };

            std::map<D3DFORMAT, char const*>::iterator formatIter;

            Com<IDirect3DSurface8> surface;
            Com<IDirect3DTexture8> texture;
            Com<IDirect3DCubeTexture8> cubeTexture;
            Com<IDirect3DVolumeTexture8> volumeTexture;

            uint32_t supportedFormats = 0;

            if (!Usage)
                std::cout << std::endl << "Running texture format tests:" << std::endl;
            else if (Usage == D3DUSAGE_RENDERTARGET)
                std::cout << std::endl << "Running render target format tests:" << std::endl;
            else if (Usage == D3DUSAGE_DEPTHSTENCIL)
                std::cout << std::endl << "Running depth stencil format tests:" << std::endl;

            for (formatIter = formats.begin(); formatIter != formats.end(); ++formatIter) {
                D3DFORMAT surfaceFormat = formatIter->first;

                // skip checking ATI1/2 support for depth stencil, as that apparently hangs Nvidia native in D3D8...
                if (Usage == D3DUSAGE_DEPTHSTENCIL && (surfaceFormat == (D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '1') ||
                                                       surfaceFormat == (D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '2')))
                    continue;

                HRESULT statusSurface       = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, Usage, D3DRTYPE_SURFACE, surfaceFormat);
                HRESULT statusTexture       = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, Usage, D3DRTYPE_TEXTURE, surfaceFormat);
                HRESULT statusCubeTexture   = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, Usage, D3DRTYPE_CUBETEXTURE, surfaceFormat);
                HRESULT statusVolumeTexture = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, Usage, D3DRTYPE_VOLUMETEXTURE, surfaceFormat);

                char surfaceSupport = '-';
                char textureSupport = '-';
                char cubeTextureSupport = '-';
                char volumeTextureSupport = '-';

                if (SUCCEEDED(statusSurface))
                    surfaceSupport = '+';
                if (SUCCEEDED(statusTexture))
                    textureSupport = '+';
                if (SUCCEEDED(statusCubeTexture))
                    cubeTextureSupport = '+';
                if (SUCCEEDED(statusVolumeTexture))
                    volumeTextureSupport = '+';

                bool     hasFailures = false;
                uint32_t testedTypes = 1;
                uint32_t supportedTypes = 0;
                char     surfaceCreate = '~';
                char     textureCreate = '~';
                char     cubeTextureCreate = '~';
                char     volumeTextureCreate = '~';

                if (!Usage) {
                    // D3DFMT_R8G8B8 is used in Hidden & Dangerous Deluxe;
                    // D3DFMT_P8 (8-bit palleted textures) are required by some early d3d8 games
                    statusSurface = m_device->CreateImageSurface(256, 256, surfaceFormat, &surface);
                } else if (Usage == D3DUSAGE_RENDERTARGET) {
                    statusSurface = m_device->CreateRenderTarget(256, 256, surfaceFormat, D3DMULTISAMPLE_NONE, FALSE, &surface);
                } else if (Usage == D3DUSAGE_DEPTHSTENCIL) {
                    statusSurface = m_device->CreateDepthStencilSurface(256, 256, surfaceFormat, D3DMULTISAMPLE_NONE, &surface);
                }

                if (FAILED(statusSurface)) {
                    surfaceCreate = '-';
                } else {
                    supportedTypes++;
                    surfaceCreate = '+';
                    surface = nullptr;
                }

                if (SUCCEEDED(statusTexture)) {
                    testedTypes++;
                    // D3DFMT_L6V5U5 is used in Star Wars: Republic Commando for bump mapping;
                    // LotR: Fellowship of the Ring atempts to create a D3DFMT_P8 texture directly
                    statusTexture = m_device->CreateTexture(256, 256, 1, Usage, surfaceFormat, D3DPOOL_DEFAULT, &texture);

                    if (FAILED(statusTexture)) {
                        hasFailures = true;
                        textureCreate = '-';
                    } else {
                        supportedTypes++;
                        textureCreate = '+';
                        texture = nullptr;
                    }
                }

                if (SUCCEEDED(statusCubeTexture)) {
                    testedTypes++;

                    statusCubeTexture = m_device->CreateCubeTexture(256, 1, Usage, surfaceFormat, D3DPOOL_DEFAULT, &cubeTexture);

                    if (FAILED(statusCubeTexture)) {
                        hasFailures = true;
                        cubeTextureCreate = '-';
                    } else {
                        supportedTypes++;
                        cubeTextureCreate = '+';
                        cubeTexture = nullptr;
                    }
                }

                if (SUCCEEDED(statusVolumeTexture)) {
                    testedTypes++;

                    statusVolumeTexture = m_device->CreateVolumeTexture(256, 256, 256, 1, Usage, surfaceFormat, D3DPOOL_DEFAULT, &volumeTexture);

                    if (FAILED(statusVolumeTexture)) {
                        hasFailures = true;
                        volumeTextureCreate = '-';
                    } else {
                        supportedTypes++;
                        volumeTextureCreate = '+';
                        volumeTexture = nullptr;
                    }
                }

                if (hasFailures) {
                    std::cout << format("  ! The ", formatIter->second, " format is improperly supported") << std::endl;
                } else if (testedTypes > 2 && testedTypes - 1 <= supportedTypes) {
                    supportedFormats++;
                    std::cout << format("  + The ", formatIter->second, " format is fully supported") << std::endl;
                } else if (supportedTypes) {
                    supportedFormats++;
                    std::cout << format("  ~ The ", formatIter->second, " format is partially supported") << std::endl;
                } else {
                    std::cout << format("  - The ", formatIter->second, " format is not supported") << std::endl;
                }

                if (!Usage)
                    std::cout << format("      Surface: ", surfaceSupport, "/", surfaceCreate);
                else if (Usage == D3DUSAGE_RENDERTARGET)
                    std::cout << format("      RT: ", surfaceSupport, "/", surfaceCreate);
                else if (Usage == D3DUSAGE_DEPTHSTENCIL)
                    std::cout << format("      DS: ", surfaceSupport, "/", surfaceCreate);

                std::cout << format("  Texture: ", textureSupport, "/", textureCreate,
                                    "  CubeTexture: ", cubeTextureSupport, "/", cubeTextureCreate,
                                    "  VolumeTexture: ", volumeTextureSupport, "/", volumeTextureCreate) << std::endl;
            }

            std::cout << format("A total of ", supportedFormats, " formats are supported") << std::endl;
        }

        // Multisample support check
        void listMultisampleSupport(BOOL windowed) {
            std::map<D3DMULTISAMPLE_TYPE, char const*> msTypes = { {D3DMULTISAMPLE_NONE,       "D3DMULTISAMPLE_NONE"},
                                                                   {D3DMULTISAMPLE_2_SAMPLES,  "D3DMULTISAMPLE_2_SAMPLES"},
                                                                   {D3DMULTISAMPLE_3_SAMPLES,  "D3DMULTISAMPLE_3_SAMPLES"},
                                                                   {D3DMULTISAMPLE_4_SAMPLES,  "D3DMULTISAMPLE_4_SAMPLES"},
                                                                   {D3DMULTISAMPLE_5_SAMPLES,  "D3DMULTISAMPLE_5_SAMPLES"},
                                                                   {D3DMULTISAMPLE_6_SAMPLES,  "D3DMULTISAMPLE_6_SAMPLES"},
                                                                   {D3DMULTISAMPLE_7_SAMPLES,  "D3DMULTISAMPLE_7_SAMPLES"},
                                                                   {D3DMULTISAMPLE_8_SAMPLES,  "D3DMULTISAMPLE_8_SAMPLES"},
                                                                   {D3DMULTISAMPLE_9_SAMPLES,  "D3DMULTISAMPLE_9_SAMPLES"},
                                                                   {D3DMULTISAMPLE_10_SAMPLES, "D3DMULTISAMPLE_10_SAMPLES"},
                                                                   {D3DMULTISAMPLE_11_SAMPLES, "D3DMULTISAMPLE_11_SAMPLES"},
                                                                   {D3DMULTISAMPLE_12_SAMPLES, "D3DMULTISAMPLE_12_SAMPLES"},
                                                                   {D3DMULTISAMPLE_13_SAMPLES, "D3DMULTISAMPLE_13_SAMPLES"},
                                                                   {D3DMULTISAMPLE_14_SAMPLES, "D3DMULTISAMPLE_14_SAMPLES"},
                                                                   {D3DMULTISAMPLE_15_SAMPLES, "D3DMULTISAMPLE_15_SAMPLES"},
                                                                   {D3DMULTISAMPLE_16_SAMPLES, "D3DMULTISAMPLE_16_SAMPLES"} };

            std::map<D3DMULTISAMPLE_TYPE, char const*>::iterator msTypesIter;

            std::cout << std::endl;
            if (windowed)
                std::cout << "Multisample type support (windowed):" << std::endl;
            else
                std::cout << "Multisample type support (full screen):" << std::endl;

            for (msTypesIter = msTypes.begin(); msTypesIter != msTypes.end(); ++msTypesIter) {

                HRESULT status = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, windowed, msTypesIter->first);

                if (FAILED(status)) {
                    std::cout << format("  - ", msTypesIter->second ," is not supported") << std::endl;
                } else {
                    std::cout << format("  + ", msTypesIter->second ," is supported") << std::endl;
                }
            }
        }

        // Vendor format hacks support check
        void listVendorFormatHacksSupport() {
            D3DFORMAT fmtNULL = (D3DFORMAT) MAKEFOURCC('N', 'U', 'L', 'L');
            D3DFORMAT fmtRESZ = (D3DFORMAT) MAKEFOURCC('R', 'E', 'S', 'Z');
            D3DFORMAT fmtATOC = (D3DFORMAT) MAKEFOURCC('A', 'T', 'O', 'C');
            D3DFORMAT fmtSSAA = (D3DFORMAT) MAKEFOURCC('S', 'S', 'A', 'A');
            D3DFORMAT fmtNVDB = (D3DFORMAT) MAKEFOURCC('N', 'V', 'D', 'B');
            D3DFORMAT fmtR2VB = (D3DFORMAT) MAKEFOURCC('R', '2', 'V', 'B');
            D3DFORMAT fmtINST = (D3DFORMAT) MAKEFOURCC('I', 'N', 'S', 'T');
            // ATI/AMD and possibly Nvidia (alterate) pixel center hack
            D3DFORMAT fmtCENT = (D3DFORMAT) MAKEFOURCC('C', 'E', 'N', 'T');

            std::cout << std::endl << "Vendor hack format support:" << std::endl;

            HRESULT statusNULL = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, fmtNULL);

            if (FAILED(statusNULL)) {
                std::cout << "  - The NULL render target format is not supported" << std::endl;
            } else {
                std::cout << "  + The NULL render target format is supported" << std::endl;
            }

            HRESULT statusRESZ = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, fmtRESZ);

            if (FAILED(statusRESZ)) {
                std::cout << "  - The RESZ render target format is not supported" << std::endl;
            } else {
                std::cout << "  + The RESZ render target format is supported" << std::endl;
            }

            HRESULT statusATOC = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtATOC);

            if (FAILED(statusATOC)) {
                std::cout << "  - The ATOC format is not supported" << std::endl;
            } else {
                std::cout << "  + The ATOC format is supported" << std::endl;
            }

            HRESULT statusSSAA = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtSSAA);

            if (FAILED(statusSSAA)) {
                std::cout << "  - The SSAA format is not supported" << std::endl;
            } else {
                std::cout << "  + The SSAA format is supported" << std::endl;
            }

            HRESULT statusNVDB = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtNVDB);

            if (FAILED(statusNVDB)) {
                std::cout << "  - The NVDB format is not supported" << std::endl;
            } else {
                std::cout << "  + The NVDB format is supported" << std::endl;
            }

            HRESULT statusR2VB = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtR2VB);

            if (FAILED(statusR2VB)) {
                std::cout << "  - The R2VB format is not supported" << std::endl;
            } else {
                std::cout << "  + The R2VB format is supported" << std::endl;
            }

            HRESULT statusINST = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtINST);

            if (FAILED(statusINST)) {
                std::cout << "  - The INST format is not supported" << std::endl;
            } else {
                std::cout << "  + The INST format is supported" << std::endl;
            }

            HRESULT statusCENT = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtCENT);

            if (FAILED(statusCENT)) {
                std::cout << "  - The CENT format is not supported" << std::endl;
            } else {
                std::cout << "  + The CENT format is supported" << std::endl;
            }
        }

        // D3D Device capabilities check
        void listDeviceCapabilities() {
            D3DCAPS8 caps8SWVP;
            D3DCAPS8 caps8HWVP;
            D3DCAPS8 caps8;

            // these are all the possible D3D8 VS versions
            std::map<DWORD, char const*> vsVersion = { {D3DVS_VERSION(0,0), "0.0"},
                                                       {D3DVS_VERSION(1,0), "1.0"},
                                                       {D3DVS_VERSION(1,1), "1.1"} };

            // these are all the possible D3D8 PS versions
            std::map<DWORD, char const*> psVersion = { {D3DPS_VERSION(0,0), "0.0"},
                                                       {D3DPS_VERSION(1,0), "1.0"},
                                                       {D3DPS_VERSION(1,1), "1.1"},
                                                       {D3DPS_VERSION(1,2), "1.2"},
                                                       {D3DPS_VERSION(1,3), "1.3"},
                                                       {D3DPS_VERSION(1,4), "1.4"} };

            // get the capabilities from the D3D device in SWVP mode
            createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);
            m_device->GetDeviceCaps(&caps8SWVP);

            // get the capabilities from the D3D device in HWVP mode
            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);
            m_device->GetDeviceCaps(&caps8HWVP);

            // get the capabilities from the D3D interface
            m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps8);

            std::cout << std::endl << "Listing device capabilities support:" << std::endl;

            if (caps8.Caps2 & D3DCAPS2_NO2DDURING3DSCENE)
                std::cout << "  + D3DCAPS2_NO2DDURING3DSCENE is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS2_NO2DDURING3DSCENE is not supported" << std::endl;

            if (caps8.DevCaps & D3DDEVCAPS_QUINTICRTPATCHES)
                std::cout << "  + D3DDEVCAPS_QUINTICRTPATCHES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_QUINTICRTPATCHES is not supported" << std::endl;

            if (caps8.DevCaps & D3DDEVCAPS_RTPATCHES)
                std::cout << "  + D3DDEVCAPS_RTPATCHES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_RTPATCHES is not supported" << std::endl;

            if (caps8.DevCaps & D3DDEVCAPS_RTPATCHHANDLEZERO)
                std::cout << "  + D3DDEVCAPS_RTPATCHHANDLEZERO is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_RTPATCHHANDLEZERO is not supported" << std::endl;

            if (caps8.DevCaps & D3DDEVCAPS_NPATCHES)
                std::cout << "  + D3DDEVCAPS_NPATCHES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_NPATCHES is not supported" << std::endl;

            // depends on D3DPRASTERCAPS_PAT
            if (caps8.PrimitiveMiscCaps & D3DPMISCCAPS_LINEPATTERNREP)
                std::cout << "  + D3DPMISCCAPS_LINEPATTERNREP is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_LINEPATTERNREP is not supported" << std::endl;

            // WineD3D supports it, but native drivers do not
            if (caps8.RasterCaps & D3DPRASTERCAPS_PAT)
                std::cout << "  + D3DPRASTERCAPS_PAT is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_PAT is not supported" << std::endl;

            // this isn't typically exposed even on native
            if (caps8.RasterCaps & D3DPRASTERCAPS_ANTIALIASEDGES)
                std::cout << "  + D3DPRASTERCAPS_ANTIALIASEDGES is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_ANTIALIASEDGES is not supported" << std::endl;

            if (caps8.RasterCaps & D3DPRASTERCAPS_STRETCHBLTMULTISAMPLE)
                std::cout << "  + D3DPRASTERCAPS_STRETCHBLTMULTISAMPLE is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_STRETCHBLTMULTISAMPLE is not supported" << std::endl;

            if (caps8.TextureFilterCaps & D3DPTFILTERCAPS_MAGFAFLATCUBIC)
                std::cout << "  + D3DPTFILTERCAPS_MAGFAFLATCUBIC is (texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFAFLATCUBIC is (texture) not supported" << std::endl;

            if (caps8.TextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC (texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC (texture) is not supported" << std::endl;

            if (caps8.CubeTextureFilterCaps & D3DPTFILTERCAPS_MAGFAFLATCUBIC)
                std::cout << "  + D3DPTFILTERCAPS_MAGFAFLATCUBIC (cube texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFAFLATCUBIC (cube texture) is not supported" << std::endl;

            if (caps8.CubeTextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC (cube texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC (cube texture) is not supported" << std::endl;

            if (caps8.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MAGFAFLATCUBIC)
                std::cout << "  + D3DPTFILTERCAPS_MAGFAFLATCUBIC (volume texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFAFLATCUBIC (volume texture) is not supported" << std::endl;

            if (caps8.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC (volume texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC (volume texture) is not supported" << std::endl;

            if (caps8.VertexProcessingCaps & D3DVTXPCAPS_NO_VSDT_UBYTE4)
                std::cout << "  + D3DVTXPCAPS_NO_VSDT_UBYTE4 is supported" << std::endl;
            else
                std::cout << "  - D3DVTXPCAPS_NO_VSDT_UBYTE4 is not supported" << std::endl;

            std::cout << std::endl << "Listing device capability limits:" << std::endl;

            std::cout << format("  ~ MaxTextureWidth: ", caps8.MaxTextureWidth) << std::endl;
            std::cout << format("  ~ MaxTextureHeight: ", caps8.MaxTextureHeight) << std::endl;
            std::cout << format("  ~ MaxVolumeExtent: ", caps8.MaxVolumeExtent) << std::endl;
            std::cout << format("  ~ MaxTextureRepeat: ", caps8.MaxTextureRepeat) << std::endl;
            std::cout << format("  ~ MaxTextureAspectRatio: ", caps8.MaxTextureAspectRatio) << std::endl;
            std::cout << format("  ~ MaxAnisotropy: ", caps8.MaxAnisotropy) << std::endl;
            std::cout << format("  ~ MaxVertexW: ", caps8.MaxVertexW) << std::endl;
            std::cout << format("  ~ GuardBandLeft: ", caps8.GuardBandLeft) << std::endl;
            std::cout << format("  ~ GuardBandTop: ", caps8.GuardBandTop) << std::endl;
            std::cout << format("  ~ GuardBandRight: ", caps8.GuardBandRight) << std::endl;
            std::cout << format("  ~ GuardBandBottom: ", caps8.GuardBandBottom) << std::endl;
            std::cout << format("  ~ ExtentsAdjust: ", caps8.ExtentsAdjust) << std::endl;
            std::cout << format("  ~ MaxTextureBlendStages: ", caps8.MaxTextureBlendStages) << std::endl;
            std::cout << format("  ~ MaxSimultaneousTextures: ", caps8.MaxSimultaneousTextures) << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxActiveLights: ", caps8.MaxActiveLights, " (I), ", caps8SWVP.MaxActiveLights,
                                " (SWVP), ", caps8HWVP.MaxActiveLights, " (HWVP)") << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxUserClipPlanes: ", caps8.MaxUserClipPlanes, " (I), ", caps8SWVP.MaxUserClipPlanes,
                                " (SWVP), ", caps8HWVP.MaxUserClipPlanes, " (HWVP)") << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxVertexBlendMatrices: ", caps8.MaxVertexBlendMatrices, " (I), ", caps8SWVP.MaxVertexBlendMatrices,
                                " (SWVP), ", caps8HWVP.MaxVertexBlendMatrices, " (HWVP)") << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxVertexBlendMatrixIndex: ", caps8.MaxVertexBlendMatrixIndex, " (I), ", caps8SWVP.MaxVertexBlendMatrixIndex,
                                " (SWVP), ", caps8HWVP.MaxVertexBlendMatrixIndex, " (HWVP)") << std::endl;
            std::cout << format("  ~ MaxPointSize: ", caps8.MaxPointSize) << std::endl;
            std::cout << format("  ~ MaxPrimitiveCount: ", caps8.MaxPrimitiveCount) << std::endl;
            std::cout << format("  ~ MaxVertexIndex: ", caps8.MaxVertexIndex) << std::endl;
            std::cout << format("  ~ MaxStreams: ", caps8.MaxStreams) << std::endl;
            std::cout << format("  ~ MaxStreamStride: ", caps8.MaxStreamStride) << std::endl;
            std::cout << format("  ~ VertexShaderVersion: ", vsVersion[caps8.VertexShaderVersion]) << std::endl;
            std::cout << format("  ~ MaxVertexShaderConst: ", caps8.MaxVertexShaderConst) << std::endl;
            std::cout << format("  ~ PixelShaderVersion: ", psVersion[caps8.PixelShaderVersion]) << std::endl;
            // typically FLT_MAX
            std::cout << format("  ~ MaxPixelShaderValue: ", caps8.MaxPixelShaderValue) << std::endl;
        }

        // D3D8 Devices shouldn't typically expose any D3D9 capabilities
        void listDeviceD3D9Capabilities() {
            D3DCAPS8 caps8;

            // get the capabilities from the D3D interface
            m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps8);

            std::cout << std::endl << "Listing device D3D9 capabilities support:" << std::endl;

            if (caps8.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP)
                std::cout << "  + D3DCAPS2_CANAUTOGENMIPMAP is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS2_CANAUTOGENMIPMAP is not supported" << std::endl;

            if (caps8.Caps3 & D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION)
                std::cout << "  + D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION is not supported" << std::endl;

            if (caps8.Caps3 & D3DCAPS3_COPY_TO_VIDMEM)
                std::cout << "  + D3DCAPS3_COPY_TO_VIDMEM is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS3_COPY_TO_VIDMEM is not supported" << std::endl;

            if (caps8.Caps3 & D3DCAPS3_COPY_TO_SYSTEMMEM)
                std::cout << "  + D3DCAPS3_COPY_TO_SYSTEMMEM is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS3_COPY_TO_SYSTEMMEM is not supported" << std::endl;

            if (caps8.PrimitiveMiscCaps & D3DPMISCCAPS_INDEPENDENTWRITEMASKS)
                std::cout << "  + D3DPMISCCAPS_INDEPENDENTWRITEMASKS is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_INDEPENDENTWRITEMASKS is not supported" << std::endl;

            if (caps8.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT)
                std::cout << "  + D3DPMISCCAPS_PERSTAGECONSTANT is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_PERSTAGECONSTANT is not supported" << std::endl;

            if (caps8.PrimitiveMiscCaps & D3DPMISCCAPS_FOGANDSPECULARALPHA)
                std::cout << "  + D3DPMISCCAPS_FOGANDSPECULARALPHA is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_FOGANDSPECULARALPHA is not supported" << std::endl;

            if (caps8.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND)
                std::cout << "  + D3DPMISCCAPS_SEPARATEALPHABLEND is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_SEPARATEALPHABLEND is not supported" << std::endl;

            if (caps8.PrimitiveMiscCaps & D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS)
                std::cout << "  + D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS is not supported" << std::endl;

            if (caps8.PrimitiveMiscCaps & D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING)
                std::cout << "  + D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING is not supported" << std::endl;

            if (caps8.PrimitiveMiscCaps & D3DPMISCCAPS_FOGVERTEXCLAMPED)
                std::cout << "  + D3DPMISCCAPS_FOGVERTEXCLAMPED is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_FOGVERTEXCLAMPED is not supported" << std::endl;

            if (caps8.PrimitiveMiscCaps & D3DPMISCCAPS_POSTBLENDSRGBCONVERT)
                std::cout << "  + D3DPMISCCAPS_POSTBLENDSRGBCONVERT is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_POSTBLENDSRGBCONVERT is not supported" << std::endl;

            if (caps8.RasterCaps & D3DPRASTERCAPS_SCISSORTEST)
                std::cout << "  + D3DPRASTERCAPS_SCISSORTEST is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_SCISSORTEST is not supported" << std::endl;

            if (caps8.RasterCaps & D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS)
                std::cout << "  + D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS is not supported" << std::endl;

            if (caps8.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS)
                std::cout << "  + D3DPRASTERCAPS_DEPTHBIAS is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_DEPTHBIAS is not supported" << std::endl;

            if (caps8.RasterCaps & D3DPRASTERCAPS_MULTISAMPLE_TOGGLE)
                std::cout << "  + D3DPRASTERCAPS_MULTISAMPLE_TOGGLE is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_MULTISAMPLE_TOGGLE is not supported" << std::endl;

            if (caps8.SrcBlendCaps & D3DPBLENDCAPS_BLENDFACTOR)
                std::cout << "  + D3DPBLENDCAPS_BLENDFACTOR (Src) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_BLENDFACTOR (Src) is not supported" << std::endl;

            if (caps8.SrcBlendCaps & D3DPBLENDCAPS_INVSRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_INVSRCCOLOR2 (Src) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_INVSRCCOLOR2 (Src) is not supported" << std::endl;

            if (caps8.SrcBlendCaps & D3DPBLENDCAPS_SRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_SRCCOLOR2 (Src) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_SRCCOLOR2 (Src) is not supported" << std::endl;

            if (caps8.DestBlendCaps & D3DPBLENDCAPS_BLENDFACTOR)
                std::cout << "  + D3DPBLENDCAPS_BLENDFACTOR (Dest) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_BLENDFACTOR (Dest) is not supported" << std::endl;

            if (caps8.DestBlendCaps & D3DPBLENDCAPS_INVSRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_INVSRCCOLOR2 (Dest) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_INVSRCCOLOR2 (Dest) is not supported" << std::endl;

            if (caps8.DestBlendCaps & D3DPBLENDCAPS_SRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_SRCCOLOR2 (Dest) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_SRCCOLOR2 (Dest) is not supported" << std::endl;

            if (caps8.TextureFilterCaps & D3DPTFILTERCAPS_CONVOLUTIONMONO)
                std::cout << "  + D3DPTFILTERCAPS_CONVOLUTIONMONO is (texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_CONVOLUTIONMONO is (texture) not supported" << std::endl;

            if (caps8.TextureFilterCaps & D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (texture) not supported" << std::endl;

            if (caps8.TextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (texture) is not supported" << std::endl;

            if (caps8.TextureFilterCaps & D3DPTFILTERCAPS_MINFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (texture) not supported" << std::endl;

            if (caps8.TextureFilterCaps & D3DPTFILTERCAPS_MINFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFGAUSSIANQUAD (texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFGAUSSIANQUAD (texture) is not supported" << std::endl;

            if (caps8.CubeTextureFilterCaps & D3DPTFILTERCAPS_CONVOLUTIONMONO)
                std::cout << "  + D3DPTFILTERCAPS_CONVOLUTIONMONO is (cube texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_CONVOLUTIONMONO is (cube texture) not supported" << std::endl;

            if (caps8.CubeTextureFilterCaps & D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (cube texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (cube texture) not supported" << std::endl;

            if (caps8.CubeTextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (cube texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (cube texture) is not supported" << std::endl;

            if (caps8.CubeTextureFilterCaps & D3DPTFILTERCAPS_MINFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (cube texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (cube texture) not supported" << std::endl;

            if (caps8.CubeTextureFilterCaps & D3DPTFILTERCAPS_MINFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFGAUSSIANQUAD (cube texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFGAUSSIANQUAD (cube texture) is not supported" << std::endl;

            if (caps8.VolumeTextureFilterCaps & D3DPTFILTERCAPS_CONVOLUTIONMONO)
                std::cout << "  + D3DPTFILTERCAPS_CONVOLUTIONMONO is (volume texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_CONVOLUTIONMONO is (volume texture) not supported" << std::endl;

            if (caps8.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (volume texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (volume texture) not supported" << std::endl;

            if (caps8.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (volume texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (volume texture) is not supported" << std::endl;

            if (caps8.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MINFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (volume texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (volume texture) not supported" << std::endl;

            if (caps8.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MINFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFGAUSSIANQUAD (volume texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFGAUSSIANQUAD (volume texture) is not supported" << std::endl;

            if (caps8.LineCaps & D3DLINECAPS_ANTIALIAS)
                std::cout << "  + D3DLINECAPS_ANTIALIAS is supported" << std::endl;
            else
                std::cout << "  - D3DLINECAPS_ANTIALIAS is not supported" << std::endl;

            if (caps8.StencilCaps & D3DSTENCILCAPS_TWOSIDED)
                std::cout << "  + D3DSTENCILCAPS_TWOSIDED is supported" << std::endl;
            else
                std::cout << "  - D3DSTENCILCAPS_TWOSIDED is not supported" << std::endl;

            if (caps8.VertexProcessingCaps & D3DVTXPCAPS_TEXGEN_SPHEREMAP)
                std::cout << "  + D3DVTXPCAPS_TEXGEN_SPHEREMAP is supported" << std::endl;
            else
                std::cout << "  - D3DVTXPCAPS_TEXGEN_SPHEREMAP is not supported" << std::endl;

            if (caps8.VertexProcessingCaps & D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER)
                std::cout << "  + D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER is supported" << std::endl;
            else
                std::cout << "  - D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER is not supported" << std::endl;
        }

        void listVCacheQueryResult() {
            resetOrRecreateDevice();

            D3DDEVINFO_VCACHE vCache;
            HRESULT status = m_device->GetInfo(D3DDEVINFOID_VCACHE, &vCache, sizeof(D3DDEVINFO_VCACHE));

            std::cout << std::endl << "Listing VCache query result:" << std::endl;

            switch (status) {
                // NVIDIA will return D3D_OK and some values in vCache
                case D3D_OK:
                    std::cout << "  ~ Response: D3D_OK" << std::endl;
                    break;
                // ATI/AMD and Intel will return S_FALSE and all 0s in vCache
                case S_FALSE:
                    std::cout << "  ~ Response: S_FALSE" << std::endl;
                    break;
                case E_FAIL:
                    std::cout << "  ~ Response: E_FAIL" << std::endl;
                    break;
                // shouldn't be returned by any vendor
                case D3DERR_NOTAVAILABLE:
                    std::cout << "  ~ Response: D3DERR_NOTAVAILABLE" << std::endl;
                    break;
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

        void listAvailableTextureMemory() {
            resetOrRecreateDevice();

            std::cout << std::endl << "Listing available texture memory:" << std::endl;

            uint32_t availableMemory = m_device->GetAvailableTextureMem();

            std::cout << format("  ~ Bytes: ", availableMemory) << std::endl;
        }

        void startTests() {
            m_totalTests  = 0;
            m_passedTests = 0;

            std::cout << std::endl << "Running D3D8 tests:" << std::endl;
        }

        // GetBackBuffer test (this shouldn't fail even with BackBufferCount set to 0)
        void testZeroBackBufferCount() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface8> bbSurface;

            m_totalTests++;

            HRESULT status = m_device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &bbSurface);
            if (FAILED(status)) {
                std::cout << "  - The GetBackBuffer test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The GetBackBuffer test has passed" << std::endl;
            }
        }

        void testRSValues() {
            resetOrRecreateDevice();

            D3DLINEPATTERN linePatternTwo = { 2, 2 };
            float patchSegmentsTwo = 2.0f;

            m_totalTests++;

            m_device->SetRenderState(D3DRS_LINEPATTERN, bit::cast<DWORD>(linePatternTwo));
            // ZVISIBLE is not supported, but set values are saved apparently
            m_device->SetRenderState(D3DRS_ZVISIBLE, 2);
            // Integer value between 0 and 16
            m_device->SetRenderState(D3DRS_ZBIAS, 2);
            m_device->SetRenderState(D3DRS_PATCHSEGMENTS, bit::cast<DWORD>(patchSegmentsTwo));
            DWORD linePatternWord;
            m_device->GetRenderState(D3DRS_LINEPATTERN, &linePatternWord);
            D3DLINEPATTERN linePattern = bit::cast<D3DLINEPATTERN>(linePatternWord);
            //std::cout << format("  * D3DRS_LINEPATTERN: ", linePattern.wRepeatFactor, " ", linePattern.wLinePattern) << std::endl;
            DWORD zVisible = 0;
            m_device->GetRenderState(D3DRS_ZVISIBLE, &zVisible);
            //std::cout << format("  * D3DRS_ZVISIBLE: ", zVisible) << std::endl;
            DWORD zBias = 0;
            m_device->GetRenderState(D3DRS_ZBIAS, &zBias);
            //std::cout << format("  * D3DRS_ZBIAS: ", zBias) << std::endl;
            float patchSegments = 0;
            DWORD patchSegmentsWord;
            m_device->GetRenderState(D3DRS_PATCHSEGMENTS, &patchSegmentsWord);
            patchSegments = bit::cast<float>(patchSegmentsWord);
            //std::cout << format("  * D3DRS_PATCHSEGMENTS: ", patchSegments) << std::endl;

            if (linePattern.wRepeatFactor == 2
             && linePattern.wLinePattern  == 2
             && zVisible == 2
             && zBias == 2
             && patchSegments == 2.0f) {
                m_passedTests++;
                std::cout << "  + The render state values test has passed" << std::endl;
            } else {
                std::cout << "  - The render state values test has failed" << std::endl;
            }
        }

        // Invalid presentation interval test on a windowed swapchain
        void testInvalidPresentationInterval() {
            D3DPRESENT_PARAMETERS piPP;

            m_totalTests++;

            memcpy(&piPP, &m_pp, sizeof(m_pp));
            piPP.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

            HRESULT statusImmediate = createDeviceWithFlags(&piPP, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);

            memcpy(&piPP, &m_pp, sizeof(m_pp));
            piPP.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;

            HRESULT statusOne = createDeviceWithFlags(&piPP, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);

            memcpy(&piPP, &m_pp, sizeof(m_pp));
            piPP.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_TWO;

            HRESULT statusTwo = createDeviceWithFlags(&piPP, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);

            if (FAILED(statusImmediate) && FAILED(statusOne) && FAILED(statusTwo)) {
                m_passedTests++;
                std::cout << "  + The invalid presentation interval test has passed" << std::endl;
            } else {
                std::cout << "  - The invalid presentation interval test has failed" << std::endl;
            }
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
                    // Reset() will have cleared the state
                    if (FAILED(m_device->EndScene()) && SUCCEEDED(m_device->BeginScene())) {
                        m_passedTests++;
                        std::cout << "  + The BeginScene & Reset test has passed" << std::endl;
                    } else {
                        std::cout << "  - The BeginScene & Reset test has failed" << std::endl;
                    }
                }
            } else {
                throw Error("Failed to begin D3D8 scene");
            }

            // optimistic, expected to fail if the test fails
            m_device->EndScene();
        }

        // D3DCREATE_PUREDEVICE with SWVP render state test
        // (games like Massive Assault try to enable SWVP in HWVP/PUREDEVICE mode)
        void testPureDeviceSetSWVPRenderState() {
            HRESULT status = createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, D3DDEVTYPE_HAL, false);

            if (FAILED(status)) {
                std::cout << "  ~ The PUREDEVICE mode is not supported" << std::endl;
            } else {
                m_totalTests++;

                status = m_device->SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING, TRUE);
                if (FAILED(status)) {
                    std::cout << "  - The SWVP RS in PUREDEVICE mode test has failed" << std::endl;
                } else {
                    m_passedTests++;
                    std::cout << "  + The SWVP RS in PUREDEVICE mode test has passed" << std::endl;
                }
            }
        }

        // D3DCREATE_PUREDEVICE only with D3DCREATE_HARDWARE_VERTEXPROCESSING test
        void testPureDeviceOnlyWithHWVP() {
            // native drivers will fail to create a device with D3DCREATE_PUREDEVICE unless
            // it is combined together with D3DCREATE_HARDWARE_VERTEXPROCESSING
            HRESULT statusHWVP = createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, D3DDEVTYPE_HAL, false);
            HRESULT statusSWVP = createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, D3DDEVTYPE_HAL, false);
            HRESULT statusMVP = createDeviceWithFlags(&m_pp, D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, D3DDEVTYPE_HAL, false);

            m_totalTests++;

            if (SUCCEEDED(statusHWVP) && FAILED(statusSWVP) && FAILED(statusMVP)) {
                m_passedTests++;
                std::cout << "  + The PUREDEVICE mode only with HWVP test has passed" << std::endl;
            } else {
                std::cout << "  - The PUREDEVICE mode only with HWVP test has failed" << std::endl;
            }
        }

        // Set/GetClipStatus test on D3DCREATE_HARDWARE_VERTEXPROCESSING/D3DCREATE_MIXED_VERTEXPROCESSING
        void testClipStatus() {
            createDeviceWithFlags(&m_pp, D3DCREATE_MIXED_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);

            D3DCLIPSTATUS8 initialClipStatus = {};
            D3DCLIPSTATUS8 setClipStatus = {D3DCS_FRONT, D3DCS_FRONT};
            D3DCLIPSTATUS8 afterSetClipStatus = {};
            D3DCLIPSTATUS8 finalClipStatus = {};
            D3DCLIPSTATUS8 testClipStatus = {};
            Com<IDirect3DVertexBuffer8> vertexBuffer;

            void* vertices;
            m_device->CreateVertexBuffer(m_rgbVerticesSize, 0, RGBT_FVF_CODES,
                                         D3DPOOL_DEFAULT, &vertexBuffer);
            vertexBuffer->Lock(0, m_rgbVerticesSize, reinterpret_cast<BYTE**>(&vertices), 0);
            memcpy(vertices, m_rgbVertices.data(), m_rgbVerticesSize);
            vertexBuffer->Unlock();

            m_totalTests++;

            HRESULT statusMixed = m_device->GetClipStatus(&initialClipStatus);
            //std::cout << format("  * initialClipStatus.ClipUnion: ", initialClipStatus.ClipUnion) << std::endl;
            //std::cout << format("  * initialClipStatus.ClipIntersection: ", initialClipStatus.ClipIntersection) << std::endl;
            if (SUCCEEDED(statusMixed)) {
                m_device->SetClipStatus(&setClipStatus);
                m_device->GetClipStatus(&afterSetClipStatus);
                //std::cout << format("  * afterSetClipStatus.ClipUnion: ", afterSetClipStatus.ClipUnion) << std::endl;
                //std::cout << format("  * afterSetClipStatus.ClipIntersection: ", afterSetClipStatus.ClipIntersection) << std::endl;
                m_device->BeginScene();
                m_device->SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING, TRUE);
                m_device->SetRenderState(D3DRS_CLIPPING, FALSE);
                m_device->SetStreamSource(0, vertexBuffer.ptr(), sizeof(RGBVERTEX));
                m_device->SetVertexShader(RGBT_FVF_CODES);
                m_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                m_device->EndScene();
                // The D3D8 documentation states: "When D3DRS_CLIPPING is set to FALSE, ClipUnion and ClipIntersection are set to zero.
                // Direct3D updates the clip status during drawing calls.", however this does not happen in practice.
                m_device->GetClipStatus(&finalClipStatus);
                //std::cout << format("  * finalClipStatus.ClipUnion: ", finalClipStatus.ClipUnion) << std::endl;
                //std::cout << format("  * finalClipStatus.ClipIntersection: ", finalClipStatus.ClipIntersection) << std::endl;
            }

            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);

            HRESULT statusHWVPSet = m_device->SetClipStatus(&setClipStatus);
            HRESULT statusHWVPGet = m_device->GetClipStatus(&testClipStatus);
            //std::cout << format("  * testClipStatus.ClipUnion: ", testClipStatus.ClipUnion) << std::endl;
            //std::cout << format("  * testClipStatus.ClipIntersection: ", testClipStatus.ClipIntersection) << std::endl;

            if (SUCCEEDED(statusMixed) && SUCCEEDED(statusHWVPGet) && SUCCEEDED(statusHWVPSet) &&
                initialClipStatus.ClipUnion == 0 && initialClipStatus.ClipIntersection == 0xffffffff &&
                afterSetClipStatus.ClipUnion == D3DCS_FRONT && afterSetClipStatus.ClipIntersection == D3DCS_FRONT &&
                finalClipStatus.ClipUnion == D3DCS_FRONT && finalClipStatus.ClipIntersection == D3DCS_FRONT &&
                testClipStatus.ClipUnion == D3DCS_FRONT && testClipStatus.ClipIntersection == D3DCS_FRONT) {
                m_passedTests++;
                std::cout << "  + The Set/GetClipStatus test has passed" << std::endl;
            } else {
                std::cout << "  - The Set/GetClipStatus test has failed" << std::endl;
            }
        }

        // CreateDevice with various devices types test
        void testDeviceTypes() {
            // D3DDEVTYPE_REF is available on Windows 8 and above
            HRESULT statusHHAL = createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);
            HRESULT statusHSW = createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_SW, false);
            HRESULT statusSHAL = createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);
            HRESULT statusSSW = createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING, D3DDEVTYPE_SW, false);

            m_totalTests++;

            if (SUCCEEDED(statusHHAL) && statusHSW == D3DERR_INVALIDCALL
             && SUCCEEDED(statusSHAL) && statusSSW == D3DERR_INVALIDCALL) {
                m_passedTests++;
                std::cout << "  + The device types test has passed" << std::endl;
            } else {
                std::cout << "  - The device types test has failed" << std::endl;
            }
        }

        // GetDeviceCaps with various device types tests
        void testGetDeviceCapsWithDeviceTypes() {
            D3DCAPS8 caps8;

            // D3DDEVTYPE_REF is available on Windows 8 and above
            HRESULT statusHAL = m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps8);
            HRESULT statusSW = m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_SW, &caps8);

            m_totalTests++;

            if (SUCCEEDED(statusHAL) && statusSW == D3DERR_INVALIDCALL) {
                m_passedTests++;
                std::cout << "  + The GetDeviceCaps with device types test has passed" << std::endl;
            } else {
                std::cout << "  - The GetDeviceCaps with device types test has failed" << std::endl;
            }
        }

        // D3DPOOL_DEFAULT allocation & Reset + D3DERR_DEVICENOTRESET state test (2 in 1)
        void testDefaultPoolAllocationReset() {
            resetOrRecreateDevice();

            // create a temporary DS surface
            Com<IDirect3DSurface8> tempDS;
            m_device->CreateDepthStencilSurface(RGBTriangle::WINDOW_WIDTH, RGBTriangle::WINDOW_HEIGHT,
                                                D3DFMT_D24X8, D3DMULTISAMPLE_NONE, &tempDS);

            m_totalTests++;
            // according to D3D8 docs, I quote: "Reset will fail unless the application releases all resources
            // that are allocated in D3DPOOL_DEFAULT, including those created by the IDirect3DDevice8::CreateRenderTarget
            // and IDirect3DDevice8::CreateDepthStencilSurface methods.", so this call should fail
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

        // CreateVertexShader handle generation test
        void testCreateVertexShaderHandleGeneration() {
            resetOrRecreateDevice();

            // sample shader declaration for FVF
            DWORD dwDecl[] =
            {
                D3DVSD_STREAM(0),
                D3DVSD_REG(D3DVSDE_POSITION,  D3DVSDT_FLOAT3),
                D3DVSD_REG(D3DVSDE_DIFFUSE,   D3DVSDT_D3DCOLOR),
                D3DVSD_END()
            };

            m_totalTests++;

            DWORD firstHandle = 0;
            HRESULT status = m_device->CreateVertexShader(dwDecl, NULL, &firstHandle, 0);
            //std::cout << format("  * First VS handle: ", firstHandle) << std::endl;

            // shader handles start at 3, to skip lower value FVF handles
            if (SUCCEEDED(status) && firstHandle == 3) {
                DWORD secondHandle = 0;
                status = m_device->CreateVertexShader(dwDecl, NULL, &secondHandle, 0);
                //std::cout << format("  * Second VS handle: ", secondHandle) << std::endl;

                if (SUCCEEDED(status) && secondHandle == 5) {
                    m_passedTests++;
                    std::cout << "  + The CreateVertexShader handle generation test has passed" << std::endl;
                } else {
                    std::cout << "  - The CreateVertexShader handle generation test has failed" << std::endl;
                }
            } else {
                std::cout << "  - The CreateVertexShader handle generation test has failed" << std::endl;
            }
        }

        // CreateStateBlock & Reset test
        void testCreateStateBlockAndReset() {
            resetOrRecreateDevice();

            // create a temporary state block
            DWORD stateBlockToken = 0;
            m_device->CreateStateBlock(D3DSBT_ALL, &stateBlockToken);
            //std::cout << format("  * State block token:", stateBlockToken) << std::endl;

            m_totalTests++;
            // D3D8 state blocks survive device Reset() calls and shouldn't be counted as losable resources
            HRESULT status = m_device->Reset(&m_pp);
            if (FAILED(status)) {
                std::cout << "  - The CreateStateBlock & Reset test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The CreateStateBlock & Reset test has passed" << std::endl;
            }

            m_device->DeleteStateBlock(stateBlockToken);
        }

        // CreateStateBlock monotonic tokens test
        void testCreateStateBlockMonotonicTokens(UINT stateBlocksCount) {
            resetOrRecreateDevice();

            std::vector<DWORD> stateBlockTokens;
            DWORD currentStateBlockToken = 0;
            // create stateBlocksCount/2 state blocks
            for (UINT i = 0; i < stateBlocksCount/2; i++) {
                m_device->CreateStateBlock(D3DSBT_ALL, &currentStateBlockToken);
                //std::cout << format("  * State block token:", currentStateBlockToken) << std::endl;
                stateBlockTokens.emplace_back(currentStateBlockToken);
            }
            // delete the last 10
            UINT deleteOffset =  stateBlockTokens.size() - 1;
            for (UINT i = 0; i < 10; i++) {
                m_device->DeleteStateBlock(stateBlockTokens[deleteOffset - i]);
                //std::cout << format("  * Deleted state block token:", stateBlockTokens[deleteOffset - i]) << std::endl;
                stateBlockTokens.pop_back();
            }
            // create another stateBlocksCount/2 state blocks
            for (UINT i = 0; i < stateBlocksCount/2; i++) {
                m_device->CreateStateBlock(D3DSBT_ALL, &currentStateBlockToken);
                //std::cout << format("  * State block token:", currentStateBlockToken) << std::endl;
                stateBlockTokens.emplace_back(currentStateBlockToken);
            }

            m_totalTests++;

            bool monotonicStreak = true;
            // the initial value of the token should be 1
            DWORD monotonicCounter = 1;
            for (auto& stateBlockToken : stateBlockTokens) {
                // subsequent tokens simply increment the initial value
                if (monotonicCounter == stateBlockToken) {
                    monotonicCounter += 1;
                    // release state blocks of checked tokens
                    m_device->DeleteStateBlock(stateBlockToken);
                }
                else {
                    monotonicStreak = false;
                    break;
                }
            }

            if (monotonicStreak) {
                m_passedTests++;
                std::cout << "  + The CreateStateBlock monotonic tokens test has passed" << std::endl;
            } else {
                std::cout << "  - The CreateStateBlock monotonic tokens test has failed" << std::endl;
            }
        }

        // BeginStateBlock & other state block calls test
        void testBeginStateBlockCalls() {
            resetOrRecreateDevice();

            DWORD deleteStateBlockToken = 0;
            DWORD endStateBlockToken = 0;
            m_device->CreateStateBlock(D3DSBT_ALL, &deleteStateBlockToken);
            m_device->BeginStateBlock();

            m_totalTests++;
            // no other calls except EndStateBlock() will succeed insides of a BeginStateBlock()
            HRESULT statusDelete = m_device->DeleteStateBlock(deleteStateBlockToken);
            HRESULT statusBegin = m_device->BeginStateBlock();
            HRESULT statusApply = m_device->ApplyStateBlock(deleteStateBlockToken);
            HRESULT statusCapture = m_device->CaptureStateBlock(deleteStateBlockToken);
            HRESULT statusCreate = m_device->CreateStateBlock(D3DSBT_ALL, &deleteStateBlockToken);
            HRESULT statusEnd = m_device->EndStateBlock(&endStateBlockToken);

            if (FAILED(statusDelete) && FAILED(statusBegin) && FAILED(statusApply)
             && FAILED(statusCapture) && FAILED(statusCreate) && SUCCEEDED(statusEnd)) {
                m_passedTests++;
                std::cout << "  + The BeginStateBlock & other state block calls test has passed" << std::endl;
            } else {
                std::cout << "  - The BeginStateBlock & other state block calls test has failed" << std::endl;
            }

            m_device->DeleteStateBlock(endStateBlockToken);
        }

        // MultiplyTransform with state blocks test
        void testMultiplyTransformRecordingAndCapture() {
            resetOrRecreateDevice();

            D3DMATRIX idMatrix =
            {{{
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            }}};
            D3DMATRIX dblMatrix =
            {{{
                2.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 2.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 2.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 2.0f,
            }}};

            DWORD endStateBlockToken = 0;
            DWORD endStateBlockToken2 = 0;
            DWORD captureStateBlockToken = 0;

            m_totalTests++;

            m_device->CreateStateBlock(D3DSBT_ALL, &captureStateBlockToken);

            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &idMatrix);
            m_device->BeginStateBlock();
            m_device->MultiplyTransform(D3DTS_WORLDMATRIX(0), &dblMatrix);
            m_device->EndStateBlock(&endStateBlockToken);
            D3DMATRIX checkMatrix1;
            m_device->GetTransform(D3DTS_WORLDMATRIX(0), &checkMatrix1);

            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &idMatrix);
            m_device->ApplyStateBlock(endStateBlockToken);
            D3DMATRIX checkMatrix2;
            m_device->GetTransform(D3DTS_WORLDMATRIX(0), &checkMatrix2);

            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &idMatrix);
            m_device->BeginStateBlock();
            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &dblMatrix);
            m_device->EndStateBlock(&endStateBlockToken2);
            D3DMATRIX checkMatrix3;
            m_device->GetTransform(D3DTS_WORLDMATRIX(0), &checkMatrix3);

            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &idMatrix);
            m_device->CaptureStateBlock(captureStateBlockToken);
            m_device->MultiplyTransform(D3DTS_WORLDMATRIX(0), &dblMatrix);
            m_device->ApplyStateBlock(captureStateBlockToken);
            D3DMATRIX checkMatrix4;
            m_device->GetTransform(D3DTS_WORLDMATRIX(0), &checkMatrix4);

            bool firstCheck  = !memcmp(&checkMatrix1, &dblMatrix, sizeof(dblMatrix));
            bool secondCheck = !memcmp(&checkMatrix2, &idMatrix,  sizeof(idMatrix));
            bool thirdCheck  = !memcmp(&checkMatrix3, &idMatrix,  sizeof(idMatrix));
            bool fourthCheck = !memcmp(&checkMatrix4, &idMatrix,  sizeof(idMatrix));

            // Calls to MultiplyTransform are not recorded in state blocks, but are applied directly
            if (firstCheck && secondCheck && thirdCheck && fourthCheck) {
                m_passedTests++;
                std::cout << "  + The MultiplyTransform with state blocks test has passed" << std::endl;
            } else {
                std::cout << "  - The MultiplyTransform with state blocks test has failed" << std::endl;
            }

            m_device->DeleteStateBlock(endStateBlockToken);
            m_device->DeleteStateBlock(captureStateBlockToken);
        }

        // StateBlock calls with an invalid token test
        void testStateBlockWithInvalidToken() {
            resetOrRecreateDevice();

            DWORD invalidToken = 5000;

            m_totalTests++;

            HRESULT captureStatus = m_device->CaptureStateBlock(invalidToken);
            HRESULT applyStatus = m_device->ApplyStateBlock(invalidToken);
            HRESULT deleteStatus = m_device->DeleteStateBlock(invalidToken);

            if (FAILED(captureStatus) && FAILED(applyStatus) && FAILED(deleteStatus)) {
                m_passedTests++;
                std::cout << "  + The StateBlock with invalid token test has passed" << std::endl;
            } else {
                std::cout << "  - The StateBlock with invalid token test has failed" << std::endl;
            }
        }

        // CopyRects with depth stencil format test
        void testCopyRectsDepthStencilFormat() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface8> sourceSurface;
            Com<IDirect3DSurface8> destinationSurface;

            m_device->CreateDepthStencilSurface(256, 256, D3DFMT_D16, D3DMULTISAMPLE_NONE, &sourceSurface);
            m_device->CreateDepthStencilSurface(256, 256, D3DFMT_D16, D3DMULTISAMPLE_NONE, &destinationSurface);

            m_totalTests++;
            // CopyRects does not handle surfaces with depth stencil formats, so this should fail
            HRESULT status = m_device->CopyRects(sourceSurface.ptr(), NULL, 0, destinationSurface.ptr(), NULL);
            if (FAILED(status)) {
                m_passedTests++;
                std::cout << "  + The CopyRects with depth stencil test has passed" << std::endl;
            } else {
                std::cout << "  - The CopyRects with depth stencil test has failed" << std::endl;
            }
        }

        // CopyRects with different surface formats test
        void testCopyRectsWithDifferentSurfaceFormats() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface8> srcSurface;
            Com<IDirect3DSurface8> dstSurface;

            m_totalTests++;
            // create a source render target with the D3DFMT_X8R8G8B8 format
            HRESULT statusCRT = m_device->CreateRenderTarget(256, 256, D3DFMT_X8R8G8B8,
                                                             D3DMULTISAMPLE_NONE, TRUE, &srcSurface);
            // create a target image surface with the D3DFMT_R8G8B8 format
            HRESULT statusCIS = m_device->CreateImageSurface(256, 256, D3DFMT_R8G8B8, &dstSurface);

            if (SUCCEEDED(statusCRT) && SUCCEEDED(statusCIS)) {
                // CopyRects will not support format conversions, so this call should fail
                HRESULT status = m_device->CopyRects(srcSurface.ptr(), nullptr, 0, dstSurface.ptr(), nullptr);

                if (FAILED(status)) {
                    m_passedTests++;
                    std::cout << "  + The CopyRects with different surface formats test has passed" << std::endl;
                } else {
                    std::cout << "  - The CopyRects with different surface formats test has failed" << std::endl;
                }
            } else {
                std::cout << "  ~ The CopyRects with different surface formats test did not run" << std::endl;
            }
        }

        // VCache query result test
        void testVCacheQueryResult() {
            resetOrRecreateDevice();

            D3DDEVINFO_VCACHE vCache;

            m_totalTests++;
            // shouldn't fail on any vendor (native AMD/Intel return S_FALSE, native Nvidia returns D3D_OK)
            HRESULT status = m_device->GetInfo(D3DDEVINFOID_VCACHE, &vCache, sizeof(D3DDEVINFO_VCACHE));
            if (FAILED(status)) {
                std::cout << "  - The VCache query response test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The VCache query response test has passed" << std::endl;
            }
        }

        // SetIndices with UINT BaseVertexIndex test
        void testSetIndicesWithUINTBVI() {
            resetOrRecreateDevice();

            Com<IDirect3DIndexBuffer8> ib;
            UINT baseVertexIndex = 0;

            // Create the index buffer with above INT_MAX length
            m_device->CreateIndexBuffer((UINT) INT_MAX + 2, 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ib);

            m_totalTests++;
            // Use a INT_MAX + 1 BaseVertexIndex
            HRESULT status = m_device->SetIndices(ib.ptr(), (UINT) INT_MAX + 1);
            if (FAILED(status)) {
                std::cout << "  - The SetIndices with UINT BaseVertexIndex test has failed" << std::endl;
            } else {
                ib = nullptr;
                // GetIndices should return the previously set value for BaseVertexIndex
                m_device->GetIndices(&ib, &baseVertexIndex);
                //std::cout << format("  * BaseVertexIndex is: ", baseVertexIndex) << std::endl;

                if (baseVertexIndex == (UINT) INT_MAX + 1) {
                    m_passedTests++;
                    std::cout << "  + The SetIndices with UINT BaseVertexIndex test has passed" << std::endl;
                } else {
                    std::cout << "  - The SetIndices with UINT BaseVertexIndex test has failed" << std::endl;
                }
            }
        }

        // RenderState with D3DRS_ZVISIBLE test
        void testRenderStateZVisible() {
            resetOrRecreateDevice();

            DWORD pValue = 0;

            m_totalTests++;
            // the state isn't supported, so allowed values aren't documented, but let's use TRUE
            HRESULT setStatus = m_device->SetRenderState(D3DRS_ZVISIBLE, TRUE);
            HRESULT getStatus = m_device->GetRenderState(D3DRS_ZVISIBLE, &pValue);
            //std::cout << format("  * D3DRS_ZVISIBLE is: ", pValue) << std::endl;

            // although the state isn't supported, the above calls should return D3D_OK
            if (SUCCEEDED(setStatus) && SUCCEEDED(getStatus)) {
                m_passedTests++;
                std::cout << "  + The RenderState with D3DRS_ZVISIBLE test has passed" << std::endl;
            } else {
                std::cout << "  - The RenderState with D3DRS_ZVISIBLE test has failed" << std::endl;
            }
        }

        // Invalid (outside of the render target) viewports test
        void testInvalidViewports() {
            resetOrRecreateDevice();

            DWORD offset = 100;
            D3DVIEWPORT8 vp1 = {0, 0, m_pp.BackBufferWidth + offset,
                                m_pp.BackBufferHeight + offset, 0.0, 1.0};
            D3DVIEWPORT8 vp2 = {offset * 2, offset * 2, m_pp.BackBufferWidth - offset / 2,
                                m_pp.BackBufferHeight - offset / 2, 0.0, 1.0};

            // not really needed, more of a d3d8to9 quirk to trigger viewport validation
            Com<IDirect3DSurface8> rt;
            m_device->GetRenderTarget(&rt);

            m_totalTests++;
            // according to D3D8 docs, this call should fail "if pViewport describes
            // a region that cannot exist within the render target surface"
            HRESULT statusVP1 = m_device->SetViewport(&vp1);
            // a viewport with a too great X or Y offset should be rejected even
            // if its dimensions are technically smaller than the render target surface
            HRESULT statusVP2 = m_device->SetViewport(&vp2);

            if (FAILED(statusVP1) && FAILED(statusVP2)) {
                m_passedTests++;
                std::cout << "  + The invalid viewports test has passed" << std::endl;
            } else {
                std::cout << "  - The invalid viewports test has failed" << std::endl;
            }
        }

        // Viewport adjustment with smaller render target test
        void testViewportAdjustmentWithSmallerRT() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface8> surface;
            UINT rtWidth  = 256;
            UINT rtHeight = 256;

            m_totalTests++;
            // set a fullscreen viewport
            D3DVIEWPORT8 vp = {0, 0, m_pp.BackBufferWidth, m_pp.BackBufferHeight, 0.0, 1.0};
            m_device->SetViewport(&vp);
            // create a smaller render target
            m_device->CreateRenderTarget(rtWidth, rtHeight, m_pp.BackBufferFormat,
                                         D3DMULTISAMPLE_NONE, TRUE, &surface);
            // set the newly created render target
            HRESULT statusRT = m_device->SetRenderTarget(surface.ptr(), NULL);

            D3DVIEWPORT8 nvp;
            // the viewport should automatically get adjusted to the dimensions
            // of the new render target, without any subsequent calls to SetViewport
            HRESULT statusGV = m_device->GetViewport(&nvp);
            //std::cout << format("  * Viewport width is: ", nvp.Width) << std::endl;
            //std::cout << format("  * Viewport height is: ", nvp.Height) << std::endl;

            if (SUCCEEDED(statusRT) && SUCCEEDED(statusGV) &&
                rtWidth == nvp.Width && rtHeight == nvp.Height) {
                m_passedTests++;
                std::cout << "  + The viewport adjustment with smaller RT test has passed" << std::endl;
            } else {
                std::cout << "  - The viewport adjustment with smaller RT test has failed" << std::endl;
            }
        }

        // GetDepthStencilSurface without EnableAutoDepthStencil test
        void testGetRenderTargetWithoutEADS() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface8> surface;

            m_totalTests++;
            // since the devices has been created without EnableAutoDepthStencil
            // set to True, the following call to GetDepthStencilSurface should fail
            HRESULT status = m_device->GetDepthStencilSurface(&surface);

            if (FAILED(status)) {
                m_passedTests++;
                std::cout << "  + The GetDepthStencilSurface without EnableAutoDepthStencil test has passed" << std::endl;
            } else {
                std::cout << "  - The GetDepthStencilSurface without EnableAutoDepthStencil test has failed" << std::endl;
            }
        }

        // D3DRS_POINTSIZE_MIN default value test
        void testPointSizeMinRSDefaultValue() {
            resetOrRecreateDevice();

            DWORD pointSizeMin = 0;

            m_totalTests++;
            HRESULT status = m_device->GetRenderState(D3DRS_POINTSIZE_MIN, &pointSizeMin);

            // the default value of D3DRS_POINTSIZE_MIN is 0.0 in D3D8, as opposed to 1.0 in D3D9
            if (SUCCEEDED(status) && pointSizeMin == static_cast<DWORD>(0.0)) {
                m_passedTests++;
                std::cout << "  + The D3DRS_POINTSIZE_MIN default value test has passed" << std::endl;
            } else {
                std::cout << "  - The D3DRS_POINTSIZE_MIN default value test has failed" << std::endl;
            }
        }

        // D3DFMT_UNKNOWN object creation test
        void testUnknownFormatObjectCreation() {
            resetOrRecreateDevice();

            IDirect3DTexture8* texture = reinterpret_cast<IDirect3DTexture8*>(0xABCDABCD);
            IDirect3DVolumeTexture8* volumeTexture = reinterpret_cast<IDirect3DVolumeTexture8*>(0xABCDABCD);
            IDirect3DCubeTexture8* cubeTexture = reinterpret_cast<IDirect3DCubeTexture8*>(0xABCDABCD);
            IDirect3DSurface8* renderTarget = reinterpret_cast<IDirect3DSurface8*>(0xABCDABCD);
            IDirect3DSurface8* depthStencil = reinterpret_cast<IDirect3DSurface8*>(0xABCDABCD);
            IDirect3DSurface8* imageSurface = reinterpret_cast<IDirect3DSurface8*>(0xABCDABCD);

            m_totalTests++;

            HRESULT statusTexture = m_device->CreateTexture(256, 256, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, &texture);
            HRESULT statusVolumeTexture = m_device->CreateVolumeTexture(256, 256, 256, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, &volumeTexture);
            HRESULT statusCubeTexture = m_device->CreateCubeTexture(256, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, &cubeTexture);
            HRESULT statusRenderTarget = m_device->CreateRenderTarget(256, 256, D3DFMT_UNKNOWN, D3DMULTISAMPLE_NONE, TRUE, &renderTarget);
            HRESULT statusDepthStencil = m_device->CreateDepthStencilSurface(256, 256, D3DFMT_UNKNOWN, D3DMULTISAMPLE_NONE, &depthStencil);
            HRESULT statusImageSurface = m_device->CreateImageSurface(256, 256, D3DFMT_UNKNOWN, &imageSurface);

            if (FAILED(statusTexture)       && !PTRCLEARED(texture)
             && FAILED(statusVolumeTexture) && !PTRCLEARED(volumeTexture)
             && FAILED(statusCubeTexture)   && !PTRCLEARED(cubeTexture)
             && FAILED(statusRenderTarget)  && !PTRCLEARED(renderTarget)
             && FAILED(statusDepthStencil)  && !PTRCLEARED(depthStencil)
             // CreateImageSurface clears the content of imageSurface before checking for D3DFMT_UNKNOWN
             && FAILED(statusImageSurface)  &&  PTRCLEARED(imageSurface)) {
                m_passedTests++;
                std::cout << "  + The D3DFMT_UNKNOWN object creation test has passed" << std::endl;
            } else {
                std::cout << "  - The D3DFMT_UNKNOWN object creation test has failed" << std::endl;
            }
        }

        // Create and use a device with a NULL HWND
        void testDeviceWithoutHWND() {
            // use a separate device for this test
            Com<IDirect3DDevice8> device;
            Com<IDirect3DSurface8> surface;
            Com<IDirect3DVertexBuffer8> vertexBuffer;

            void* vertices;
            m_device->CreateVertexBuffer(m_rgbVerticesSize, 0, RGBT_FVF_CODES,
                                         D3DPOOL_DEFAULT, &vertexBuffer);
            vertexBuffer->Lock(0, m_rgbVerticesSize, reinterpret_cast<BYTE**>(&vertices), 0);
            memcpy(vertices, m_rgbVertices.data(), m_rgbVerticesSize);
            vertexBuffer->Unlock();

            D3DPRESENT_PARAMETERS presentParams;
            ZeroMemory(&presentParams, sizeof(presentParams));

            presentParams.Windowed = TRUE;
            presentParams.hDeviceWindow = NULL;
            // set to D3DSWAPEFFECT_COPY or D3DSWAPEFFECT_FLIP for no VSync
            presentParams.SwapEffect = D3DSWAPEFFECT_COPY;
            // according to D3D9 spec "0 is treated as 1" here
            presentParams.BackBufferCount = 0;
            presentParams.BackBufferWidth = WINDOW_WIDTH;
            presentParams.BackBufferHeight = WINDOW_HEIGHT;
            presentParams.BackBufferFormat = m_pp.BackBufferFormat;

            DWORD behaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;

            HRESULT status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
                                                 behaviorFlags, &presentParams, &device);

            if(FAILED(status)) {
                std::cout << "  ~ The device does not support creation with a NULL HWND" << std::endl;
            } else {
                m_totalTests++;

                HRESULT statusBB      = device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &surface);

                /*D3DSURFACE_DESC desc;
                surface->GetDesc(&desc);
                std::cout << format("  ~ Back buffer width: ",  desc.Width)  << std::endl;
                std::cout << format("  ~ Back buffer height: ", desc.Height) << std::endl;*/

                device->BeginScene();
                device->SetStreamSource(0, vertexBuffer.ptr(), sizeof(RGBVERTEX));
                device->SetVertexShader(RGBT_FVF_CODES);
                HRESULT statusDraw    = device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                device->EndScene();
                HRESULT statusPresent = device->Present(NULL, NULL, NULL, NULL);

                if (FAILED(statusBB) || FAILED(statusDraw) || FAILED(statusPresent)) {
                    std::cout << "  - The device with NULL HWND test has failed" << std::endl;
                } else {
                    m_passedTests++;
                    std::cout << "  + The device with NULL HWND test has passed" << std::endl;
                }
            }
        }

        void testCheckDeviceFormatWithBuffers() {
            resetOrRecreateDevice();

            m_totalTests++;

            HRESULT resVB = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_VERTEXBUFFER, D3DFMT_VERTEXDATA);
            HRESULT resIB = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_INDEXBUFFER, D3DFMT_INDEX16);

            HRESULT resVB2 = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_VERTEXBUFFER, m_pp.BackBufferFormat);
            HRESULT resIB2 = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_INDEXBUFFER, m_pp.BackBufferFormat);

            if (resVB  != D3DERR_INVALIDCALL ||
                resIB  != D3DERR_INVALIDCALL ||
                resVB2 != D3DERR_INVALIDCALL ||
                resIB2 != D3DERR_INVALIDCALL) {
                std::cout << "  - The CheckDeviceFormat with index/vertex buffers test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The CheckDeviceFormat with index/vertex buffers test has passed" << std::endl;
            }
        }

        // Clear with unbound depth stencil test
        void testClearWithUnboundDepthStencil() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface8> renderTarget;

            m_device->CreateRenderTarget(256, 256, m_pp.BackBufferFormat, D3DMULTISAMPLE_NONE, TRUE, &renderTarget);
            HRESULT status = m_device->SetRenderTarget(renderTarget.ptr(), NULL);

            if (SUCCEEDED(status)) {
                m_totalTests++;

                // D3DCLEAR_ZBUFFER or D3DCLEAR_STENCIL will fail if a depth stencil isn't bound
                HRESULT clearZStatus = m_device->Clear(0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0f, 0);
                HRESULT clearSStatus = m_device->Clear(0, NULL, D3DCLEAR_STENCIL, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0f, 0);

                if (FAILED(clearZStatus) && FAILED(clearSStatus)) {
                    m_passedTests++;
                    std::cout << "  + The D3DCLEAR_ZBUFFER with bound depth stencil test has passed" << std::endl;
                } else {
                    std::cout << "  - The D3DCLEAR_ZBUFFER with bound depth stencil test has failed" << std::endl;
                }
            } else {
                std::cout << "  ~ The D3DCLEAR_ZBUFFER with bound depth stencil test did not run" << std::endl;
            }
        }

        // Rect/Box clearing behavior on lock test
        void testRectBoxClearingOnLock() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface8> surface;
            Com<IDirect3DTexture8> texture;
            Com<IDirect3DTexture8> sysmemTexture;
            Com<IDirect3DCubeTexture8> cubeTexture;
            Com<IDirect3DVolumeTexture8> volumeTexture;

            D3DLOCKED_RECT surfaceRect;
            D3DLOCKED_RECT textureRect;
            D3DLOCKED_RECT sysmemTextureRect;
            D3DLOCKED_RECT cubeTextureRect;
            D3DLOCKED_BOX volumeTextureBox;

            m_totalTests++;

            m_device->CreateImageSurface(256, 256, D3DFMT_X8R8G8B8, &surface);
            m_device->CreateTexture(256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &texture);
            m_device->CreateTexture(256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sysmemTexture);
            m_device->CreateVolumeTexture(256, 256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &volumeTexture);
            m_device->CreateCubeTexture(256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &cubeTexture);

            // the second lock will fail, but the LockRect contents
            // should be cleared for everything outside of D3DPOOL_DEFAULT,
            // except for surface types equal to D3DRTYPE_TEXTURE.
            // LockBox clears the content universally.
            surface->LockRect(&surfaceRect, NULL, 0);
            surfaceRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            surfaceRect.Pitch = 10;
            HRESULT surfaceStatus = surface->LockRect(&surfaceRect, NULL, 0);
            surface->UnlockRect();
            texture->LockRect(0, &textureRect, NULL, 0);
            textureRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            textureRect.Pitch = 10;
            HRESULT textureStatus = texture->LockRect(0, &textureRect, NULL, 0);
            texture->UnlockRect(0);
            sysmemTexture->LockRect(0, &sysmemTextureRect, NULL, 0);
            sysmemTextureRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            sysmemTextureRect.Pitch = 10;
            HRESULT sysmemTextureStatus = sysmemTexture->LockRect(0, &sysmemTextureRect, NULL, 0);
            sysmemTexture->UnlockRect(0);
            cubeTexture->LockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0, &cubeTextureRect, NULL, 0);
            cubeTextureRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            cubeTextureRect.Pitch = 10;
            HRESULT cubeTextureStatus = cubeTexture->LockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0, &cubeTextureRect, NULL, 0);
            cubeTexture->UnlockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0);
            volumeTexture->LockBox(0, &volumeTextureBox, NULL, 0);
            volumeTextureBox.pBits = reinterpret_cast<void*>(0xABCDABCD);
            volumeTextureBox.RowPitch = 10;
            volumeTextureBox.SlicePitch = 10;
            HRESULT volumeTextureStatus = volumeTexture->LockBox(0, &volumeTextureBox, NULL, 0);
            volumeTexture->UnlockBox(0);

            //std::cout << format("  * surfaceRect pBits: ", surfaceRect.pBits, " Pitch: ", surfaceRect.Pitch) << std::endl;
            //std::cout << format("  * textureRect pBits: ", textureRect.pBits, " Pitch: ", textureRect.Pitch) << std::endl;
            //std::cout << format("  * sysmemTextureRect pBits: ", sysmemTextureRect.pBits, " Pitch: ", sysmemTextureRect.Pitch) << std::endl;
            //std::cout << format("  * cubeTextureRect pBits: ", cubeTextureRect.pBits, " Pitch: ", cubeTextureRect.Pitch) << std::endl;
            //std::cout << format("  * volumeTextureBox pBits: ", volumeTextureBox.pBits, " RowPitch: ", volumeTextureBox.RowPitch,
            //                    " SlicePitch: ", volumeTextureBox.SlicePitch) << std::endl;

            if (SUCCEEDED(surfaceStatus) ||
                SUCCEEDED(textureStatus) ||
                SUCCEEDED(sysmemTextureStatus) ||
                SUCCEEDED(cubeTextureStatus) ||
                SUCCEEDED(volumeTextureStatus) ||
                surfaceRect.pBits != nullptr ||
                surfaceRect.Pitch != 0 ||
                textureRect.pBits != nullptr ||
                textureRect.Pitch != 0 ||
                sysmemTextureRect.pBits != reinterpret_cast<void*>(0xABCDABCD) ||
                sysmemTextureRect.Pitch != 10 ||
                cubeTextureRect.pBits != nullptr ||
                cubeTextureRect.Pitch != 0 ||
                volumeTextureBox.pBits != nullptr ||
                volumeTextureBox.RowPitch != 0 ||
                volumeTextureBox.SlicePitch != 0) {
                std::cout << "  - The Rect/Box clearing on lock test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The Rect/Box clearing on lock test has passed" << std::endl;
            }
        }

        // Various CheckDeviceMultiSampleType validation tests
        void testCheckDeviceMultiSampleTypeValidation() {
            resetOrRecreateDevice();

            constexpr D3DFORMAT D3DFMT_NULL = (D3DFORMAT) MAKEFOURCC('N', 'U', 'L', 'L');

            m_totalTests++;

            // The call will fail with anything above D3DMULTISAMPLE_16_SAMPLES
            HRESULT statusSample = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, FALSE,
                                                                     (D3DMULTISAMPLE_TYPE) ((UINT) D3DMULTISAMPLE_16_SAMPLES * 2));

            // The call will fail with D3DFMT_UNKNOWN
            HRESULT statusUnknown = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_UNKNOWN,
                                                                      FALSE, D3DMULTISAMPLE_NONE);

            // The call will pass with D3DFMT_NULL
            HRESULT statusNull = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_NULL,
                                                                   FALSE, D3DMULTISAMPLE_NONE);

            // The call will faill with D3DFMT_NULL above D3DMULTISAMPLE_16_SAMPLES
            HRESULT statusNullSample = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_NULL, FALSE,
                                                                         (D3DMULTISAMPLE_TYPE) ((UINT) D3DMULTISAMPLE_16_SAMPLES * 2));

            if (FAILED(statusSample) && FAILED(statusUnknown) && SUCCEEDED(statusNull) && FAILED(statusNullSample)) {
                m_passedTests++;
                std::cout << "  + The CheckDeviceMultiSampleType validation test has passed" << std::endl;
            } else {
                std::cout << "  - The CheckDeviceMultiSampleType validation test has failed" << std::endl;
            }
        }

        // D3D Device capabilities tests
        void testDeviceCapabilities() {
            createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);

            D3DCAPS8 caps8SWVP;
            D3DCAPS8 caps8;

            m_device->GetDeviceCaps(&caps8SWVP);
            m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps8);

            std::cout << std::endl << "Running device capabilities tests:" << std::endl;

            m_totalTests++;
            // Some D3D8 UE2.x games only enable character shadows if this capability is supported
            if (caps8.RasterCaps & D3DPRASTERCAPS_ZBIAS) {
                std::cout << "  + The D3DPRASTERCAPS_ZBIAS test has passed" << std::endl;
                m_passedTests++;
            } else {
                std::cout << "  - The D3DPRASTERCAPS_ZBIAS test has failed" << std::endl;
            }

            m_totalTests++;
            // MaxVertexBlendMatrixIndex is typically 0 when queried from the D3D8 interface
            // and 255 when queried from the device in SWVP mode (neither should go above 255)
            if (caps8.MaxVertexBlendMatrixIndex < 256u && caps8SWVP.MaxVertexBlendMatrixIndex < 256u) {
                std::cout << format("  + The MaxVertexBlendMatrixIndex INTF and SWVP test has passed (", caps8.MaxVertexBlendMatrixIndex,
                                    ", ", caps8SWVP.MaxVertexBlendMatrixIndex, ")") << std::endl;
                m_passedTests++;
            } else {
                std::cout << format("  - The MaxVertexBlendMatrixIndex INTF and SWVP test has failed (", caps8.MaxVertexBlendMatrixIndex,
                                    ", ", caps8SWVP.MaxVertexBlendMatrixIndex, ")") << std::endl;
            }

            m_totalTests++;
            // VS 1.1 is the latest version supported in D3D8
            UINT majorVSVersion = static_cast<UINT>((caps8.VertexShaderVersion & 0x0000FF00) >> 8);
            UINT minorVSVersion = static_cast<UINT>(caps8.VertexShaderVersion & 0x000000FF);
            if (majorVSVersion <= 1u && minorVSVersion <= 1u) {
                std::cout << format("  + The VertexShaderVersion test has passed (", majorVSVersion, ".", minorVSVersion, ")") << std::endl;
                m_passedTests++;
            } else {
                std::cout << format("  - The VertexShaderVersion test has failed (", majorVSVersion, ".", minorVSVersion, ")") << std::endl;
            }

            m_totalTests++;
            // typically 256 and should not go above that in D3D8
            if (static_cast<UINT>(caps8.MaxVertexShaderConst) <= 256u) {
                std::cout << format("  + The MaxVertexShaderConst test has passed (", caps8.MaxVertexShaderConst, ")") << std::endl;
                m_passedTests++;
            } else {
                std::cout << format("  - The MaxVertexShaderConst test has failed (", caps8.MaxVertexShaderConst, ")") << std::endl;
            }

            m_totalTests++;
            // PS 1.4 is the latest version supported in D3D8
            UINT majorPSVersion = static_cast<UINT>((caps8.PixelShaderVersion & 0x0000FF00) >> 8);
            UINT minorPSVersion = static_cast<UINT>(caps8.PixelShaderVersion & 0x000000FF);
            if (majorPSVersion <= 1u && minorPSVersion <= 4u) {
                std::cout << format("  + The PixelShaderVersion test has passed (", majorPSVersion, ".", minorPSVersion, ")") << std::endl;
                m_passedTests++;
            } else {
                std::cout << format("  - The PixelShaderVersion test has failed (", majorPSVersion, ".", minorPSVersion, ")") << std::endl;
            }
        }

        // CheckDeviceMultiSampleType formats test
        void testCheckDeviceMultiSampleTypeFormats() {
            resetOrRecreateDevice();

            std::map<D3DFORMAT, char const*> sfFormats = { {D3DFMT_D16_LOCKABLE, "D3DFMT_D16_LOCKABLE"},
                                                           {D3DFMT_DXT1, "D3DFMT_DXT1"},
                                                           {D3DFMT_DXT2, "D3DFMT_DXT2"},
                                                           {D3DFMT_DXT3, "D3DFMT_DXT3"},
                                                           {D3DFMT_DXT4, "D3DFMT_DXT4"},
                                                           {D3DFMT_DXT5, "D3DFMT_DXT5"} };

            std::map<D3DFORMAT, char const*>::iterator sfFormatIter;

            std::cout << std::endl << "Running CheckDeviceMultiSampleType formats tests:" << std::endl;

            for (sfFormatIter = sfFormats.begin(); sfFormatIter != sfFormats.end(); ++sfFormatIter) {
                D3DFORMAT surfaceFormat = sfFormatIter->first;

                m_totalTests++;

                // None of the above textures are supported with anything beside D3DMULTISAMPLE_NONE
                HRESULT status = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, surfaceFormat,
                                                                   FALSE, D3DMULTISAMPLE_2_SAMPLES);

                if (SUCCEEDED(status)) {
                    std::cout << format("  - The ", sfFormatIter->second , " format test has failed") << std::endl;
                } else {
                    m_passedTests++;
                    std::cout << format("  + The ", sfFormatIter->second ," format test has passed") << std::endl;
                }
            }
        }

        void printTestResults() {
            std::cout << std::endl << format("Passed ", m_passedTests, "/", m_totalTests, " tests") << std::endl;
        }

        void prepare() {
            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);

            // don't need any of these for 2D rendering
            HRESULT status = m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D8 render state for D3DRS_ZENABLE");
            status = m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            if (FAILED(status))
                throw Error("Failed to set D3D8 render state for D3DRS_CULLMODE");
            status = m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D8 render state for D3DRS_LIGHTING");

            // Vertex Buffer
            void* vertices = nullptr;

            status = m_device->CreateVertexBuffer(m_rgbVerticesSize, 0, RGBT_FVF_CODES,
                                                  D3DPOOL_DEFAULT, &m_vb);
            if (FAILED(status))
                throw Error("Failed to create D3D8 vertex buffer");

            status = m_vb->Lock(0, m_rgbVerticesSize, reinterpret_cast<BYTE**>(&vertices), 0);
            if (FAILED(status))
                throw Error("Failed to lock D3D8 vertex buffer");
            memcpy(vertices, m_rgbVertices.data(), m_rgbVerticesSize);
            status = m_vb->Unlock();
            if (FAILED(status))
                throw Error("Failed to unlock D3D8 vertex buffer");
        }

        void render() {
            HRESULT status = m_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            if (FAILED(status))
                throw Error("Failed to clear D3D8 viewport");
            if (SUCCEEDED(m_device->BeginScene())) {
                status = m_device->SetStreamSource(0, m_vb.ptr(), sizeof(RGBVERTEX));
                if (FAILED(status))
                    throw Error("Failed to set D3D8 stream source");
                status = m_device->SetVertexShader(RGBT_FVF_CODES);
                if (FAILED(status))
                    throw Error("Failed to set D3D8 vertex shader");
                status = m_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                if (FAILED(status))
                    throw Error("Failed to draw D3D8 triangle list");
                if (SUCCEEDED(m_device->EndScene())) {
                    status = m_device->Present(NULL, NULL, NULL, NULL);
                    if (FAILED(status))
                        throw Error("Failed to present");
                } else {
                    throw Error("Failed to end D3D8 scene");
                }
            } else {
                throw Error("Failed to begin D3D8 scene");
            }
        }

    private:

        HRESULT createDeviceWithFlags(D3DPRESENT_PARAMETERS* presentParams,
                                      DWORD behaviorFlags,
                                      D3DDEVTYPE deviceType,
                                      bool throwErrorOnFail) {
            if (m_d3d == nullptr)
                throw Error("The D3D8 interface hasn't been initialized");

            m_device = nullptr;

            HRESULT status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, deviceType, m_hWnd,
                                                 behaviorFlags, presentParams, &m_device);
            if (throwErrorOnFail && FAILED(status))
                throw Error("Failed to create D3D8 device");

            return status;
        }

        HRESULT resetOrRecreateDevice() {
            if (m_d3d == nullptr)
                throw Error("The D3D8 interface hasn't been initialized");

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
                throw Error("Failed to create D3D8 device");

            return status;
        }

        HWND                          m_hWnd;

        DWORD                         m_vendorID;

        Com<IDirect3D8>               m_d3d;
        Com<IDirect3DDevice8>         m_device;
        Com<IDirect3DVertexBuffer8>   m_vb;

        D3DPRESENT_PARAMETERS         m_pp;

        // tailored for 1024x768 and the appearance of being centered
        std::array<RGBVERTEX, 3>      m_rgbVertices = {{ { 60.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0),},
                                                         {350.0f,  45.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 255, 0),},
                                                         {640.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255),} }};
        const size_t                  m_rgbVerticesSize = m_rgbVertices.size() * sizeof(RGBVERTEX);

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
                     "D3D8_Triangle", NULL};
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow("D3D8_Triangle", "D3D8 Triangle - Blisto Retro Testing Edition",
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
        rgbTriangle.listSurfaceFormats(0);
        rgbTriangle.listSurfaceFormats(D3DUSAGE_RENDERTARGET);
        rgbTriangle.listSurfaceFormats(D3DUSAGE_DEPTHSTENCIL);
        rgbTriangle.listMultisampleSupport(FALSE);
        rgbTriangle.listMultisampleSupport(TRUE);
        rgbTriangle.listVendorFormatHacksSupport();
        rgbTriangle.listDeviceCapabilities();
        rgbTriangle.listDeviceD3D9Capabilities();
        rgbTriangle.listVCacheQueryResult();
        rgbTriangle.listAvailableTextureMemory();

        // run D3D Device tests
        rgbTriangle.startTests();
        rgbTriangle.testZeroBackBufferCount();
        rgbTriangle.testRSValues();
        rgbTriangle.testInvalidPresentationInterval();
        rgbTriangle.testBeginSceneReset();
        rgbTriangle.testPureDeviceSetSWVPRenderState();
        rgbTriangle.testPureDeviceOnlyWithHWVP();
        rgbTriangle.testClipStatus();
        rgbTriangle.testDeviceTypes();
        rgbTriangle.testGetDeviceCapsWithDeviceTypes();
        rgbTriangle.testDefaultPoolAllocationReset();
        rgbTriangle.testCreateVertexShaderHandleGeneration();
        rgbTriangle.testCreateStateBlockAndReset();
        rgbTriangle.testCreateStateBlockMonotonicTokens(100);
        rgbTriangle.testBeginStateBlockCalls();
        rgbTriangle.testMultiplyTransformRecordingAndCapture();
        // native drivers don't appear to validate tokens at
        // all, and will straight-up crash in these situations
        //rgbTriangle.testStateBlockWithInvalidToken();
        rgbTriangle.testCopyRectsDepthStencilFormat();
        rgbTriangle.testCopyRectsWithDifferentSurfaceFormats();
        rgbTriangle.testVCacheQueryResult();
        // tests against the underflow of BaseVertexIndex,
        // but has to allocate a ~2GB index buffer to do so,
        // which is very slow, hence disabling by default
        //rgbTriangle.testSetIndicesWithUINTBVI();
        rgbTriangle.testRenderStateZVisible();
        rgbTriangle.testInvalidViewports();
        rgbTriangle.testViewportAdjustmentWithSmallerRT();
        rgbTriangle.testGetRenderTargetWithoutEADS();
        rgbTriangle.testPointSizeMinRSDefaultValue();
        rgbTriangle.testUnknownFormatObjectCreation();
        rgbTriangle.testDeviceWithoutHWND();
        rgbTriangle.testCheckDeviceFormatWithBuffers();
        rgbTriangle.testClearWithUnboundDepthStencil();
        // outright crashes on certain native drivers/hardware
        //rgbTriangle.testRectBoxClearingOnLock();
        rgbTriangle.testCheckDeviceMultiSampleTypeValidation();
        rgbTriangle.testDeviceCapabilities();
        rgbTriangle.testCheckDeviceMultiSampleTypeFormats();
        rgbTriangle.printTestResults();

        // D3D8 triangle
        rgbTriangle.prepare();

        ShowWindow(hWnd, SW_SHOWDEFAULT);
        UpdateWindow(hWnd);

        while (true) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    UnregisterClass("D3D8_Triangle", wc.hInstance);
                    return msg.wParam;
                }
            } else {
                rgbTriangle.render();
            }
        }

    } catch (const Error& e) {
        std::cerr << e.message() << std::endl;
        UnregisterClass("D3D8_Triangle", wc.hInstance);
        return msg.wParam;
    }
}
