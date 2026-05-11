#include "display.h"

#include "Engine/Texture2D.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/UnrealMemory.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "PixelFormat.h"

#include "cartridge.h"

#include <string>

namespace
{
    // NTSC NES master clock yields ~60.0988 fields/sec. Used to convert a
    // wall-clock DeltaTime into emulated frames.
    constexpr double kNesNtscFps = 60.0988;

    // Maximum number of frames we are willing to run inside a single Tick to
    // avoid the spiral-of-death when the game thread stalls.
    constexpr int32 kMaxFramesPerTick = 4;
}

UDisplayComponent::UDisplayComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;

    // The NES system itself is allocated lazily so the constructor stays
    // cheap (CDO construction must not touch the file system).
}

void UDisplayComponent::BeginPlay()
{
    Super::BeginPlay();

    NesBus = MakeUnique<Bus>();
    EnsureTextureCreated();

    if (!AutoLoadRomPath.IsEmpty())
    {
        LoadCartridge(AutoLoadRomPath);
    }
}

void UDisplayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    NesBus.Reset();
    ScreenTexture = nullptr;
    Super::EndPlay(EndPlayReason);
}

void UDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!NesBus.IsValid() || !IsCartridgeLoaded())
    {
        return;
    }

    // Decide how many emulated frames to run this Tick. With EmulationSpeed=1
    // and a 60Hz Tick this resolves to one frame per Tick on average.
    FrameAccumulator += static_cast<double>(DeltaTime) * EmulationSpeed * kNesNtscFps;
    int32 FramesToRun = FMath::Clamp(static_cast<int32>(FrameAccumulator), 0, kMaxFramesPerTick);
    FrameAccumulator -= FramesToRun;

    for (int32 i = 0; i < FramesToRun; ++i)
    {
        // Mirror the live controller state into the bus before running the
        // frame — the game can latch the controller at any point during it.
        NesBus->controller[0] = LiveController[0];
        NesBus->controller[1] = LiveController[1];

        RunOneFrame();
    }

    if (FramesToRun > 0)
    {
        UploadFrameToTexture();
    }
}

// -----------------------------------------------------------------------------
// ROM management
// -----------------------------------------------------------------------------

