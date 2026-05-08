#include "mapper_0.h"

Mapper_0::Mapper_0(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
}

Mapper_0::~Mapper_0() {
}

bool Mapper_0::cpuMapRead(uint16_t address, uint32_t &mapped_addr) {
    if (address >= 0x8000 && address <= 0xFFFF) {
        mapped_addr = address & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_0::cpuMapWrite(uint16_t address, uint32_t &mapped_addr) {
    if (address >= 0x8000 && address <= 0xFFFF) {
        mapped_addr = address & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper_0::ppuMapRead(uint16_t address, uint32_t &mapped_addr) {
    if (address >= 0x0000 && address <= 0x1FFF) {
        mapped_addr = address;
        return true;
    }
    return false;
}

bool Mapper_0::ppuMapWrite(uint16_t address, uint32_t &mapped_addr) {
    if (address >= 0x0000 && address <= 0x1FFF) {
        if (nCHRBanks == 0) {
            mapped_addr = address;
            return true;
        }
    }
    return false;
}
