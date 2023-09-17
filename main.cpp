// main.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
//#include <unistd.h>
#include "crc.h"
#include "i2c.h"
#include "gff.h"

struct FlashDesc
{
    const char* device_name;
    uint32_t    jedec_id;
    uint32_t    size_kb;
    uint32_t    page_size;
    uint32_t    block_size_kb;
};

static const FlashDesc FlashDevices[] =
{
    // name,        Jedec ID,    sizeK, page size, block sizeK
    {"AT25DF041A", 0x1F4401,      512,       256, 64},
    {"AT25DF161", 0x1F4602, 2 * 1024,       256, 64},
    {"AT26DF081A", 0x1F4501, 1 * 1024,       256, 64},
    {"AT26DF0161", 0x1F4600, 2 * 1024,       256, 64},
    {"AT26DF161A", 0x1F4601, 2 * 1024,       256, 64},
    {"AT25DF321",  0x1F4701, 4 * 1024,       256, 64},
    {"AT25DF512B", 0x1F6501,       64,       256, 32},
    {"AT25DF512B", 0x1F6500,       64,       256, 32},
    {"AT25DF021", 0x1F3200,      256,       256, 64},
    {"AT26DF641",  0x1F4800, 8 * 1024,       256, 64},
    // Manufacturer: ST
    {"M25P05", 0x202010,       64,       256, 32},
    {"M25P10", 0x202011,      128,       256, 32},
    {"M25P20", 0x202012,      256,       256, 64},
    {"M25P40", 0x202013,      512,       256, 64},
    {"M25P80", 0x202014, 1 * 1024,       256, 64},
    {"M25P16", 0x202015, 2 * 1024,       256, 64},
    {"M25P32", 0x202016, 4 * 1024,       256, 64},
    {"M25P64", 0x202017, 8 * 1024,       256, 64},
    // Manufacturer: Windbond
    {"W25X10", 0xEF3011,      128,       256, 64},
    {"W25X20", 0xEF3012,      256,       256, 64},
    {"W25X40", 0xEF3013,      512,       256, 64},
    {"W25X80", 0xEF3014, 1 * 1024,       256, 64},
    {"W25Q80", 0xEF4014, 1 * 1024,       256, 64},
    {"GD25Q80", 0xC84014, 1 * 1024,      256, 64}, 
    // Manufacturer: Macronix
    {"MX25L512", 0xC22010,       64,       256, 64},
    {"25D40",    0xC22013,      512,       256, 64},
    {"MX25L3205", 0xC22016, 4 * 1024,       256, 64},
    {"MX25L6405", 0xC22017, 8 * 1024,       256, 64},
    {"MX25L8005", 0xC22014,     1024,       256, 64},
    // Microchip
    {"SST25VF512", 0xBF4800,       64,       256, 32},
    {"SST25VF032", 0xBF4A00, 4 * 1024,       256, 32},
    // PMC
	{"PM25LQ010B", 0x7F9D21,       128,       256, 64},
	// FM
    {"FM25F04", 0xA14013,    512,       256, 64},
    {NULL, 0, 0, 0, 0}
};

enum ECommondCommandType
{
    E_CC_NOOP = 0,
    E_CC_WRITE = 1,
    E_CC_READ = 2,
    E_CC_WRITE_AFTER_WREN = 3,
    E_CC_WRITE_AFTER_EWSR = 4,
    E_CC_ERASE = 5
};

