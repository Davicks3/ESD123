#include "CC1101_Packet.h"

void CC1101_Packet::begin() {
    pinMode(cs, OUTPUT);
    csHigh();

    SPI.begin();   // ESP32: SCK=18, MISO=19, MOSI=23 by default
    reset();
    applyConfig_433_fast();
}

void CC1101_Packet::reset() {
    csHigh();
    delayMicroseconds(5);
    csLow();
    delayMicroseconds(10);
    csHigh();
    delayMicroseconds(40);

    csLow();
    SPI.transfer(CC1101_SRES);
    csHigh();
    delay(1);
}

void CC1101_Packet::writeReg(uint8_t addr, uint8_t val) {
    csLow();
    SPI.transfer(addr);
    SPI.transfer(val);
    csHigh();
}

uint8_t CC1101_Packet::readReg(uint8_t addr) {
    csLow();
    SPI.transfer(addr | 0x80);
    uint8_t v = SPI.transfer(0);
    csHigh();
    return v;
}

void CC1101_Packet::writeBurst(uint8_t addr, const uint8_t* data, uint8_t len) {
    csLow();
    SPI.transfer(addr | 0x40);
    for (uint8_t i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }
    csHigh();
}

void CC1101_Packet::readBurst(uint8_t addr, uint8_t* data, uint8_t len) {
    csLow();
    SPI.transfer(addr | 0xC0);
    for (uint8_t i = 0; i < len; i++) {
        data[i] = SPI.transfer(0);
    }
    csHigh();
}

void CC1101_Packet::applyConfig_433_fast() {
    // GDO2: sync word / packet received, GDO0: chip ready
    writeReg(IOCFG2, 0x06);
    writeReg(IOCFG0, 0x29);

    writeReg(FIFOTHR, 0x47);

    // Sync word (can be anything, just match on both ends)
    writeReg(SYNC1, 0xD3);
    writeReg(SYNC0, 0x91);

    // Packet control:
    //  - No address check
    //  - No CRC (to keep packet short)
    //  - Variable length, first byte is length
    writeReg(PKTCTRL1, 0x00);
    writeReg(PKTCTRL0, 0x01);   // CRC off, variable length

    // 433.92 MHz frequency (example values)
    writeReg(FSCTRL1, 0x06);
    writeReg(FREQ2,  0x10);
    writeReg(FREQ1,  0xB0);
    writeReg(FREQ0,  0x71);

    // === Data rate / filter ===
    // For now, use a known-good 250 kBaud GFSK-like config.
    // Later: generate 500 kBaud MSK in SmartRF and replace MDMCFG4/3/DEVIATN.
    writeReg(MDMCFG4, 0x2D);  // 250 kBaud example
    writeReg(MDMCFG3, 0x3B);

    // Modem config: GFSK, data/whitening off, sync on (16/16)
    writeReg(MDMCFG2, 0x12);  // SYNC_MODE=2, MOD_FORMAT=GFSK, no Manchester

    // MDMCFG1: NUM_PREAMBLE=2 bytes (minimum), other bits as default-ish
    // NUM_PREAMBLE = 000 -> 2 bytes 
    writeReg(MDMCFG1, 0x02);
    writeReg(MDMCFG0, 0xF8);

    // Deviation (ok for 250 kBaud)
    writeReg(DEVIATN, 0x62);

    // Main state machine:
    // After TX, go to IDLE; after RX, stay in RX.
    writeReg(MCSM1, 0x3F);   // CCA off, RX->RX
    writeReg(MCSM0, 0x18);   // FS_AUTOCAL = from IDLE to RX/TX

    // Frequency offset / AFC
    writeReg(FOCCFG,  0x1D);
    writeReg(BSCFG,   0x1C);

    // AGC
    writeReg(AGCCTRL2, 0xC7);
    writeReg(AGCCTRL1, 0x00);
    writeReg(AGCCTRL0, 0xB2);

    writeReg(WORCTRL, 0xFB);

    // Synth calibration
    writeReg(FSCAL3, 0xEA);
    writeReg(FSCAL2, 0x2A);
    writeReg(FSCAL1, 0x00);
    writeReg(FSCAL0, 0x11);

    // Test regs
    writeReg(TEST2, 0x81);
    writeReg(TEST1, 0x35);
    writeReg(TEST0, 0x09);

    enterIdle();
}

void CC1101_Packet::enterRx() {
    csLow();
    SPI.transfer(CC1101_SRX);
    csHigh();
}

void CC1101_Packet::enterIdle() {
    csLow();
    SPI.transfer(CC1101_SIDLE);
    csHigh();
}

// --- TX: send 1 payload byte as a tiny packet ---

void CC1101_Packet::sendByte(uint8_t b) {
    // Go idle, flush TX FIFO
    enterIdle();
    csLow();
    SPI.transfer(CC1101_SFTX);
    csHigh();

    // Length + payload
    uint8_t buf[2] = { 1, b };
    writeBurst(TXFIFO, buf, 2);

    // Strobe TX
    csLow();
    SPI.transfer(CC1101_STX);
    csHigh();

    // Wait until TX FIFO is empty (packet sent)
    // (you could also poll GDO bits if you want)
    /*
    while (true) {
        uint8_t txb = readReg(TXBYTES);
        if ((txb & 0x7F) == 0) break;
    }
    */
}

// --- RX: try to read 1-byte packet, non-blocking ---
// returns 1 if a byte was received, 0 otherwise.
int CC1101_Packet::receiveByte(uint8_t* out) {
    // Make sure we're in RX
    enterRx();

    uint8_t rxbytes = readReg(RXBYTES);
    uint8_t count   = rxbytes & 0x7F;
    if (count < 2) {
        return 0;
    }

    uint8_t buf[2];
    readBurst(RXFIFO, buf, 2);  // length + one data byte

    if (buf[0] != 1) {
        // Unexpected length, flush FIFO
        enterIdle();
        csLow(); SPI.transfer(CC1101_SFRX); csHigh();
        enterRx();
        return 0;
    }

    *out = buf[1];
    return 1;
}
