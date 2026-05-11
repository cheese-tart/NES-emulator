#pragma once
#include <cstdint>
#include <string>
#include <fstream>
#include <vector>

#include "mapper_0.h"

class Cartridge {
public:
    cartridge(const std::string& sFileName);
    ~cartridge();

public:
    bool ImageValid();

    enum MIRROR {
        HORIZONTAL,
        VERTICAL,
        ONESCREEN_LO,
        ONESCREEN_HI,
    } mirror = HORIZONTAL;

private:
    bool bImageValid = false;

    std::vector<uint8_t> vPRGMemory;
    std::vector<uint8_t> vCHRMemory;

    uint8_t nMapperID = 0;
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;

    std::shared_ptr<Mapper> pMapper;

public:
    // communication with main bus
    bool cpuRead(uint16_t address, uint8_t &data);
    bool cpuWrite(uint16_t address, uint8_t data);

    // communication with ppu bus
    bool ppuRead(uint16_t address, uint8_t &data);
    bool ppuWrite(uint16_t address, uint8_t data);

    // permits system rest of mapper to known state
    void reset();
};
