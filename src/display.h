#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/UniquePtr.h"

#include "bus.h"

#include "display.generated.h"

class UTexture2D;

/** Logical NES controller buttons. The byte sent to the bus is encoded MSB-first
    in standard NES order: A, B, Select, Start, Up, Down, Left, Right. */
UENUM(BlueprintType)
enum class ENesButton : uint8
{
    A      UMETA(DisplayName = "A"),
    B      UMETA(DisplayName = "B"),
    Select UMETA(DisplayName = "Select"),
    Start  UMETA(DisplayName = "Start"),
    Up     UMETA(DisplayName = "Up"),
    Down   UMETA(DisplayName = "Down"),
    Left   UMETA(DisplayName = "Left"),
    Right  UMETA(DisplayName = "Right"),
};

/**
 * UDisplayComponent
 *
 * Hosts a full NES emulator (CPU + PPU + cartridge bus) inside an Unreal Engine
 * actor. Each frame this component clocks the NES until the PPU signals a
 * complete frame and then uploads the 256x240 framebuffer into a dynamic
 * UTexture2D. Bind GetScreenTexture() into a material parameter (with point
 * filtering, no sRGB) and apply that material to a plane mesh / UMG widget /
 * post-process to display the emulator output.
 *
 * Add this component to any AActor, set AutoLoadRomPath to an iNES (.nes) file,
 * and the emulator will start running on BeginPlay.
 */
UCLASS(ClassGroup = (NES), meta = (BlueprintSpawnableComponent), Blueprintable)
class UDisplayComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDisplayComponent();

    //~ Begin UActorComponent interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    //~ End UActorComponent interface

    // ---- ROM management --------------------------------------------------

    /** Load an iNES (.nes) ROM. Path can be absolute or relative to the
        project's content directory. Returns true if the image is valid. */
    UFUNCTION(BlueprintCallable, Category = "NES|ROM")
    bool LoadCartridge(const FString& RomFilePath);

    /** Hard-reset the emulator, preserving the currently loaded ROM. */
    UFUNCTION(BlueprintCallable, Category = "NES|ROM")
    void ResetEmulator();

    /** True when a ROM has been loaded and is ready to run. */
    UFUNCTION(BlueprintPure, Category = "NES|ROM")
    bool IsCartridgeLoaded() const;

    // ---- Display ---------------------------------------------------------

    /** Live framebuffer texture. Bind to a material; the component refreshes
        its contents once per emulated frame. */
    UFUNCTION(BlueprintPure, Category = "NES|Display")
    UTexture2D* GetScreenTexture() const { return ScreenTexture; }

    // ---- Input -----------------------------------------------------------

    /** Set or clear a single button on a given controller (0 or 1). */
    UFUNCTION(BlueprintCallable, Category = "NES|Input")
    void SetControllerButton(int32 Player, ENesButton Button, bool bPressed);

    /** Replace the entire 8-bit controller state for a given controller. The
        bit layout (MSB to LSB) is A, B, Select, Start, Up, Down, Left, Right. */
    UFUNCTION(BlueprintCallable, Category = "NES|Input")
    void SetControllerStateRaw(int32 Player, uint8 State);

    /** Clear all currently-held buttons on both controllers. */
    UFUNCTION(BlueprintCallable, Category = "NES|Input")
    void ClearControllers();

    // ---- Properties ------------------------------------------------------

    /** Multiplier applied to NTSC frame rate. 1.0 == real-time (60.0988 Hz). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NES",
              meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "4.0"))
    float EmulationSpeed = 1.0f;

    /** When set, this ROM is loaded automatically on BeginPlay. Absolute path,
        or path relative to FPaths::ProjectContentDir(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NES")
    FString AutoLoadRomPath;

    /** If true, the screen texture's filter is set to nearest-neighbour for a
        crunchy authentic look. Disable for bilinear filtering. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NES|Display")
    bool bUsePointFiltering = true;

protected:
    /** GPU-resident framebuffer the emulator writes to each frame. */
    UPROPERTY(VisibleAnywhere, Transient, Category = "NES|Display")
    UTexture2D* ScreenTexture = nullptr;

private:
    /** Lazily allocate the dynamic screen texture and clear it to black. */
    void EnsureTextureCreated();

    /** Push the current PPU framebuffer to the GPU resource backing
        ScreenTexture. Called once per emulated frame on the game thread. */
    void UploadFrameToTexture();

    /** Run the bus until the PPU signals a complete frame. Safe no-op when
        no cartridge is loaded. */
    void RunOneFrame();

    /** Map a logical NES button to its bit mask in the controller byte. */
    static uint8 ButtonMask(ENesButton Button);

    /** Resolve a ROM path: absolute paths are returned as-is, otherwise the
        path is interpreted relative to the project's Content directory. */
    static FString ResolveRomPath(const FString& InPath);

    /** The full NES system. Held by-pointer so we can null it before loading. */
    TUniquePtr<Bus> NesBus;

    /** Live controller state mirrored each frame into NesBus->controller. */
    uint8 LiveController[2] = { 0, 0 };

    /** Accumulator for fractional frames when EmulationSpeed != 1.0. */
    double FrameAccumulator = 0.0;
};
