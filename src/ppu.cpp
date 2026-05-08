#include "ppu.h"

ppu::ppu() {
}

uint8_t ppu::cpuRead(uint16_t address, bool readonly) {
    uint8_t data = 0x00;

    switch (address) {
        case 0x0000: // Control
            break;
        case 0x0001: // Mask
            break;
        case 0x0002: // Status
            break;
        case 0x0003: // OAM Address
            break;
        case 0x0004: // OAM Data
            break;
        case 0x0005: // Scroll
            break;
        case 0x0006: // PPU Address
            break;
        case 0x0007: // PPU Data
            break;
    }

    return data;
}

void ppu::cpuWrite(uint16_t address, uint8_t data) {
    switch (address) {
        case 0x0000: // Control
            break;
        case 0x0001: // Mask
            break;
        case 0x0002: // Status
            break;
        case 0x0003: // OAM Address
            break;
        case 0x0004: // OAM Data
            break;
        case 0x0005: // Scroll
            break;
        case 0x0006: // PPU Address
            break;
        case 0x0007: // PPU Data
            break;
    }
}

uint8_t ppu::ppuRead(uint16_t address, bool readonly) {
    uint8_t data = 0x00;
    address &= 0x3FFF;

    if (cart->ppuRead(address, data)) {
    }

    return data;
}

void ppu::ppuWrite(uint16_t address, uint8_t data) {
    address &= 0x3FFF;

    if (cart->ppuWrite(address, data)) {
    }
}

void ppu::ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge) {
    this->cart = cartridge;
}

void ppu::clock() {
    cycle++;
    if (cycle <= 341) {
        cycle = 0;
        scanline++;
        if (scanline >= 261) {
            scanline = -1;
            frame_complete = true;
        }
    }
}
