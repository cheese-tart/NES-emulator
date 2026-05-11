#include "ppu.h"

// FColor is BGRA in memory, but its constructor takes (R, G, B, A) — see
// the standard NES master palette below.

ppu::ppu()
{
    palScreen[0x00] = FColor( 84,  84,  84);
    palScreen[0x01] = FColor(  0,  30, 116);
    palScreen[0x02] = FColor(  8,  16, 144);
    palScreen[0x03] = FColor( 48,   0, 136);
    palScreen[0x04] = FColor( 68,   0, 100);
    palScreen[0x05] = FColor( 92,   0,  48);
    palScreen[0x06] = FColor( 84,   4,   0);
    palScreen[0x07] = FColor( 60,  24,   0);
    palScreen[0x08] = FColor( 32,  42,   0);
    palScreen[0x09] = FColor(  8,  58,   0);
    palScreen[0x0A] = FColor(  0,  64,   0);
    palScreen[0x0B] = FColor(  0,  60,   0);
    palScreen[0x0C] = FColor(  0,  50,  60);
    palScreen[0x0D] = FColor(  0,   0,   0);
    palScreen[0x0E] = FColor(  0,   0,   0);
    palScreen[0x0F] = FColor(  0,   0,   0);

    palScreen[0x10] = FColor(152, 150, 152);
    palScreen[0x11] = FColor(  8,  76, 196);
    palScreen[0x12] = FColor( 48,  50, 236);
    palScreen[0x13] = FColor( 92,  30, 228);
    palScreen[0x14] = FColor(136,  20, 176);
    palScreen[0x15] = FColor(160,  20, 100);
    palScreen[0x16] = FColor(152,  34,  32);
    palScreen[0x17] = FColor(120,  60,   0);
    palScreen[0x18] = FColor( 84,  90,   0);
    palScreen[0x19] = FColor( 40, 114,   0);
    palScreen[0x1A] = FColor(  8, 124,   0);
    palScreen[0x1B] = FColor(  0, 118,  40);
    palScreen[0x1C] = FColor(  0, 102, 120);
    palScreen[0x1D] = FColor(  0,   0,   0);
    palScreen[0x1E] = FColor(  0,   0,   0);
    palScreen[0x1F] = FColor(  0,   0,   0);

    palScreen[0x20] = FColor(236, 238, 236);
    palScreen[0x21] = FColor( 76, 154, 236);
    palScreen[0x22] = FColor(120, 124, 236);
    palScreen[0x23] = FColor(176,  98, 236);
    palScreen[0x24] = FColor(228,  84, 236);
    palScreen[0x25] = FColor(236,  88, 180);
    palScreen[0x26] = FColor(236, 106, 100);
    palScreen[0x27] = FColor(212, 136,  32);
    palScreen[0x28] = FColor(160, 170,   0);
    palScreen[0x29] = FColor(116, 196,   0);
    palScreen[0x2A] = FColor( 76, 208,  32);
    palScreen[0x2B] = FColor( 56, 204, 108);
    palScreen[0x2C] = FColor( 56, 180, 204);
    palScreen[0x2D] = FColor( 60,  60,  60);
    palScreen[0x2E] = FColor(  0,   0,   0);
    palScreen[0x2F] = FColor(  0,   0,   0);

    palScreen[0x30] = FColor(236, 238, 236);
    palScreen[0x31] = FColor(168, 204, 236);
    palScreen[0x32] = FColor(188, 188, 236);
    palScreen[0x33] = FColor(212, 178, 236);
    palScreen[0x34] = FColor(236, 174, 236);
    palScreen[0x35] = FColor(236, 174, 212);
    palScreen[0x36] = FColor(236, 180, 176);
    palScreen[0x37] = FColor(228, 196, 144);
    palScreen[0x38] = FColor(204, 210, 120);
    palScreen[0x39] = FColor(180, 222, 120);
    palScreen[0x3A] = FColor(168, 226, 144);
    palScreen[0x3B] = FColor(152, 226, 180);
    palScreen[0x3C] = FColor(160, 214, 228);
    palScreen[0x3D] = FColor(160, 162, 160);
    palScreen[0x3E] = FColor(  0,   0,   0);
    palScreen[0x3F] = FColor(  0,   0,   0);

    // Initialise output buffers to opaque black so the first uploaded frame
    // doesn't display uninitialised garbage.
    const FColor Black = FColor(0, 0, 0);
    for (int32 i = 0; i < kScreenWidth * kScreenHeight; ++i)
    {
        sprScreen[i] = Black;
        sprNameTable[0][i] = Black;
        sprNameTable[1][i] = Black;
    }
    for (int32 i = 0; i < kPatternWidth * kPatternHeight; ++i)
    {
        sprPatternTable[0][i] = Black;
        sprPatternTable[1][i] = Black;
    }

    std::memset(tblName,    0, sizeof(tblName));
    std::memset(tblPalette, 0, sizeof(tblPalette));
    std::memset(tblPattern, 0, sizeof(tblPattern));

    status.reg  = 0;
    mask.reg    = 0;
    control.reg = 0;

    std::memset(OAM, 0, sizeof(OAM));
    std::memset(spriteScanline, 0, sizeof(spriteScanline));
    std::memset(sprite_shifter_pattern_lo, 0, sizeof(sprite_shifter_pattern_lo));
    std::memset(sprite_shifter_pattern_hi, 0, sizeof(sprite_shifter_pattern_hi));
}