bool UDisplayComponent::LoadCartridge(const FString& RomFilePath)
{
    if (!NesBus.IsValid())
    {
        // Allow loading a ROM before BeginPlay (e.g. from the editor) by
        // creating the bus on demand.
        NesBus = MakeUnique<Bus>();
    }

    const FString AbsolutePath = ResolveRomPath(RomFilePath);

    if (!FPaths::FileExists(AbsolutePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[NES] ROM not found: %s"), *AbsolutePath);
        return false;
    }

    const std::string StdPath(TCHAR_TO_UTF8(*AbsolutePath));
    auto Cart = std::make_shared<Cartridge>(StdPath);
    if (!Cart->ImageValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[NES] Cartridge image invalid: %s"), *AbsolutePath);
        return false;
    }

    NesBus->InsertCartridge(Cart);
    NesBus->reset();
    FrameAccumulator = 0.0;
    UE_LOG(LogTemp, Log, TEXT("[NES] Loaded ROM: %s"), *AbsolutePath);
    return true;
}

void UDisplayComponent::ResetEmulator()
{
    if (NesBus.IsValid())
    {
        NesBus->reset();
        FrameAccumulator = 0.0;
    }
}

bool UDisplayComponent::IsCartridgeLoaded() const
{
    return NesBus.IsValid()
        && NesBus->cart
        && NesBus->cart->ImageValid();
}

// -----------------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------------

void UDisplayComponent::SetControllerButton(int32 Player, ENesButton Button, bool bPressed)
{
    if (Player < 0 || Player > 1) return;

    const uint8 Mask = ButtonMask(Button);
    if (bPressed)
    {
        LiveController[Player] |= Mask;
    }
    else
    {
        LiveController[Player] &= ~Mask;
    }
}

void UDisplayComponent::SetControllerStateRaw(int32 Player, uint8 State)
{
    if (Player < 0 || Player > 1) return;
    LiveController[Player] = State;
}

void UDisplayComponent::ClearControllers()
{
    LiveController[0] = 0;
    LiveController[1] = 0;
}

uint8 UDisplayComponent::ButtonMask(ENesButton Button)
{
    // NES controller bit ordering, MSB first.
    switch (Button)
    {
    case ENesButton::A:      return 0x80;
    case ENesButton::B:      return 0x40;
    case ENesButton::Select: return 0x20;
    case ENesButton::Start:  return 0x10;
    case ENesButton::Up:     return 0x08;
    case ENesButton::Down:   return 0x04;
    case ENesButton::Left:   return 0x02;
    case ENesButton::Right:  return 0x01;
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Internals
// -----------------------------------------------------------------------------

FString UDisplayComponent::ResolveRomPath(const FString& InPath)
{
    if (FPaths::IsRelative(InPath))
    {
        return FPaths::Combine(FPaths::ProjectContentDir(), InPath);
    }
    return InPath;
}

void UDisplayComponent::RunOneFrame()
{
    if (!NesBus.IsValid()) return;

    NesBus->ppu.frame_complete = false;
    // Tight system clock loop. The bus internally clocks PPU every call and
    // CPU every third call.
    while (!NesBus->ppu.frame_complete)
    {
        NesBus->clock();
    }
    NesBus->ppu.frame_complete = false;
}

void UDisplayComponent::EnsureTextureCreated()
{
    if (ScreenTexture)
    {
        return;
    }

    const int32 W = ppu::kScreenWidth;
    const int32 H = ppu::kScreenHeight;

    ScreenTexture = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, TEXT("NesScreenTexture"));
    if (!ScreenTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("[NES] Failed to create screen texture"));
        return;
    }

    ScreenTexture->SRGB = false;
    ScreenTexture->Filter = bUsePointFiltering ? TF_Nearest : TF_Bilinear;
    ScreenTexture->MipGenSettings = TMGS_NoMipmaps;
    ScreenTexture->NeverStream = true;
    ScreenTexture->AddressX = TA_Clamp;
    ScreenTexture->AddressY = TA_Clamp;

    // Initialise the mip-0 bulk data to opaque black so the first frame on
    // screen isn't filled with whatever the allocator handed us.
    if (FTexturePlatformData* PlatformData = ScreenTexture->GetPlatformData())
    {
        if (PlatformData->Mips.Num() > 0)
        {
            FTexture2DMipMap& Mip = PlatformData->Mips[0];
            void* MipData = Mip.BulkData.Lock(LOCK_READ_WRITE);
            if (MipData)
            {
                FMemory::Memzero(MipData, W * H * sizeof(FColor));
            }
            Mip.BulkData.Unlock();
        }
    }

    ScreenTexture->UpdateResource();
}

void UDisplayComponent::UploadFrameToTexture()
{
    if (!NesBus.IsValid()) return;
    EnsureTextureCreated();
    if (!ScreenTexture) return;

    FTextureResource* Resource = ScreenTexture->GetResource();
    if (!Resource)
    {
        return;
    }

    const int32 W = ppu::kScreenWidth;
    const int32 H = ppu::kScreenHeight;
    const uint32 SrcPitch = W * sizeof(FColor);
    const SIZE_T NumBytes = SrcPitch * H;

    // Copy the framebuffer into a heap allocation that the render thread
    // takes ownership of. Doing this avoids a race with the next emulated
    // frame potentially overwriting the buffer before the GPU reads it.
    uint8* SrcData = reinterpret_cast<uint8*>(FMemory::Malloc(NumBytes));
    FMemory::Memcpy(SrcData, NesBus->ppu.GetScreenBuffer(), NumBytes);

    ENQUEUE_RENDER_COMMAND(NesUploadFrame)(
        [Resource, W, H, SrcPitch, SrcData](FRHICommandListImmediate& RHICmdList)
        {
            FRHITexture* TexRHI = Resource->TextureRHI;
            FRHITexture2D* Tex2D = TexRHI ? TexRHI->GetTexture2D() : nullptr;
            if (Tex2D)
            {
                const FUpdateTextureRegion2D Region(0, 0, 0, 0, W, H);
                RHIUpdateTexture2D(Tex2D, /*MipIndex=*/0, Region, SrcPitch, SrcData);
            }
            FMemory::Free(SrcData);
        });
}