//SPICommonCommand(E_CC_READ, 0x9f, 3, 0, 0);
uint32_t SPICommonCommand(ECommondCommandType cmd_type,
                          uint8_t cmd_code,
                          uint8_t num_reads,
                          uint8_t num_writes,
                          uint32_t write_value)
{
    num_reads &= 3;
    num_writes &= 3;
    write_value &= 0xFFFFFF;
    uint8_t reg_value = (cmd_type << 5) |
                        (num_writes << 3) |
                        (num_reads << 1);

    WriteReg(0x60, reg_value);
    WriteReg(0x61, cmd_code);
    switch (num_writes)
    {
    case 3:
        WriteReg(0x64, write_value >> 16);
        WriteReg(0x65, write_value >> 8);
        WriteReg(0x66, write_value);
        break;
    case 2:
        WriteReg(0x64, write_value >> 8);
        WriteReg(0x65, write_value);
        break;
    case 1:
        WriteReg(0x64, write_value);
        break;
    }
    WriteReg(0x60, reg_value | 1); // Execute the command

    uint8_t b;
    do
    {
        b = ReadReg(0x60);
    }
    while (b & 1);    // TODO: add timeout and reset the controller

    switch (num_reads)
    {
    case 0:
        return 0;
    case 1:
        return ReadReg(0x67);
    case 2:
        return (ReadReg(0x67) << 8) | ReadReg(0x68);
    case 3:
        return (ReadReg(0x67) << 16) | (ReadReg(0x68) << 8) | ReadReg(0x69);
    }
    return 0;
}

void SPIRead(uint32_t address, uint8_t *data, int32_t len)
{
    WriteReg(0x60, 0x46);
    WriteReg(0x61, 0x3);
    WriteReg(0x64, address>>16);
    WriteReg(0x65, address>>8);
    WriteReg(0x66, address);
    WriteReg(0x60, 0x47); // Execute the command
    uint8_t b;
    do
    {
        b = ReadReg(0x60);
    }
    while (b & 1);    // TODO: add timeout and reset the controller
    while (len > 0)
    {
        int32_t read_len = len;
        if (read_len > 128)
            read_len = 128;
        ReadBytesFromAddr(0x70, data, read_len);
        data += read_len;
        len -= read_len;
    }
}

void PrintManufacturer(uint32_t id)
{
    switch (id)
    {
    case 0x20:
        fprintf(stderr, "ST");
        break;
    case 0xef:
        fprintf(stderr, "Winbond");
        break;
    case 0x1f:
        fprintf(stderr, "Atmel");
        break;
    case 0xc2:
        fprintf(stderr, "Macronix");
        break;
    case 0xbf:
        fprintf(stderr, "Microchip");
        break;
    default:
        fprintf(stderr, "Unknown");
        break;
    }
}

static const FlashDesc* FindChip(uint32_t jedec_id)
{
    const FlashDesc* chip = FlashDevices;
    while (chip->jedec_id != 0)
    {
        if (chip->jedec_id == jedec_id)
            return chip;
        chip++;
    }
    return NULL;
}

uint8_t SPIComputeCRC(uint32_t start, uint32_t end)
{
    WriteReg(0x64, start >> 16);
    WriteReg(0x65, start >> 8);
    WriteReg(0x66, start);

    WriteReg(0x72, end >> 16);
    WriteReg(0x73, end >> 8);
    WriteReg(0x74, end);

    WriteReg(0x6f, 0x84);
    uint8_t b;
    do
    {
        b = ReadReg(0x6f);
    }
    while (!(b & 0x2));    // TODO: add timeout and reset the controller
    return ReadReg(0x75);
}

uint8_t GetManufacturerId(uint32_t jedec_id)
{
    return jedec_id >> 16;
}

void SetupChipCommands(uint32_t jedec_id)
{
    uint8_t manufacturer_id = GetManufacturerId(jedec_id);
    switch (manufacturer_id)
    {
    case 0xEF:
    case 0xC2:	// Add Taka
    case 0xC8:	// Add Taka
        // These are the codes for Winbond
        WriteReg(0x62, 0x06); // Flash Write enable op code
        WriteReg(0x63, 0x50); // Flash Write register op code
        WriteReg(0x6a, 0x03); // Flash Read op code.
        WriteReg(0x6b, 0x0b); // Flash Fast read op code.
        WriteReg(0x6d, 0x02); // Flash program op code.
        WriteReg(0x6e, 0x05); // Flash read status op code.
        break;
    default:
        fprintf(stderr, "Can not handle manufacturer code %02x\n", manufacturer_id);
        exit(-6);
        break;
    }
}