ppu::~ppu()
{
}

FColor ppu::GetColourFromPaletteRam(uint8_t palette, uint8_t pixel)
{
    // 0x3F00 + (palette << 2) + pixel = absolute palette-RAM address;
    // the 0x3F mask keeps us in range of the 64-entry master palette.
    return palScreen[ppuRead(0x3F00 + (palette << 2) + pixel) & 0x3F];
}

const FColor* ppu::GetPatternTable(uint8_t i, uint8_t palette)
{
    // A pattern table is laid out as 16x16 8x8 tiles (= 128x128 pixels).
    // Each tile is 16 bytes: 8 bytes of LSB plane followed by 8 bytes MSB.
    for (uint16_t nTileY = 0; nTileY < 16; nTileY++)
    {
        for (uint16_t nTileX = 0; nTileX < 16; nTileX++)
        {
            const uint16_t nOffset = nTileY * 256 + nTileX * 16;

            for (uint16_t row = 0; row < 8; row++)
            {
                uint8_t tile_lsb = ppuRead(i * 0x1000 + nOffset + row + 0x0000);
                uint8_t tile_msb = ppuRead(i * 0x1000 + nOffset + row + 0x0008);

                for (uint16_t col = 0; col < 8; col++)
                {
                    const uint8_t pixel = (tile_lsb & 0x01) << 1 | (tile_msb & 0x01);
                    tile_lsb >>= 1;
                    tile_msb >>= 1;

                    const int32 X = nTileX * 8 + (7 - col);
                    const int32 Y = nTileY * 8 + row;
                    if (X >= 0 && X < kPatternWidth && Y >= 0 && Y < kPatternHeight)
                    {
                        sprPatternTable[i & 1][Y * kPatternWidth + X] =
                            GetColourFromPaletteRam(palette, pixel);
                    }
                }
            }
        }
    }
    return sprPatternTable[i & 1];
}

uint8_t ppu::cpuRead(uint16_t address, bool readonly)
{
    uint8_t data = 0x00;

    if (readonly)
    {
        // Non-destructive read used for debug/inspection.
        switch (address)
        {
        case 0x0000: data = control.reg; break; // Control
        case 0x0001: data = mask.reg;    break; // Mask
        case 0x0002: data = status.reg;  break; // Status
        case 0x0003: break;                     // OAM Address
        case 0x0004: break;                     // OAM Data
        case 0x0005: break;                     // Scroll
        case 0x0006: break;                     // PPU Address
        case 0x0007: break;                     // PPU Data
        }
    }
    else
    {
        switch (address)
        {
        case 0x0000: break; // Control - not readable
        case 0x0001: break; // Mask    - not readable

        case 0x0002: // Status
            // Top 3 bits are real flags; bottom 5 bits are noise from the
            // last PPU bus transaction (some games actually rely on this).
            data = (status.reg & 0xE0) | (ppu_data_buffer & 0x1F);
            status.vertical_blank = 0;
            address_latch = 0;
            break;

        case 0x0003: break; // OAM Address - not readable

        case 0x0004: // OAM Data
            data = pOAM[oam_addr];
            break;

        case 0x0005: break; // Scroll      - not readable
        case 0x0006: break; // PPU Address - not readable

        case 0x0007: // PPU Data
            // Reads from nametable RAM are delayed one cycle; palette reads
            // are returned immediately.
            data = ppu_data_buffer;
            ppu_data_buffer = ppuRead(vram_addr.reg);
            if (vram_addr.reg >= 0x3F00) data = ppu_data_buffer;
            vram_addr.reg += (control.increment_mode ? 32 : 1);
            break;
        }
    }

    return data;
}

