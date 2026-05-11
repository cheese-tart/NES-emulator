#pragma once
#include <cstdint>

class Mapper {
public:
    Mapper(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper();

public:
    // transform cpu bus address into prg rom offset
    virtual bool cpuMapRead(uint16_t address, uint32_t &mapped_addr) = 0;
    virtual bool cpuMapWrite(uint16_t address, uint32_t &mapped_addr) = 0;
    // transform ppu bus address into chr rom offset
    virtual bool ppuMapRead(uint16_t address, uint32_t &mapped_addr) = 0;
    virtual bool ppuMapWrite(uint16_t address, uint32_t &mapped_addr) = 0;

    virtual void reset() = 0;

protected:
    // these are stored locally as many of the mappers require this info
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;
};
