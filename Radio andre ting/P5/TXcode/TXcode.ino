// cc1101_tx.ino - MAX SPEED (500 kBaud)
// ESP32 (VSPI) + CC1101 (433MHz)
// PINS: SCK=18, MISO=19, MOSI=23, CSN=5, GDO0=17

#include <Arduino.h>
#include <SPI.h>

// --- HARDWARE PINS ---
#define PIN_SCK   18
#define PIN_MISO  19
#define PIN_MOSI  23
#define PIN_CSN   5
#define PIN_GDO0  17

// --- SPI SETTINGS ---
const SPISettings spi_settings(8000000, MSBFIRST, SPI_MODE0); // Max SPI speed (8MHz)

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
#define CC1101_TEST0    0x2E
#define CC1101_PARTNUM  0x30
#define CC1101_VERSION  0x31
#define CC1101_PATABLE  0x3E
#define CC1101_TXFIFO   0x3F

// --- STROBES ---
#define SRES    0x30
#define STX     0x35
#define SIDLE   0x36
#define SFTX    0x3B
#define SNOP    0x3D

// --- Testing Pin ---
#define testPin 2

// --- HELPER FUNCTIONS ---
void spi_write_reg(uint8_t addr, uint8_t value) {
    SPI.beginTransaction(spi_settings);
    digitalWrite(PIN_CSN, LOW);
    while(digitalRead(PIN_MISO) == HIGH); 
    SPI.transfer(addr);
    SPI.transfer(value);
    digitalWrite(PIN_CSN, HIGH);
    SPI.endTransaction();
}

void spi_write_burst(uint8_t addr, uint8_t *buf, uint8_t len) {
    SPI.beginTransaction(spi_settings);
    digitalWrite(PIN_CSN, LOW);
    while(digitalRead(PIN_MISO) == HIGH);
    SPI.transfer(addr | 0x40); 
    for(uint8_t i=0; i<len; i++) SPI.transfer(buf[i]);
    digitalWrite(PIN_CSN, HIGH);
    SPI.endTransaction();
}

void spi_strobe(uint8_t cmd) {
    SPI.beginTransaction(spi_settings);
    digitalWrite(PIN_CSN, LOW);
    while(digitalRead(PIN_MISO) == HIGH);
    SPI.transfer(cmd);
    digitalWrite(PIN_CSN, HIGH);
    SPI.endTransaction();
}

uint8_t spi_read_reg(uint8_t addr) {
    SPI.beginTransaction(spi_settings);
    digitalWrite(PIN_CSN, LOW);
    while(digitalRead(PIN_MISO) == HIGH);
    SPI.transfer(addr | 0x80);
    uint8_t val = SPI.transfer(0x00);
    digitalWrite(PIN_CSN, HIGH);
    SPI.endTransaction();
    return val;
}

// --- RESET SEQUENCE ---
void cc1101_reset() {
    digitalWrite(PIN_CSN, HIGH);
    delayMicroseconds(40);
    digitalWrite(PIN_CSN, LOW);
    delay(10);
    digitalWrite(PIN_CSN, HIGH);
    delayMicroseconds(40);
    
    digitalWrite(PIN_CSN, LOW);
    unsigned long t0 = millis();
    while(digitalRead(PIN_MISO) == HIGH) {
        if(millis() - t0 > 100) {
            Serial.println("HARDWARE ERROR: MISO stuck HIGH during reset!");
            return;
        }
    }
    
    SPI.beginTransaction(spi_settings);
    SPI.transfer(SRES);
    SPI.endTransaction();
    
    while(digitalRead(PIN_MISO) == HIGH); 
    digitalWrite(PIN_CSN, HIGH);
    Serial.println("Reset sequence complete.");
}