bool SaveFlash(const char *output_file_name, uint32_t chip_size)
{
    FILE *dump;
    uint32_t addr = 0;
	fopen_s(&dump, output_file_name, "wb");
    InitCRC();
    do
    {
        uint8_t buffer[1024];
        fprintf(stderr, "Reading addr %x\r", addr);
        SPIRead(addr, buffer, sizeof(buffer));
        fwrite(buffer, 1, sizeof(buffer), dump);
        ProcessCRC(buffer, sizeof(buffer));
        addr += sizeof(buffer);
    }
    /**
     * don't read entire flash chip but only
     * 0x3ffff bytes (256k) which corresponds
     * to found firmwares around the web
     *
     * while (addr < chip_size);
     *
     */
    //while (addr < 0x3ffff && addr < chip_size);
    while (addr < chip_size); 
    fprintf(stderr, "\ndone.\n");
    fclose(dump);
    uint8_t data_crc = GetCRC();
    //uint8_t chip_crc = SPIComputeCRC(0, chip_size - 1);
    uint8_t chip_crc = SPIComputeCRC(0, addr - 1);
    fprintf(stderr, "Received data CRC %02x\n", data_crc);
    fprintf(stderr, "Chip CRC %02x\n", chip_crc);
    return data_crc == chip_crc;
}

uint64_t GetFileSize(FILE* file)
{
    uint64_t current_pos;
    uint64_t result;
    // TODO
    /*
    current_pos = _ftelli64(file);
    fseek(file, 0, SEEK_END);
    result = _ftelli64(file);
    _fseeki64(file, current_pos, SEEK_SET);

    */
    current_pos = ftell(file);
    fseek(file, 0, SEEK_END);
    result = ftell(file);
    fseek(file, current_pos, SEEK_SET);

    return result;
}

static uint8_t* ReadFile(const char *file_name, uint32_t* size)
{
    FILE *file;
    uint8_t* result = NULL;
	fopen_s(&file, file_name, "rb");
    if (NULL == file)
    {
        fprintf(stderr, "Can't open input file %s\n", file_name);
        return result;
    }
    uint64_t file_size64 = GetFileSize(file);
    if (file_size64 > 8*1024*1024)
    {
        fprintf(stderr, "This file looks to big %lld\n", file_size64);
        fclose(file);
        return result;
    }
    uint32_t file_size = (uint32_t)file_size64;
    result = new uint8_t[file_size];
    if (NULL == result)
    {
        fprintf(stderr, "Not enough RAM.\n");
        fclose(file);
        return result;
    }
    fread(result, 1, file_size, file);
    fclose(file);
    if (memcmp("GMI GFF V1.0", result, 12) == 0)
    {
        fprintf(stderr, "Detected GFF image.\n");
        // Handle GFF file
        if (file_size < 256)
        {
            fprintf(stderr, "This file looks to small %d\n", file_size);
            delete [] result;
            return NULL;
        }
        uint32_t gff_size = ComputeGffDecodedSize(result + 256,
                            file_size - 256);
        if (gff_size == 0)
        {
            fprintf(stderr, "GFF Decoding failed for this file\n");
            delete [] result;
            return NULL;
        }
        uint8_t* gff_data = new uint8_t[gff_size];
        if (NULL == gff_data)
        {
            fprintf(stderr, "Not enough RAM.\n");
            delete [] result;
            return NULL;
        }
        DecodeGff(result + 256, file_size - 256, gff_data);
        // Replace the encoded buffer with the decoded data.
        delete [] result;
        result = gff_data;
        file_size = gff_size;
    }
    if (NULL != size)
    {
        *size = file_size;
    }
    return result;
}

static bool ShouldProgramPage(uint8_t* buffer, uint32_t size)
{
    for (uint32_t idx = 0; idx < size; ++idx)
    {
        if (buffer[idx] != 0xff)
            return true;
    }
    return false;
}

