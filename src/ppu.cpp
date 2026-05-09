#include "ppu.h"

ppu::ppu() {
}

ppu::~ppu() {
}

uint8_t ppu::cpuRead(uint16_t address, bool readonly) {
    if (rdonly)
	{
		// Reading from PPU registers can affect their contents
		// so this read only option is used for examining the
		// state of the PPU without changing its state. This is
		// really only used in debug mode.
		switch (addr)
		{
		case 0x0000: // Control
			data = control.reg;
			break;
		case 0x0001: // Mask
			data = mask.reg;
			break;
		case 0x0002: // Status
			data = status.reg;
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
	else
	{
		// These are the live PPU registers that repsond
		// to being read from in various ways. Note that not
		// all the registers are capable of being read from
		// so they just return 0x00
		switch (addr)
		{
			// Control - Not readable
		case 0x0000: break;

			// Mask - Not Readable
		case 0x0001: break;

			// Status
		case 0x0002:
			// Reading from the status register has the effect of resetting
			// different parts of the circuit. Only the top three bits
			// contain status information, however it is possible that
			// some "noise" gets picked up on the bottom 5 bits which
			// represent the last PPU bus transaction. Some games "may"
			// use this noise as valid data (even though they probably
			// shouldn't)
			data = (status.reg & 0xE0) | (ppu_data_buffer & 0x1F);

			// Clear the vertical blanking flag
			status.vertical_blank = 0;

			// Reset Loopy's Address latch flag
			address_latch = 0;
			break;

			// OAM Address
		case 0x0003: break;

			// OAM Data
		case 0x0004: break;

			// Scroll - Not Readable
		case 0x0005: break;

			// PPU Address - Not Readable
		case 0x0006: break;

			// PPU Data
		case 0x0007:
			// Reads from the NameTable ram get delayed one cycle,
			// so output buffer which contains the data from the
			// previous read request
			data = ppu_data_buffer;
			// then update the buffer for next time
			ppu_data_buffer = ppuRead(vram_addr.reg);
			// However, if the address was in the palette range, the
			// data is not delayed, so it returns immediately
			if (vram_addr.reg >= 0x3F00) data = ppu_data_buffer;
			// All reads from PPU data automatically increment the nametable
			// address depending upon the mode set in the control register.
			// If set to vertical mode, the increment is 32, so it skips
			// one whole nametable row; in horizontal mode it just increments
			// by 1, moving to the next column
			vram_addr.reg += (control.increment_mode ? 32 : 1);
			break;
		}
	}

	return data;
}

void ppu::cpuWrite(uint16_t address, uint8_t data) {
    switch (addr)
	{
	case 0x0000: // Control
		control.reg = data;
		tram_addr.nametable_x = control.nametable_x;
		tram_addr.nametable_y = control.nametable_y;
		break;
	case 0x0001: // Mask
		mask.reg = data;
		break;
	case 0x0002: // Status
		break;
	case 0x0003: // OAM Address
		break;
	case 0x0004: // OAM Data
		break;
	case 0x0005: // Scroll
		if (address_latch == 0)
		{
			// First write to scroll register contains X offset in pixel space
			// which we split into coarse and fine x values
			fine_x = data & 0x07;
			tram_addr.coarse_x = data >> 3;
			address_latch = 1;
		}
		else
		{
			// First write to scroll register contains Y offset in pixel space
			// which we split into coarse and fine Y values
			tram_addr.fine_y = data & 0x07;
			tram_addr.coarse_y = data >> 3;
			address_latch = 0;
		}
		break;
	case 0x0006: // PPU Address
		if (address_latch == 0)
		{
			// PPU address bus can be accessed by CPU via the ADDR and DATA
			// registers. The fisrt write to this register latches the high byte
			// of the address, the second is the low byte. Note the writes
			// are stored in the tram register...
			tram_addr.reg = (uint16_t)((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF);
			address_latch = 1;
		}
		else
		{
			// ...when a whole address has been written, the internal vram address
			// buffer is updated. Writing to the PPU is unwise during rendering
			// as the PPU will maintam the vram address automatically whilst
			// rendering the scanline position.
			tram_addr.reg = (tram_addr.reg & 0xFF00) | data;
			vram_addr = tram_addr;
			address_latch = 0;
		}
		break;
	case 0x0007: // PPU Data
		ppuWrite(vram_addr.reg, data);
		// All writes from PPU data automatically increment the nametable
		// address depending upon the mode set in the control register.
		// If set to vertical mode, the increment is 32, so it skips
		// one whole nametable row; in horizontal mode it just increments
		// by 1, moving to the next column
		vram_addr.reg += (control.increment_mode ? 32 : 1);
		break;
	}
}

uint8_t ppu::ppuRead(uint16_t address, bool readonly) {
	uint8_t data = 0x00;
	addr &= 0x3FFF;

	if (cart->ppuRead(addr, data))
	{

	}
	else if (addr >= 0x0000 && addr <= 0x1FFF)
	{
		// If the cartridge cant map the address, have
		// a physical location ready here
		data = tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF];
	}
	else if (addr >= 0x2000 && addr <= 0x3EFF)
	{
		addr &= 0x0FFF;

		if (cart->mirror == Cartridge::MIRROR::VERTICAL)
		{
			// Vertical
			if (addr >= 0x0000 && addr <= 0x03FF)
				data = tblName[0][addr & 0x03FF];
			if (addr >= 0x0400 && addr <= 0x07FF)
				data = tblName[1][addr & 0x03FF];
			if (addr >= 0x0800 && addr <= 0x0BFF)
				data = tblName[0][addr & 0x03FF];
			if (addr >= 0x0C00 && addr <= 0x0FFF)
				data = tblName[1][addr & 0x03FF];
		}
		else if (cart->mirror == Cartridge::MIRROR::HORIZONTAL)
		{
			// Horizontal
			if (addr >= 0x0000 && addr <= 0x03FF)
				data = tblName[0][addr & 0x03FF];
			if (addr >= 0x0400 && addr <= 0x07FF)
				data = tblName[0][addr & 0x03FF];
			if (addr >= 0x0800 && addr <= 0x0BFF)
				data = tblName[1][addr & 0x03FF];
			if (addr >= 0x0C00 && addr <= 0x0FFF)
				data = tblName[1][addr & 0x03FF];
		}
	}
	else if (addr >= 0x3F00 && addr <= 0x3FFF)
	{
		addr &= 0x001F;
		if (addr == 0x0010) addr = 0x0000;
		if (addr == 0x0014) addr = 0x0004;
		if (addr == 0x0018) addr = 0x0008;
		if (addr == 0x001C) addr = 0x000C;
		data = tblPalette[addr] & (mask.grayscale ? 0x30 : 0x3F);
	}

	return data;
}

void ppu::ppuWrite(uint16_t address, uint8_t data) {
	addr &= 0x3FFF;

	if (cart->ppuWrite(addr, data))
	{

	}
	else if (addr >= 0x0000 && addr <= 0x1FFF)
	{
		tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF] = data;
	}
	else if (addr >= 0x2000 && addr <= 0x3EFF)
	{
		addr &= 0x0FFF;
		if (cart->mirror == Cartridge::MIRROR::VERTICAL)
		{
			// Vertical
			if (addr >= 0x0000 && addr <= 0x03FF)
				tblName[0][addr & 0x03FF] = data;
			if (addr >= 0x0400 && addr <= 0x07FF)
				tblName[1][addr & 0x03FF] = data;
			if (addr >= 0x0800 && addr <= 0x0BFF)
				tblName[0][addr & 0x03FF] = data;
			if (addr >= 0x0C00 && addr <= 0x0FFF)
				tblName[1][addr & 0x03FF] = data;
		}
		else if (cart->mirror == Cartridge::MIRROR::HORIZONTAL)
		{
			// Horizontal
			if (addr >= 0x0000 && addr <= 0x03FF)
				tblName[0][addr & 0x03FF] = data;
			if (addr >= 0x0400 && addr <= 0x07FF)
				tblName[0][addr & 0x03FF] = data;
			if (addr >= 0x0800 && addr <= 0x0BFF)
				tblName[1][addr & 0x03FF] = data;
			if (addr >= 0x0C00 && addr <= 0x0FFF)
				tblName[1][addr & 0x03FF] = data;
		}
	}
	else if (addr >= 0x3F00 && addr <= 0x3FFF)
	{
		addr &= 0x001F;
		if (addr == 0x0010) addr = 0x0000;
		if (addr == 0x0014) addr = 0x0004;
		if (addr == 0x0018) addr = 0x0008;
		if (addr == 0x001C) addr = 0x000C;
		tblPalette[addr] = data;
	}
}

void ppu::ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge) {
    this->cart = cartridge;
}

void ppu::reset() {
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
	bg_shifter_attrib_lo = 0x0000;
	bg_shifter_attrib_hi = 0x0000;
	status.reg = 0x00;
	mask.reg = 0x00;
	control.reg = 0x00;
	vram_addr.reg = 0x0000;
	tram_addr.reg = 0x0000;
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
