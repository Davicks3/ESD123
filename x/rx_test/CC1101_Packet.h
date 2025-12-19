#pragma once
#include <Arduino.h>
#include <SPI.h>

// --- CC1101 command strobes ---
#define CC1101_SRES     0x30
#define CC1101_SFSTXON  0x31
#define CC1101_SXOFF    0x32
#define CC1101_SCAL     0x33
#define CC1101_SRX      0x34
#define CC1101_STX      0x35
#define CC1101_SIDLE    0x36
#define CC1101_SFRX     0x3A
#define CC1101_SFTX     0x3B

// --- registers we touch ---
#define IOCFG2   0x00
#define IOCFG0   0x02
#define FIFOTHR  0x03
#define SYNC1    0x04
#define SYNC0    0x05
#define PKTCTRL1 0x07
#define PKTCTRL0 0x08
#define FSCTRL1  0x0B
#define FREQ2    0x0D
#define FREQ1    0x0E
#define FREQ0    0x0F
#define MDMCFG4  0x10
#define MDMCFG3  0x11
#define MDMCFG2  0x12
#define MDMCFG1  0x13
#define MDMCFG0  0x14
#define DEVIATN  0x15
#define MCSM1    0x17
#define MCSM0    0x18
#define FOCCFG   0x19
#define BSCFG    0x1A
#define AGCCTRL2 0x1B
#define AGCCTRL1 0x1C
#define AGCCTRL0 0x1D
#define WORCTRL  0x20
#define FSCAL3   0x23
#define FSCAL2   0x24
#define FSCAL1   0x25
#define FSCAL0   0x26
#define TEST2    0x2C
#define TEST1    0x2D
#define TEST0    0x2E

#define TXFIFO   0x3F
#define RXFIFO   0x3F

#define RXBYTES  0x3B
#define TXBYTES  0x3A

class CC1101_Packet {
public:
    explicit CC1101_Packet(uint8_t csPin) : cs(csPin) {}

    void begin();                       // init SPI + radio
    void enterRx();                     // permanent RX
    void enterIdle();                   // SIDLE

    // minimal 1-byte payload send, blocking
    void sendByte(uint8_t b);

    // receive function: returns number of payload bytes (0 or 1 here)
    int receiveByte(uint8_t* out);

private:
    uint8_t cs;

    void csLow()  { digitalWrite(cs, LOW);  }
    void csHigh() { digitalWrite(cs, HIGH); }

    void reset();
    void writeReg(uint8_t addr, uint8_t val);
    uint8_t readReg(uint8_t addr);
    void writeBurst(uint8_t addr, const uint8_t* data, uint8_t len);
    void readBurst(uint8_t addr, uint8_t* data, uint8_t len);

    void applyConfig_433_fast();  // 433 MHz, high-speed, tiny packets
};