bool ProgramFlash(const char *input_file_name, uint32_t chip_size)
{
    uint32_t prog_size;
    uint8_t* prog = ReadFile(input_file_name, &prog_size);
    if (NULL == prog)
    {
        return false;
    }

	/*
	WriteReg(0xF4, 0x9F);
	fprintf(stderr, "%02X == 0x06\n", ReadReg(0xF5));

	WriteReg(0xF4, 0x9F);
	WriteReg(0xF5, 0x00);

	WriteReg(0xF4, 0x00);
	fprintf(stderr, "%02X == 0xCE\n", ReadReg(0xF5));

	WriteReg(0xF4, 0x0F);
	fprintf(stderr, "%02X == 0x23\n", ReadReg(0xF5));

	WriteReg(0xF4, 0x9F);
	WriteReg(0xF5, 0x06);
	*/

//	WriteReg(0xF4, 0x9F);
//	WriteReg(0xF5, 0x10);

	// RTD2556‚ÌWP‰ðœ
	WriteReg(0xF4, 0x29);
	fprintf(stderr, "Reg:0x29 Value=%02X\n", ReadReg(0xF5));

	WriteReg(0xF4, 0x29);
	WriteReg(0xF5, 0x01);

	WriteReg(0xF4, 0x9F);
	WriteReg(0xF5, 0xFE);

	WriteReg(0xF4, 0x19);
	fprintf(stderr, "Reg:0x19 Value=%02X\n", ReadReg(0xF5));

	WriteReg(0xF4, 0x19);
	WriteReg(0xF5, 0x01);
		
	fprintf(stderr, "Erasing...");
    fflush(stdout);
    SPICommonCommand(E_CC_WRITE_AFTER_EWSR, 1, 0, 1, 0); // Unprotect the Status Register
    SPICommonCommand(E_CC_WRITE_AFTER_WREN, 1, 0, 1, 0); // Unprotect the flash
    SPICommonCommand(E_CC_ERASE, 0xc7, 0, 0, 0);         // Chip Erase
    fprintf(stderr, "done\n");

    //RTD266x can program only 256 bytes at a time.
    uint8_t buffer[256];
    uint8_t b;
    uint32_t addr = 0;
    uint8_t* data_ptr = prog;
    uint32_t data_len = prog_size;
    InitCRC();
    do
    {
        // Wait for programming cycle to finish
        do
        {
            b = ReadReg(0x6f);
        }
        while (b & 0x40);

        fprintf(stderr, "Writing addr %x\r", addr);
        // Fill with 0xff in case we read a partial buffer.
        memset(buffer, 0xff, sizeof(buffer));
        uint32_t len = sizeof(buffer);
        if (len > data_len)
        {
            len = data_len;
        }
        memcpy(buffer, data_ptr, len);
        data_ptr += len;
        data_len -= len;

        if (ShouldProgramPage(buffer, sizeof(buffer)))
        {
            // Set program size-1
            WriteReg(0x71, 255);

            // Set the programming address
            WriteReg(0x64, addr >> 16);
            WriteReg(0x65, addr >> 8);
            WriteReg(0x66, addr);

            // Write the content to register 0x70
            // Out USB gizmo supports max 63 bytes at a time.
#if 0
            WriteBytesToAddr(0x70, buffer, 63);
            WriteBytesToAddr(0x70, buffer + 63, 63);
            WriteBytesToAddr(0x70, buffer + 126, 63);
            WriteBytesToAddr(0x70, buffer + 189, 63);
            WriteBytesToAddr(0x70, buffer + 252, 4);
#else
            WriteBytesToAddr(0x70, buffer, 128);
            WriteBytesToAddr(0x70, buffer+128, 128);
#endif

            WriteReg(0x6f, 0xa0); // Start Programing
        }
        ProcessCRC(buffer, sizeof(buffer));
        addr += 256;
    }
    while (addr < chip_size && data_len != 0 && addr < prog_size);
    delete [] prog;

    // Wait for programming cycle to finish
    do
    {
        b = ReadReg(0x6f);
    }
    while (b & 0x40);

    SPICommonCommand(E_CC_WRITE_AFTER_EWSR, 1, 0, 1, 0x1c); // Unprotect the Status Register
    SPICommonCommand(E_CC_WRITE_AFTER_WREN, 1, 0, 1, 0x1c); // Protect the flash

    uint8_t data_crc = GetCRC();
    uint8_t chip_crc = SPIComputeCRC(0, addr - 1);
    fprintf(stderr, "Received data CRC %02x\n", data_crc);
    fprintf(stderr, "Chip CRC %02x\n", chip_crc);
	if (data_crc == chip_crc) {
		fprintf(stderr, "Reset\n");
		WriteReg(0xEE, 0x04);
		WriteReg(0xEE, 0x06);
	}

    return data_crc == chip_crc;
}



