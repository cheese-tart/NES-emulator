#pragma once
#include <cstdint>
#include <array>

#include "cpu.h"
#include "ppu.h"
#include "cartridge.h"

class Bus
{
public:
    Bus();
    ~Bus();

public: // devices on bus
    cpu cpu;
    ppu ppu;
    std::shared_ptr<Cartridge> cart;
    // fake ram
    uint8_t cpuRam[2048];

public: // bus read & write
    void cpuWrite(uint16_t address, uint8_t data);
    uint8_t cpuRead(uint16_t address, bool readOnly = false);

private: // a count of how many clocks have passed
    uint32_t nSystemClockCounter = 0;

public: // system interface
    void InsertCartridge(const std::shared_ptr<Cartridge>& cartridge);
    void reset();
    void clock();
};