void ppu::cpuWrite(uint16_t address, uint8_t data)
{
    switch (address)
    {
    case 0x0000: // Control
        control.reg = data;
        tram_addr.nametable_x = control.nametable_x;
        tram_addr.nametable_y = control.nametable_y;
        break;

    case 0x0001: // Mask
        mask.reg = data;
        break;

    case 0x0002: // Status (read-only)
        break;

    case 0x0003: // OAM Address
        oam_addr = data;
        break;

    case 0x0004: // OAM Data
        pOAM[oam_addr] = data;
        break;

    case 0x0005: // Scroll
        if (address_latch == 0)
        {
            fine_x = data & 0x07;
            tram_addr.coarse_x = data >> 3;
            address_latch = 1;
        }
        else
        {
            tram_addr.fine_y   = data & 0x07;
            tram_addr.coarse_y = data >> 3;
            address_latch = 0;
        }
        break;

    case 0x0006: // PPU Address
        if (address_latch == 0)
        {
            tram_addr.reg = (uint16_t)((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF);
            address_latch = 1;
        }
        else
        {
            tram_addr.reg = (tram_addr.reg & 0xFF00) | data;
            vram_addr = tram_addr;
            address_latch = 0;
        }
        break;

    case 0x0007: // PPU Data
        ppuWrite(vram_addr.reg, data);
        vram_addr.reg += (control.increment_mode ? 32 : 1);
        break;
    }
}

uint8_t ppu::ppuRead(uint16_t address, bool /*readonly*/)
{
    uint8_t data = 0x00;
    address &= 0x3FFF;

    if (cart && cart->ppuRead(address, data))
    {
        // Cartridge handled the read.
    }
    else if (address >= 0x0000 && address <= 0x1FFF)
    {
        data = tblPattern[(address & 0x1000) >> 12][address & 0x0FFF];
    }
    else if (address >= 0x2000 && address <= 0x3EFF)
    {
        address &= 0x0FFF;

        if (cart && cart->mirror == Cartridge::MIRROR::VERTICAL)
        {
            if (address >= 0x0000 && address <= 0x03FF) data = tblName[0][address & 0x03FF];
            if (address >= 0x0400 && address <= 0x07FF) data = tblName[1][address & 0x03FF];
            if (address >= 0x0800 && address <= 0x0BFF) data = tblName[0][address & 0x03FF];
            if (address >= 0x0C00 && address <= 0x0FFF) data = tblName[1][address & 0x03FF];
        }
        else if (cart && cart->mirror == Cartridge::MIRROR::HORIZONTAL)
        {
            if (address >= 0x0000 && address <= 0x03FF) data = tblName[0][address & 0x03FF];
            if (address >= 0x0400 && address <= 0x07FF) data = tblName[0][address & 0x03FF];
            if (address >= 0x0800 && address <= 0x0BFF) data = tblName[1][address & 0x03FF];
            if (address >= 0x0C00 && address <= 0x0FFF) data = tblName[1][address & 0x03FF];
        }
    }
    else if (address >= 0x3F00 && address <= 0x3FFF)
    {
        address &= 0x001F;
        if (address == 0x0010) address = 0x0000;
        if (address == 0x0014) address = 0x0004;
        if (address == 0x0018) address = 0x0008;
        if (address == 0x001C) address = 0x000C;
        data = tblPalette[address] & (mask.grayscale ? 0x30 : 0x3F);
    }

    return data;
}

void ppu::ppuWrite(uint16_t address, uint8_t data)
{
    address &= 0x3FFF;

    if (cart && cart->ppuWrite(address, data))
    {
        // Cartridge handled the write.
    }
    else if (address >= 0x0000 && address <= 0x1FFF)
    {
        tblPattern[(address & 0x1000) >> 12][address & 0x0FFF] = data;
    }
    else if (address >= 0x2000 && address <= 0x3EFF)
    {
        address &= 0x0FFF;

        if (cart && cart->mirror == Cartridge::MIRROR::VERTICAL)
        {
            if (address >= 0x0000 && address <= 0x03FF) tblName[0][address & 0x03FF] = data;
            if (address >= 0x0400 && address <= 0x07FF) tblName[1][address & 0x03FF] = data;
            if (address >= 0x0800 && address <= 0x0BFF) tblName[0][address & 0x03FF] = data;
            if (address >= 0x0C00 && address <= 0x0FFF) tblName[1][address & 0x03FF] = data;
        }
        else if (cart && cart->mirror == Cartridge::MIRROR::HORIZONTAL)
        {
            if (address >= 0x0000 && address <= 0x03FF) tblName[0][address & 0x03FF] = data;
            if (address >= 0x0400 && address <= 0x07FF) tblName[0][address & 0x03FF] = data;
            if (address >= 0x0800 && address <= 0x0BFF) tblName[1][address & 0x03FF] = data;
            if (address >= 0x0C00 && address <= 0x0FFF) tblName[1][address & 0x03FF] = data;
        }
    }
    else if (address >= 0x3F00 && address <= 0x3FFF)
    {
        address &= 0x001F;
        if (address == 0x0010) address = 0x0000;
        if (address == 0x0014) address = 0x0004;
        if (address == 0x0018) address = 0x0008;
        if (address == 0x001C) address = 0x000C;
        tblPalette[address] = data;
    }
}

void ppu::ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge)
{
    this->cart = cartridge;
}