#define SSD1306_I2C_ADDR 0x3C
#define SSD1306_COMMAND 0x00
#define SSD1306_DATA 0x40

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 32

BOOL sendCommand(ULONG iIndex, UCHAR iDevice, UCHAR command) {
    // I2C Write
    UCHAR iAddr = SSD1306_COMMAND; // SSD1306 OLED Register Address
    UCHAR iByte = command; // SSD1306 OLED Write Data Byte

    BOOL b = CH341WriteI2C(iIndex, iDevice, iAddr, iByte); // Display OFF
    return b;
}

BOOL sendCommandArray(ULONG iIndex, UCHAR iDevice, UCHAR commandArray[], ULONG arraySize) {
    // I2C Write
    UCHAR iAddr = SSD1306_COMMAND; // SSD1306 OLED Register Address
    UCHAR iByte; // Write Data Byte
    BOOL b; // Result

    for (int i = 0; i < arraySize; ++i) {
        iByte = commandArray[i]; // SSD1306 OLED Write Data Byte
        b = CH341WriteI2C(iIndex, iDevice, iAddr, iByte); // SSD1306 OLED Write CommandA
        if (!b) break;
    }
    return b;
}

BOOL sendDataBuffer(ULONG iIndex, UCHAR iDevice, ULONG iWriteLength, PVOID iWriteBuffer) {
    // I2C Transfer
    ULONG iTmpWriteLength = iWriteLength + 2;
    PUCHAR iTmpWriteBuffer = new UCHAR[iTmpWriteLength];

    memcpy(&iTmpWriteBuffer[2], iWriteBuffer, iWriteLength);
    iTmpWriteBuffer[0] = iDevice << 1; // SSD1306 I2C Address (But Need Shifted)
    iTmpWriteBuffer[1] = SSD1306_DATA; // SSD1306 OLED Write Data
    BOOL b = CH341StreamI2C(iIndex, iTmpWriteLength, iTmpWriteBuffer, 0UL, NULL);

    delete[] iTmpWriteBuffer;
    return b;
}

