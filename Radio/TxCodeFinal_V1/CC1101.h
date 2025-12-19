#ifndef CC1101_H
#define CC1101_H

#include <Arduino.h>
#include <SPI.h>

// --- REGISTERS ---
#define CC1101_IOCFG0   0x02
#define CC1101_FIFOTHR  0x03
#define CC1101_PKTCTRL0 0x08
#define CC1101_FSCTRL1  0x0B
#define CC1101_FREQ2    0x0D
#define CC1101_FREQ1    0x0E
#define CC1101_FREQ0    0x0F
#define CC1101_MDMCFG4  0x10
#define CC1101_MDMCFG3  0x11
#define CC1101_MDMCFG2  0x12
#define CC1101_DEVIATN  0x15
#define CC1101_MCSM0    0x18
#define CC1101_FOCCFG   0x19
#define CC1101_BSCFG    0x1A
#define CC1101_AGCCTRL2 0x1B
#define CC1101_AGCCTRL1 0x1C
#define CC1101_AGCCTRL0 0x1D
#define CC1101_FSCAL3   0x23
#define CC1101_FSCAL2   0x24
#define CC1101_FSCAL1   0x25
#define CC1101_FSCAL0   0x26
#define CC1101_TEST2    0x2C
#define CC1101_TEST1    0x2D
#define CC1101_VERSION  0x31
#define CC1101_PATABLE  0x3E
#define CC1101_TXFIFO   0x3F
#define CC1101_RXFIFO   0x3F

// --- STROBES ---
#define SRES    0x30
#define SRX     0x34
#define STX     0x35
#define SIDLE   0x36
#define SFRX    0x3A
#define SFTX    0x3B

class CC1101 {
  private:
    uint8_t _csn, _gdo0, _miso, _mosi, _sck;
    SPISettings _spiSettings;

    void _waitMiso() {
      while (digitalRead(_miso) == HIGH);
    }

    void _writeReg(uint8_t addr, uint8_t value) {
      SPI.beginTransaction(_spiSettings);
      digitalWrite(_csn, LOW);
      _waitMiso();
      SPI.transfer(addr);
      SPI.transfer(value);
      digitalWrite(_csn, HIGH);
      SPI.endTransaction();
    }

    uint8_t _readReg(uint8_t addr) {
      SPI.beginTransaction(_spiSettings);
      digitalWrite(_csn, LOW);
      _waitMiso();
      SPI.transfer(addr | 0x80);
      uint8_t val = SPI.transfer(0x00);
      digitalWrite(_csn, HIGH);
      SPI.endTransaction();
      return val;
    }

    void _readBurst(uint8_t addr, uint8_t *buf, uint8_t len) {
      SPI.beginTransaction(_spiSettings);
      digitalWrite(_csn, LOW);
      _waitMiso();
      SPI.transfer(addr | 0xC0);
      for (uint8_t i = 0; i < len; i++) buf[i] = SPI.transfer(0x00);
      digitalWrite(_csn, HIGH);
      SPI.endTransaction();
    }

    void _writeBurst(uint8_t addr, uint8_t *buf, uint8_t len) {
      SPI.beginTransaction(_spiSettings);
      digitalWrite(_csn, LOW);
      _waitMiso();
      SPI.transfer(addr | 0x40);
      for (uint8_t i = 0; i < len; i++) SPI.transfer(buf[i]);
      digitalWrite(_csn, HIGH);
      SPI.endTransaction();
    }

    void _strobe(uint8_t cmd) {
      SPI.beginTransaction(_spiSettings);
      digitalWrite(_csn, LOW);
      _waitMiso();
      SPI.transfer(cmd);
      digitalWrite(_csn, HIGH);
      SPI.endTransaction();
    }

    void _reset() {
      digitalWrite(_csn, HIGH);
      delayMicroseconds(40);
      digitalWrite(_csn, LOW);
      delay(10);
      digitalWrite(_csn, HIGH);
      delayMicroseconds(40);

      digitalWrite(_csn, LOW);
      unsigned long t0 = millis();
      while (digitalRead(_miso) == HIGH) {
        if (millis() - t0 > 100) return; // Timeout
      }

      SPI.beginTransaction(_spiSettings);
      SPI.transfer(SRES);
      SPI.endTransaction();

      while (digitalRead(_miso) == HIGH);
      digitalWrite(_csn, HIGH);
    }