// --- CONFIGURATION (500 kBaud - MSK) ---
void cc1101_init() {
    spi_strobe(SRES);
    delay(10);
    
    spi_write_reg(CC1101_IOCFG0,   0x06); 
    spi_write_reg(CC1101_FIFOTHR,  0x47);
    
    // Fixed Packet Length Mode, 10 bytes
    spi_write_reg(CC1101_PKTCTRL0, 0x00); 
    spi_write_reg(0x06,            10);   // PKTLEN = 10
    
    // 433.92 MHz
    spi_write_reg(CC1101_FREQ2,    0x10);
    spi_write_reg(CC1101_FREQ1,    0xA7);
    spi_write_reg(CC1101_FREQ0,    0x62);
    
    // --- 500 kBaud Settings (MSK Modulation) ---
    // RX Filter Bandwidth = 812 kHz (Max)
    // Data Rate = 500 kBaud
    spi_write_reg(CC1101_FSCTRL1,  0x0C); // IF freq intermediate
    spi_write_reg(CC1101_MDMCFG4,  0x0E); // RX BW 812kHz, DRATE_E = 14
    spi_write_reg(CC1101_MDMCFG3,  0x3B); // DRATE_M = 59 -> 499.8 kBaud
    spi_write_reg(CC1101_MDMCFG2,  0x73); // MSK Modulation + 30/32 Sync
    spi_write_reg(CC1101_DEVIATN,  0x00); // Not used for MSK, but good practice to clear
    
    // High speed specific tuning
    spi_write_reg(CC1101_MCSM0,    0x18); // Calibrate from IDLE to RX/TX
    spi_write_reg(CC1101_FOCCFG,   0x1D); // Frequency Offset Comp (High setting for high BW)
    spi_write_reg(CC1101_BSCFG,    0x1C); // Bit sync config
    
    // AGC settings for high bandwidth
    spi_write_reg(CC1101_AGCCTRL2, 0xC7); 
    spi_write_reg(CC1101_AGCCTRL1, 0x00);
    spi_write_reg(CC1101_AGCCTRL0, 0xB2);
    
    // Calibration
    spi_write_reg(CC1101_FSCAL3,   0xE9);
    spi_write_reg(CC1101_FSCAL2,   0x2A);
    spi_write_reg(CC1101_FSCAL1,   0x00);
    spi_write_reg(CC1101_FSCAL0,   0x1F);
    
    spi_write_reg(CC1101_TEST2,    0x81);
    spi_write_reg(CC1101_TEST1,    0x35);
    
    // PA Table (Max Power)
    spi_write_reg(CC1101_PATABLE,  0xC0);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- CC1101 TX MAX SPEED (500k) ---");

    pinMode(testPin, OUTPUT);

    pinMode(PIN_CSN, OUTPUT);
    pinMode(PIN_GDO0, INPUT);
    pinMode(PIN_MISO, INPUT_PULLUP); 
    digitalWrite(PIN_CSN, HIGH);

    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CSN);
    
    cc1101_reset();

    // HARDWARE CHECK
    uint8_t version = spi_read_reg(CC1101_VERSION);
    if (version == 0x00 || version == 0xFF || version == 0x5F) {
        Serial.println("CRITICAL FAILURE: Chip not responding.");
        while(1); 
    }

    cc1101_init();
    Serial.println("Radio Configured (500 kBaud).");
}

void loop() {
    char msg[11]; 
    
    digitalWrite(testPin, HIGH);
    unsigned long testTimer = micros();
    
    spi_strobe(SIDLE);
    spi_strobe(SFTX);
    spi_write_burst(CC1101_TXFIFO, (uint8_t*)msg, 10);
    spi_strobe(STX);
    
    unsigned long tStart = millis();
    while(digitalRead(PIN_GDO0) == LOW) {
        if(millis() - tStart > 20) break; // Tighter timeout for high speed
    }
    while(digitalRead(PIN_GDO0) == HIGH);
    testTimer = micros() - testTimer;
    
    Serial.print("[testTimer: "); Serial.print(testTimer); Serial.println(" us]");
    
    Serial.printf("Sent: %s\n", msg);
    Serial.flush();
    
    digitalWrite(testPin, LOW);

    //delay(100); // Very short delay to demonstrate speed
}