int ssd1306()
{
    // Device Index Number
    ULONG iIndex = 0;

    // Open Device
    HANDLE h = CH341OpenDevice(iIndex);

    // DLL verison
    ULONG dllVersion = CH341GetVersion();
    std::cout << "DLL verison " << dllVersion << "\n";

    // Driver version
    ULONG driverVersion = CH341GetDrvVersion();
    std::cout << "Driver verison " << driverVersion << std::endl;

    // Device Name
    PVOID p = CH341GetDeviceName(iIndex);
    std::cout << "Device Name " << (PCHAR)p << std::endl;

    // IC verison 0x10=CH341,0x20=CH341A,0x30=CH341A3
    ULONG icVersion = CH341GetVerIC(iIndex);
    std::cout << "IC version " << std::hex << icVersion << std::endl;

    // Reset Device
    BOOL b = CH341ResetDevice(iIndex);
    std::cout << "Reset Device " << b << std::endl;

    // Set serial stream mode
    // B1-B0: I2C SCL freq. 00=20KHz,01=100KHz,10=400KHz,11=750KHz
    // B2:    SPI I/O mode, 0=D3 CLK/D5 OUT/D7 IMP, 1=D3 CLK/D5&D4 OUT/D7&D6 INP)
    // B7:    SPI MSB/LSB, 0=LSB, 1=MSB
    // ULONG iMode = 0; // SCL = 10KHz
    // ULONG iMode = 1; // SCL = 100KHz
    ULONG iMode = 2; // SCL = 400KHz
    // ULONG iMode = 3; // SCL = 750KHz
    b = CH341SetStream(iIndex, iMode);
    std::cout << "Set Stream " << b << std::endl;

    // I2C Transfer
    UCHAR iDevice = 0x3C; // SSD1306 OLED I2C Address
    UCHAR iAddr; // SSD1306 OLED Register Address
    UCHAR iByte; // SSD1306 OLED Write Data Byte
    UCHAR oByte; // SSD1306 OLED Read Data Byte

    // Initialise SSD1306 OLED
    iAddr = 0x00; // SSD1306 OLED Write Command

    // Adafruit
    // 4.4 Actual Application Example
    // https://cdn-shop.adafruit.com/datasheets/UG-2864HSWEG01.pdf
    UCHAR initialization_command_array_adafruit[] = {
        0xAE, // Set Display Off
        0xD5, 0x80, // Set Display Clock Divide Ratio/Oscillator Frequency
        0xA8, SSD1306_HEIGHT - 1, // Set Multiplex Ratio
        0xD3, 0x00, // Set Display Offset
        0x40, // Set Display Start Line
        0x8D, 0x14, // Set Charge Pump, VCC Generated by Internal DC/DC Circuit
        0xA1, // Set Segment Re-Map
        0xC8, // Set COM Output Scan Direction
#if (SSD1306_HEIGHT == 32)
        0xDA, 0x02, // Set COM Pins Hardware Configuration
#else
        0xDA, 0x12, // Set COM Pins Hardware Configuration
#endif
        0x81, 0xCF, // * Set Contrast Control, VCC Generated by Internal DC/DC Circuit
        0xD9, 0xF1, // * Set Pre-Charge Period, VCC Generated by Internal DC/DC Circuit
        0xDB, 0x40, // Set VCOMH Deselect Level
        0xA4, // Set Entire Display On/Off
        0xA6, // Set Normal/Inverse Display
        // omit // Clear Screen
        0xAF, // Set Display On
    };

    // Waveshare
    // 3 Software Configuration
    // https://www.waveshare.com/w/upload/a/af/SSD1306-Revision_1.1.pdf
    UCHAR initialization_command_array_waveshare[] = {
        0xAE, // Set Display Off
        0xA8, SSD1306_HEIGHT - 1, // Set Multiplex Ratio
        0xD3, 0x00, // Set Display Offset
        0x40, // Set Display Start Line
        0xA1, // Set Segment Re-Map
        0xC8, // Set COM Output Scan Direction
#if (SSD1306_HEIGHT == 32)
        0xDA, 0x02, // Set COM Pins Hardware Configuration
#else
        0xDA, 0x12, // Set COM Pins Hardware Configuration
#endif
        0x81, 0x7F, // * Set Contrast Control
        // 0xD9, 0xF1, // * Set Pre-Charge Period, VCC Generated by Internal DC/DC Circuit
        // 0xDB, 0x40, // Set VCOMH Deselect Level
        0xA4, // Set Entire Display On/Off
        0xA6, // Set Normal/Inverse Display
        0xD5, 0x80, // Set Display Clock Divide Ratio/Oscillator Frequency
        0x8D, 0x14, // Set Charge Pump, VCC Generated by Internal DC/DC Circuit
        0xAF, // Set Display On
    };

    b = sendCommandArray(iIndex, iDevice, initialization_command_array_adafruit, sizeof(initialization_command_array_adafruit));
    // b = sendCommandArray(iIndex, iDevice, initialization_command_array_waveshare, sizeof(initialization_command_array_waveshare));

    for (UCHAR k = 0; k < 100; ++k) {
        for (UCHAR i = 0; i < SSD1306_HEIGHT / 8; ++i) {
            b = sendCommand(iIndex, iDevice, 0xB0 | i); // Set the current RAM page address.
            b = sendCommand(iIndex, iDevice, 0x00); //
            b = sendCommand(iIndex, iDevice, 0x10); //

            // Write Stream
            ULONG iWriteLength = SSD1306_WIDTH;
            UCHAR iWriteBuffer[SSD1306_WIDTH];
            for (UCHAR j = 0; j < SSD1306_WIDTH; ++j) {
                iWriteBuffer[j] = j + i + k;
            }
            b = sendDataBuffer(iIndex, iDevice, iWriteLength, iWriteBuffer);

            Sleep(1); // 1ms Need Waiting

            // ???
            // ULONG iDelay = 100UL;
            // b = CH341SetDelaymS(iIndex, iDelay);
        }
    }

    // Close Device
    CH341CloseDevice(iIndex);

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
#if 1
    bool bRet = true;
    uint8_t b, port = 0x4a;
    uint32_t jedec_id;

    InitI2C();
    fprintf(stderr, "Ready\n");
    if (5 <= argc) {
        port = strtol(argv[4], NULL, 0);
    }
    SetI2CAddr(port);

    const FlashDesc* chip;
    if (!WriteReg(0x6f, 0x80))    // Enter ISP mode
    {
        fprintf(stderr, "Write to 6F failed.\n");
        goto L_RET;
    }
    b = ReadReg(0x6f);
    if (!(b & 0x80))
    {
        fprintf(stderr, "Can't enable ISP mode\n");
        goto L_RET;
    }
	/*
    uint8_t chip_crc = SPIComputeCRC(0, 0xFFFF);
	fprintf(stderr, "chip_crc=%x\n", chip_crc);
    goto L_RET;
	*/

    jedec_id = SPICommonCommand(E_CC_READ, 0x9f, 3, 0, 0);
    fprintf(stderr, "JEDEC ID: 0x%02x\n", jedec_id);
    chip = FindChip(jedec_id);
    if (NULL == chip)
    {
        fprintf(stderr, "Unknown chip ID\n");
        goto L_RET;
    }
    fprintf(stderr, "Manufacturer ");
    PrintManufacturer(GetManufacturerId(chip->jedec_id));
    fprintf(stderr, "\n");
    fprintf(stderr, "Chip: %s\n", chip->device_name);
    fprintf(stderr, "Size: %dKB\n", chip->size_kb);

    // Setup flash command codes
    SetupChipCommands(chip->jedec_id);

    //SPICommonCommand(E_CC_WRITE, 1, 0, 1, 0); // Unprotect the Status Register

//  SPICommonCommand(E_CC_ERASE, 0x60, 0, 0, 0);         // Chip Erase
    b = SPICommonCommand(E_CC_READ, 0x5, 1, 0, 0);
    fprintf(stderr, "Flash status register(S7-S0): 0x%02x\n", b);
    b = SPICommonCommand(E_CC_READ, 0x35, 1, 0, 0);
    fprintf(stderr, "Flash status register(S15-S8): 0x%02x\n", b);

	int size = chip->size_kb * 1024;
	if (4 <= argc) {
		size = atoi(argv[3])*1024;
	}
	if (3 <= argc &&strcmp(argv[1], "-r")==0) {
		fprintf(stderr, "SaveFlash %s size=%d(kbyte)\n", argv[2], size/1024);
	    bRet = SaveFlash(argv[2], size);
	}
	else if (3 <= argc &&strcmp(argv[1], "-w")==0) {
		fprintf(stderr, "ProgramFlash %s size=%d(kbyte)\n\n", argv[2], size/1024);
	    bRet = ProgramFlash(argv[2], size);
	}
	else {
		fprintf(stderr, "%s (-r/-w) filepath (size kbyte) (i2c port)\n", argv[0]);
		goto L_RET;
	}
	if (bRet) {
		fprintf(stderr, "Success!\n");
	}
	else {
		fprintf(stderr, "Fail CRC unmatched!\n");
	}

L_RET:
    CloseI2C();
    return 0;
#else
	ssd1306();
#endif
}
