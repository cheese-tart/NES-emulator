#pragma once

#include "CoreMinimal.h"

#include <cstdint>
#include <cstring>
#include <memory>

#include "cartridge.h"

// 2C02 Picture Processing Unit.
//
// This implementation is engine-agnostic: it produces pixel buffers laid out as
// rows of FColor values (compatible with Unreal's PF_B8G8R8A8 textures). The
// UDisplayComponent samples these buffers each frame and uploads them to a
// dynamic UTexture2D so the emulator can be hosted inside Unreal Engine.
class ppu
{
public:
    ppu();
    ~ppu();

    // Output buffer dimensions.
    static constexpr int32 kScreenWidth   = 256;
    static constexpr int32 kScreenHeight  = 240;
    static constexpr int32 kPatternWidth  = 128;
    static constexpr int32 kPatternHeight = 128;

private:
    // PPU-side memories that back the address space when the cartridge does
    // not handle a given access.
    uint8_t tblName[2][1024];
    uint8_t tblPalette[32];
    uint8_t tblPattern[2][4096];

    // The 64-colour NES master palette, decoded once in the constructor.
    FColor palScreen[0x40];

    // Pixel output buffers in screen space (top-left origin, row-major).
    // These are the *only* surface the renderer ever needs to inspect; an
    // Unreal display component can copy them straight into a UTexture2D.
    FColor sprScreen[kScreenWidth * kScreenHeight];
    FColor sprNameTable[2][kScreenWidth * kScreenHeight];
    FColor sprPatternTable[2][kPatternWidth * kPatternHeight];

public:
    // ---- Frame buffer access for the host (UDisplayComponent) ------------

    /** Pointer to a kScreenWidth * kScreenHeight FColor buffer. */
    const FColor* GetScreenBuffer() const { return sprScreen; }
    int32 GetScreenWidth()  const { return kScreenWidth;  }
    int32 GetScreenHeight() const { return kScreenHeight; }

    /** Returns one of the two cached nametable visualisations. */
    const FColor* GetNameTable(uint8_t i) const { return sprNameTable[i & 1]; }

    /** Decodes one of the two CHR pattern tables into an 128x128 buffer using
        the supplied 4-colour palette index, and returns a pointer to it. */
    const FColor* GetPatternTable(uint8_t i, uint8_t palette);

    /** Resolves a (palette, pixel) pair through palette RAM into an FColor. */
    FColor GetColourFromPaletteRam(uint8_t palette, uint8_t pixel);

    /** Set to true at the end of every visible frame; the host clears it
        after pushing the buffer to the GPU. */
    bool frame_complete = false;

private:
    // ---- PPU registers ---------------------------------------------------
    union
    {
        struct
        {
            uint8_t unused : 5;
            uint8_t sprite_overflow : 1;
            uint8_t sprite_zero_hit : 1;
            uint8_t vertical_blank : 1;
        };
        uint8_t reg;
    } status;

    union
    {
        struct
        {
            uint8_t grayscale : 1;
            uint8_t render_background_left : 1;
            uint8_t render_sprites_left : 1;
            uint8_t render_background : 1;
            uint8_t render_sprites : 1;
            uint8_t enhance_red : 1;
            uint8_t enhance_green : 1;
            uint8_t enhance_blue : 1;
        };
        uint8_t reg;
    } mask;

    union PPUCTRL
    {
        struct
        {
            uint8_t nametable_x : 1;
            uint8_t nametable_y : 1;
            uint8_t increment_mode : 1;
            uint8_t pattern_sprite : 1;
            uint8_t pattern_background : 1;
            uint8_t sprite_size : 1;
            uint8_t slave_mode : 1; // unused
            uint8_t enable_nmi : 1;
        };
        uint8_t reg;
    } control;

    // "Loopy" internal scroll/address register.
    union loopy_register
    {
        struct
        {
            uint16_t coarse_x : 5;
            uint16_t coarse_y : 5;
            uint16_t nametable_x : 1;
            uint16_t nametable_y : 1;
            uint16_t fine_y : 3;
            uint16_t unused : 1;
        };
        uint16_t reg = 0x0000;
    };

    loopy_register vram_addr;
    loopy_register tram_addr;

    uint8_t fine_x = 0x00;

    uint8_t address_latch = 0x00;
    uint8_t ppu_data_buffer = 0x00;

    int16_t scanline = 0;
    int16_t cycle = 0;

    // ---- Background pipeline ---------------------------------------------
    uint8_t  bg_next_tile_id     = 0x00;
    uint8_t  bg_next_tile_attrib = 0x00;
    uint8_t  bg_next_tile_lsb    = 0x00;
    uint8_t  bg_next_tile_msb    = 0x00;
    uint16_t bg_shifter_pattern_lo = 0x0000;
    uint16_t bg_shifter_pattern_hi = 0x0000;
    uint16_t bg_shifter_attrib_lo  = 0x0000;
    uint16_t bg_shifter_attrib_hi  = 0x0000;

    // ---- Sprite pipeline -------------------------------------------------
    struct sObjectAttributeEntry
    {
        uint8_t y;
        uint8_t id;
        uint8_t attribute;
        uint8_t x;
    } OAM[64];

    uint8_t oam_addr = 0x00;

    sObjectAttributeEntry spriteScanline[8];
    uint8_t sprite_count = 0;
    uint8_t sprite_shifter_pattern_lo[8];
    uint8_t sprite_shifter_pattern_hi[8];

    bool bSpriteZeroHitPossible   = false;
    bool bSpriteZeroBeingRendered = false;

public:
    // OAM is exposed as a raw byte pointer for the bus to perform DMA into.
    uint8_t* pOAM = (uint8_t*)OAM;

    // ---- Bus communication ----------------------------------------------
    uint8_t cpuRead(uint16_t address, bool readonly = false);
    void    cpuWrite(uint16_t address, uint8_t data);

    uint8_t ppuRead(uint16_t address, bool readonly = false);
    void    ppuWrite(uint16_t address, uint8_t data);

private:
    std::shared_ptr<Cartridge> cart;

public:
    void ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge);
    void clock();
    void reset();

    /** Set by the PPU at the start of vblank when control.enable_nmi is on.
        The bus polls this and forwards an NMI to the CPU. */
    bool nmi = false;
};
