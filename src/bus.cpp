#include "bus.h"

Bus::Bus()
{
    cpu.ConnectBus(this);
}

Bus::~Bus()
{
}

void Bus::cpuWrite(uint16_t address, uint8_t data)
{
    if (cart->cpuWrite(address, data)) {
        // for future extension (mappers)
    } else if (address >= 0x0000 && address <= 0x1FFF) {
        cpuRam[address & 0x07FF] = data;
    } else if (address >= 0x2000 && address <= 0x3FFF) {
        ppu.cpuWrite(address & 0x0007, data);
    } else if (addr >= 0x4016 && addr <= 0x4017) {
        controller_state[addr & 0x0001] = controller[addr & 0x0001];
    }
}

uint8_t Bus::cpuRead(uint16_t address, bool readOnly)
{
    uint8_t data = 0x00;

    if (cart->cpuRead(address, data)) {
        // cartridge address range
    } else if (address >= 0x0000 && address <= 0x1FFF) {
        data = cpuRam[address & 0x07FF];
    } else if (address >= 0x2000 && address <= 0x3FFF) {
        data = ppu.cpuRead(address & 0x0007, readOnly);
    } else if (addr >= 0x4016 && addr <= 0x4017) {
        data = (controller_state[addr & 0x0001] & 0x80) > 0;
        controller_state[addr & 0x0001] <<= 1;
    }

    return data;
}

void Bus::InsertCartridge(const std::shared_ptr<Cartridge> &cartridge) {
    this->cart = cartridge;
    ppu.ConnectCartridge(cartridge);
}

void Bus::reset() {
    cart->reset();
    cpu.reset();
    ppu.reset();
    nSystemClockCounter = 0;
}

void Bus::clock() {
    ppu.clock();
    if (nSystemClockCounter % 3 == 0) {
        cpu.clock();
    }
    if (ppu.nmi) {
        ppu.nmi = false;
        cpu.nmi();
    }
    nSystemClockCounter++;
}