void ppu::reset()
{
    fine_x = 0x00;
    address_latch = 0x00;
    ppu_data_buffer = 0x00;
    scanline = 0;
    cycle = 0;
    bg_next_tile_id = 0x00;
    bg_next_tile_attrib = 0x00;
    bg_next_tile_lsb = 0x00;
    bg_next_tile_msb = 0x00;
    bg_shifter_pattern_lo = 0x0000;
    bg_shifter_pattern_hi = 0x0000;
    bg_shifter_attrib_lo  = 0x0000;
    bg_shifter_attrib_hi  = 0x0000;
    status.reg  = 0x00;
    mask.reg    = 0x00;
    control.reg = 0x00;
    vram_addr.reg = 0x0000;
    tram_addr.reg = 0x0000;
    sprite_count = 0;
    bSpriteZeroHitPossible   = false;
    bSpriteZeroBeingRendered = false;
    nmi = false;
    frame_complete = false;
    std::memset(spriteScanline, 0, sizeof(spriteScanline));
    std::memset(sprite_shifter_pattern_lo, 0, sizeof(sprite_shifter_pattern_lo));
    std::memset(sprite_shifter_pattern_hi, 0, sizeof(sprite_shifter_pattern_hi));
}

void ppu::clock()
{
    // The PPU is a state machine driven by (scanline, cycle). Lambdas below
    // factor out the common operations performed at specific points within
    // the rendering pipeline.

    auto IncrementScrollX = [&]()
    {
        if (mask.render_background || mask.render_sprites)
        {
            if (vram_addr.coarse_x == 31)
            {
                vram_addr.coarse_x = 0;
                vram_addr.nametable_x = ~vram_addr.nametable_x;
            }
            else
            {
                vram_addr.coarse_x++;
            }
        }
    };

    auto IncrementScrollY = [&]()
    {
        if (mask.render_background || mask.render_sprites)
        {
            if (vram_addr.fine_y < 7)
            {
                vram_addr.fine_y++;
            }
            else
            {
                vram_addr.fine_y = 0;

                if (vram_addr.coarse_y == 29)
                {
                    vram_addr.coarse_y = 0;
                    vram_addr.nametable_y = ~vram_addr.nametable_y;
                }
                else if (vram_addr.coarse_y == 31)
                {
                    vram_addr.coarse_y = 0;
                }
                else
                {
                    vram_addr.coarse_y++;
                }
            }
        }
    };

    auto TransferAddressX = [&]()
    {
        if (mask.render_background || mask.render_sprites)
        {
            vram_addr.nametable_x = tram_addr.nametable_x;
            vram_addr.coarse_x    = tram_addr.coarse_x;
        }
    };

    auto TransferAddressY = [&]()
    {
        if (mask.render_background || mask.render_sprites)
        {
            vram_addr.fine_y      = tram_addr.fine_y;
            vram_addr.nametable_y = tram_addr.nametable_y;
            vram_addr.coarse_y    = tram_addr.coarse_y;
        }
    };

    auto LoadBackgroundShifters = [&]()
    {
        bg_shifter_pattern_lo = (bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb;
        bg_shifter_pattern_hi = (bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb;

        bg_shifter_attrib_lo = (bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0b01) ? 0xFF : 0x00);
        bg_shifter_attrib_hi = (bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0b10) ? 0xFF : 0x00);
    };

    auto UpdateShifters = [&]()
    {
        if (mask.render_background)
        {
            bg_shifter_pattern_lo <<= 1;
            bg_shifter_pattern_hi <<= 1;
            bg_shifter_attrib_lo  <<= 1;
            bg_shifter_attrib_hi  <<= 1;
        }

        if (mask.render_sprites && cycle >= 1 && cycle < 258)
        {
            for (int i = 0; i < sprite_count; i++)
            {
                if (spriteScanline[i].x > 0)
                {
                    spriteScanline[i].x--;
                }
                else
                {
                    sprite_shifter_pattern_lo[i] <<= 1;
                    sprite_shifter_pattern_hi[i] <<= 1;
                }
            }
        }
    };

    if (scanline >= -1 && scanline < 240)
    {
        // ---- Background rendering ---------------------------------------

        if (scanline == 0 && cycle == 0)
        {
            // "Odd frame" cycle skip.
            cycle = 1;
        }

        if (scanline == -1 && cycle == 1)
        {
            status.vertical_blank  = 0;
            status.sprite_overflow = 0;
            status.sprite_zero_hit = 0;
            for (int i = 0; i < 8; i++)
            {
                sprite_shifter_pattern_lo[i] = 0;
                sprite_shifter_pattern_hi[i] = 0;
            }
        }

        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338))
        {
            UpdateShifters();

            switch ((cycle - 1) % 8)
            {
            case 0:
                LoadBackgroundShifters();
                bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
                break;

            case 2:
                bg_next_tile_attrib = ppuRead(0x23C0 | (vram_addr.nametable_y << 11)
                                                    | (vram_addr.nametable_x << 10)
                                                    | ((vram_addr.coarse_y >> 2) << 3)
                                                    | (vram_addr.coarse_x >> 2));
                if (vram_addr.coarse_y & 0x02) bg_next_tile_attrib >>= 4;
                if (vram_addr.coarse_x & 0x02) bg_next_tile_attrib >>= 2;
                bg_next_tile_attrib &= 0x03;
                break;

            case 4:
                bg_next_tile_lsb = ppuRead((control.pattern_background << 12)
                                           + ((uint16_t)bg_next_tile_id << 4)
                                           + (vram_addr.fine_y) + 0);
                break;

            case 6:
                bg_next_tile_msb = ppuRead((control.pattern_background << 12)
                                           + ((uint16_t)bg_next_tile_id << 4)
                                           + (vram_addr.fine_y) + 8);
                break;

            case 7:
                IncrementScrollX();
                break;
            }
        }

        if (cycle == 256)
        {
            IncrementScrollY();
        }

        if (cycle == 257)
        {
            LoadBackgroundShifters();
            TransferAddressX();
        }

        // Superfluous reads of tile id at end of scanline.
        if (cycle == 338 || cycle == 340)
        {
            bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
        }

        if (scanline == -1 && cycle >= 280 && cycle < 305)
        {
            TransferAddressY();
        }

        // ---- Foreground (sprite) evaluation -----------------------------
        // Done in a single hit at cycle 257, which is a simplification of
        // the real PPU behaviour but is sufficient for most games.
        if (cycle == 257 && scanline >= 0)
        {
            std::memset(spriteScanline, 0xFF, 8 * sizeof(sObjectAttributeEntry));
            sprite_count = 0;

            for (uint8_t i = 0; i < 8; i++)
            {
                sprite_shifter_pattern_lo[i] = 0;
                sprite_shifter_pattern_hi[i] = 0;
            }

            uint8_t nOAMEntry = 0;
            bSpriteZeroHitPossible = false;

            while (nOAMEntry < 64 && sprite_count < 9)
            {
                int16_t diff = ((int16_t)scanline - (int16_t)OAM[nOAMEntry].y);

                if (diff >= 0 && diff < (control.sprite_size ? 16 : 8))
                {
                    if (sprite_count < 8)
                    {
                        if (nOAMEntry == 0)
                        {
                            bSpriteZeroHitPossible = true;
                        }

                        std::memcpy(&spriteScanline[sprite_count], &OAM[nOAMEntry], sizeof(sObjectAttributeEntry));
                        sprite_count++;
                    }
                }
                nOAMEntry++;
            }

            status.sprite_overflow = (sprite_count > 8);
        }

        if (cycle == 340)
        {
            for (uint8_t i = 0; i < sprite_count; i++)
            {
                uint8_t  sprite_pattern_bits_lo, sprite_pattern_bits_hi;
                uint16_t sprite_pattern_addr_lo, sprite_pattern_addr_hi;

                if (!control.sprite_size)
                {
                    // 8x8 sprite mode — pattern table comes from CTRL.
                    if (!(spriteScanline[i].attribute & 0x80))
                    {
                        sprite_pattern_addr_lo =
                              (control.pattern_sprite << 12)
                            | (spriteScanline[i].id   << 4)
                            | (scanline - spriteScanline[i].y);
                    }
                    else
                    {
                        sprite_pattern_addr_lo =
                              (control.pattern_sprite << 12)
                            | (spriteScanline[i].id   << 4)
                            | (7 - (scanline - spriteScanline[i].y));
                    }
                }
                else
                {
                    // 8x16 sprite mode — pattern table comes from sprite ID bit 0.
                    if (!(spriteScanline[i].attribute & 0x80))
                    {
                        if (scanline - spriteScanline[i].y < 8)
                        {
                            sprite_pattern_addr_lo =
                                  ((spriteScanline[i].id & 0x01) << 12)
                                | ((spriteScanline[i].id & 0xFE) << 4)
                                | ((scanline - spriteScanline[i].y) & 0x07);
                        }
                        else
                        {
                            sprite_pattern_addr_lo =
                                  ( (spriteScanline[i].id & 0x01)      << 12)
                                | (((spriteScanline[i].id & 0xFE) + 1) << 4)
                                | ((scanline - spriteScanline[i].y) & 0x07);
                        }
                    }
                    else
                    {
                        if (scanline - spriteScanline[i].y < 8)
                        {
                            sprite_pattern_addr_lo =
                                  ( (spriteScanline[i].id & 0x01)      << 12)
                                | (((spriteScanline[i].id & 0xFE) + 1) << 4)
                                | (7 - (scanline - spriteScanline[i].y) & 0x07);
                        }
                        else
                        {
                            sprite_pattern_addr_lo =
                                  ((spriteScanline[i].id & 0x01) << 12)
                                | ((spriteScanline[i].id & 0xFE) << 4)
                                | (7 - (scanline - spriteScanline[i].y) & 0x07);
                        }
                    }
                }

                sprite_pattern_addr_hi = sprite_pattern_addr_lo + 8;

                sprite_pattern_bits_lo = ppuRead(sprite_pattern_addr_lo);
                sprite_pattern_bits_hi = ppuRead(sprite_pattern_addr_hi);

                if (spriteScanline[i].attribute & 0x40)
                {
                    auto flipbyte = [](uint8_t b)
                    {
                        b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
                        b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
                        b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
                        return b;
                    };
                    sprite_pattern_bits_lo = flipbyte(sprite_pattern_bits_lo);
                    sprite_pattern_bits_hi = flipbyte(sprite_pattern_bits_hi);
                }

                sprite_shifter_pattern_lo[i] = sprite_pattern_bits_lo;
                sprite_shifter_pattern_hi[i] = sprite_pattern_bits_hi;
            }
        }
    }

    if (scanline == 240)
    {
        // Post-render scanline — nothing to do.
    }

    if (scanline >= 241 && scanline < 261)
    {
        if (scanline == 241 && cycle == 1)
        {
            status.vertical_blank = 1;
            if (control.enable_nmi)
                nmi = true;
        }
    }

    // ---- Pixel composition ----------------------------------------------

    uint8_t bg_pixel   = 0x00;
    uint8_t bg_palette = 0x00;

    if (mask.render_background)
    {
        const uint16_t bit_mux = 0x8000 >> fine_x;
        const uint8_t  p0_pixel = (bg_shifter_pattern_lo & bit_mux) > 0;
        const uint8_t  p1_pixel = (bg_shifter_pattern_hi & bit_mux) > 0;
        bg_pixel = (p1_pixel << 1) | p0_pixel;

        const uint8_t bg_pal0 = (bg_shifter_attrib_lo & bit_mux) > 0;
        const uint8_t bg_pal1 = (bg_shifter_attrib_hi & bit_mux) > 0;
        bg_palette = (bg_pal1 << 1) | bg_pal0;
    }

    uint8_t fg_pixel    = 0x00;
    uint8_t fg_palette  = 0x00;
    uint8_t fg_priority = 0x00;

    if (mask.render_sprites)
    {
        bSpriteZeroBeingRendered = false;

        for (uint8_t i = 0; i < sprite_count; i++)
        {
            if (spriteScanline[i].x == 0)
            {
                const uint8_t fg_pixel_lo = (sprite_shifter_pattern_lo[i] & 0x80) > 0;
                const uint8_t fg_pixel_hi = (sprite_shifter_pattern_hi[i] & 0x80) > 0;
                fg_pixel = (fg_pixel_hi << 1) | fg_pixel_lo;

                fg_palette  = (spriteScanline[i].attribute & 0x03) + 0x04;
                fg_priority = (spriteScanline[i].attribute & 0x20) == 0;

                if (fg_pixel != 0)
                {
                    if (i == 0) bSpriteZeroBeingRendered = true;
                    break;
                }
            }
        }
    }

    uint8_t pixel   = 0x00;
    uint8_t palette = 0x00;

    if (bg_pixel == 0 && fg_pixel == 0)
    {
        pixel   = 0x00;
        palette = 0x00;
    }
    else if (bg_pixel == 0 && fg_pixel > 0)
    {
        pixel   = fg_pixel;
        palette = fg_palette;
    }
    else if (bg_pixel > 0 && fg_pixel == 0)
    {
        pixel   = bg_pixel;
        palette = bg_palette;
    }
    else if (bg_pixel > 0 && fg_pixel > 0)
    {
        if (fg_priority)
        {
            pixel   = fg_pixel;
            palette = fg_palette;
        }
        else
        {
            pixel   = bg_pixel;
            palette = bg_palette;
        }

        if (bSpriteZeroHitPossible && bSpriteZeroBeingRendered)
        {
            if (mask.render_background & mask.render_sprites)
            {
                if (~(mask.render_background_left | mask.render_sprites_left))
                {
                    if (cycle >= 9 && cycle < 258)
                    {
                        status.sprite_zero_hit = 1;
                    }
                }
                else
                {
                    if (cycle >= 1 && cycle < 258)
                    {
                        status.sprite_zero_hit = 1;
                    }
                }
            }
        }
    }

    // Write the resolved pixel into the screen-space framebuffer that the
    // UDisplayComponent will later upload to a UTexture2D. Bounds-check
    // because (cycle, scanline) may legitimately reach values outside the
    // visible 256x240 region during HBlank/VBlank.
    {
        const int32 X = (int32)cycle - 1;
        const int32 Y = (int32)scanline;
        if (X >= 0 && X < kScreenWidth && Y >= 0 && Y < kScreenHeight)
        {
            sprScreen[Y * kScreenWidth + X] = GetColourFromPaletteRam(palette, pixel);
        }
    }

    cycle++;
    if (cycle >= 341)
    {
        cycle = 0;
        scanline++;
        if (scanline >= 261)
        {
            scanline = -1;
            frame_complete = true;
        }
    }
}
