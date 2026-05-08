#pragma once
#include <cstdint>
#include <memory>

#include "cartridge.h"

class ppu {
public:
    ppu();
    ~ppu();

private:
    uint8_t tblName[2][1024];
    uint8_t tblPalette[32];
    uint8_t tblPattern[4][4096]; // for future extension (mappers)

    uint16_t scanline = 0;
    uint16_t cycle = 0;

public:
    uint8_t cpuRead(uint16_t address, bool readonly = false);
    void cpuWrite(uint16_t address, uint8_t data);

    uint8_t ppuRead(uint16_t address, bool readonly = false);
    void ppuWrite(uint16_t address, uint8_t data);

private:
    std::shared_ptr<Cartridge> cart;

public: // interface
    void ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge);
    void clock();
};