    void _configure() {
      _strobe(SRES);
      delay(10);

      _writeReg(CC1101_IOCFG0,   0x06);
      _writeReg(CC1101_FIFOTHR,  0x47);

      // Fixed Packet Length Mode, 10 bytes
      _writeReg(CC1101_PKTCTRL0, 0x00);
      _writeReg(0x06,            10);   // PKTLEN = 10

      // 433.92 MHz
      _writeReg(CC1101_FREQ2,    0x10);
      _writeReg(CC1101_FREQ1,    0xA7);
      _writeReg(CC1101_FREQ0,    0x62);

      // --- 500 kBaud Settings (MSK Modulation) ---
      _writeReg(CC1101_FSCTRL1,  0x0C);
      _writeReg(CC1101_MDMCFG4,  0x0E); // RX BW 812kHz
      _writeReg(CC1101_MDMCFG3,  0x3B); // 500 kBaud
      _writeReg(CC1101_MDMCFG2,  0x73); // MSK
      _writeReg(CC1101_DEVIATN,  0x00);

      _writeReg(CC1101_MCSM0,    0x18);
      _writeReg(CC1101_FOCCFG,   0x1D);
      _writeReg(CC1101_BSCFG,    0x1C);

      _writeReg(CC1101_AGCCTRL2, 0xC7);
      _writeReg(CC1101_AGCCTRL1, 0x00);
      _writeReg(CC1101_AGCCTRL0, 0xB2);

      // Calibration
      _writeReg(CC1101_FSCAL3,   0xE9);
      _writeReg(CC1101_FSCAL2,   0x2A);
      _writeReg(CC1101_FSCAL1,   0x00);
      _writeReg(CC1101_FSCAL0,   0x1F);
      _writeReg(CC1101_TEST2,    0x81);
      _writeReg(CC1101_TEST1,    0x35);
      
      _writeReg(CC1101_PATABLE,  0x34);
    }

  public:
    CC1101(uint8_t csn, uint8_t gdo0, uint8_t miso, uint8_t mosi, uint8_t sck) 
      : _csn(csn), _gdo0(gdo0), _miso(miso), _mosi(mosi), _sck(sck), 
        _spiSettings(8000000, MSBFIRST, SPI_MODE0) {}

    bool begin() {
      pinMode(_csn, OUTPUT);
      pinMode(_gdo0, INPUT);
      pinMode(_miso, INPUT);
      digitalWrite(_csn, HIGH);

      SPI.begin(_sck, _miso, _mosi, _csn);
      
      _reset(); delay(100);

      // Hardware Check
      uint8_t version = _readReg(CC1101_VERSION);
      if (version == 0x00 || version == 0xFF || version == 0x5F) {
        return false; // Hardware failure
      }

      _configure();
      return true;
    }

    void setRx() {
      _strobe(SRX);
    }

    // Now accepts rssi as a reference
    bool checkPacket(uint8_t* buf, uint8_t& len, int8_t& rssi) {
      if (digitalRead(_gdo0) == HIGH) {
        while (digitalRead(_gdo0) == HIGH); // Wait for packet end

        // Read 12 bytes (10 payload + 2 status)
        uint8_t tempBuf[12];
        _readBurst(CC1101_RXFIFO, tempBuf, 12);

        // Copy payload
        len = 10;
        memcpy(buf, tempBuf, len);

        // Calculate RSSI from 11th byte
        int8_t raw_rssi = (int8_t)tempBuf[10];
        if (raw_rssi >= 128) rssi = (raw_rssi - 256) / 2 - 74;
        else rssi = raw_rssi / 2 - 74;

        _strobe(SIDLE);
        _strobe(SFRX); // Flush RX
        _strobe(SRX);  // Back to RX
        return true;
      }
      return false;
    }

    void sendPacket(uint8_t* buf, uint8_t len) {
      _strobe(SIDLE);
      _strobe(SFTX);
      
      // Create temp buffer to ensure we only send 10 bytes
      uint8_t txBuf[10];
      memset(txBuf, 0, 10);
      uint8_t copyLen = (len > 10) ? 10 : len;
      memcpy(txBuf, buf, copyLen);

      _writeBurst(CC1101_TXFIFO, txBuf, 10);
      _strobe(STX);

      // Wait for GDO0 High (Sync)
      unsigned long t0 = millis();
      while (digitalRead(_gdo0) == LOW) {
        if (millis() - t0 > 50) break; // Timeout
      }
      // Wait for GDO0 Low (End)
      while (digitalRead(_gdo0) == HIGH);
    }
};

#endif