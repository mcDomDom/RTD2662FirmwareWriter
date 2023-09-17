#include "stdafx.h"
#include "i2c.h"
#include "CH341DLL_EN.H"

ULONG g_iIndex = 0;
ULONG g_iDevice = 0x4a;	// RTD2662 I2C Address

// open the Linux device
bool InitI2C()
{
    // Open Device
    HANDLE h = CH341OpenDevice(g_iIndex);
	if (h == INVALID_HANDLE_VALUE) {
		return false;
	}

    // DLL verison
    ULONG dllVersion = CH341GetVersion();
    std::cout << "DLL verison " << dllVersion << "\n";

    // Driver version
    ULONG driverVersion = CH341GetDrvVersion();
    std::cout << "Driver verison " << driverVersion << std::endl;

    // Device Name
    PVOID p = CH341GetDeviceName(g_iIndex);
    std::cout << "Device Name " << (PCHAR)p << std::endl;

    // IC verison 0x10=CH341,0x20=CH341A,0x30=CH341A3
    ULONG icVersion = CH341GetVerIC(g_iIndex);
    std::cout << "IC version " << std::hex << icVersion << std::endl;

    // Reset Device
    BOOL b = CH341ResetDevice(g_iIndex);
    std::cout << "Reset Device " << b << std::endl;
	if (!b) {
		return false;
	}

    // Set serial stream mode
    // B1-B0: I2C SCL freq. 00=20KHz,01=100KHz,10=400KHz,11=750KHz
    // B2:    SPI I/O mode, 0=D3 CLK/D5 OUT/D7 IMP, 1=D3 CLK/D5&D4 OUT/D7&D6 INP)
    // B7:    SPI MSB/LSB, 0=LSB, 1=MSB
    // ULONG iMode = 0; // SCL = 10KHz
    // ULONG iMode = 1; // SCL = 100KHz
    ULONG iMode = 2; // SCL = 400KHz
    // ULONG iMode = 3; // SCL = 750KHz
    b = CH341SetStream(g_iIndex, iMode);
    std::cout << "Set Stream " << b << std::endl;
	return b;
}

// close the Linux device
void CloseI2C()
{
    CH341CloseDevice(g_iIndex);
}

void SetI2CAddr(uint8_t address)
{
	g_iDevice = address;
}

bool WriteBytesToAddr(uint8_t reg, uint8_t* values, uint8_t len)
{
    // I2C Transfer
    ULONG iTmpWriteLength = len + 2;
    PUCHAR iTmpWriteBuffer = new UCHAR[iTmpWriteLength];

    memcpy(&iTmpWriteBuffer[2], values, len);
    iTmpWriteBuffer[0] = g_iDevice << 1; // SSD1306 I2C Address (But Need Shifted)
    iTmpWriteBuffer[1] = reg; // SSD1306 OLED Write Data
    BOOL b = CH341StreamI2C(g_iIndex, iTmpWriteLength, iTmpWriteBuffer, 0UL, NULL);
#ifdef _DEBUG
	for (int i=1; i<iTmpWriteLength; i++) {
		printf("0x%02X,0x%02X,Write\n", g_iDevice, iTmpWriteBuffer[i]);
	}
#endif

    delete[] iTmpWriteBuffer;
	return b;

}

bool ReadBytesFromAddr(uint8_t reg, uint8_t* dest, uint8_t len)
{
    // I2C Transfer
	uint8_t wr[2] = {g_iDevice<<1, reg};
    BOOL b = CH341StreamI2C(g_iIndex, 2, &wr[0], len, dest);
#ifdef _DEBUG
	for (int i=1; i<2; i++) {
		printf("0x%02X,0x%02X,Write\n", g_iDevice, wr[i]);
	}
	for (int i=0; i<len; i++) {
		printf("0x%02X,0x%02X,Read\n", g_iDevice, dest[i]);
	}
#endif

	return b;
}

uint8_t ReadReg(uint8_t reg)
{
	uint8_t	data;
#if 1
   //BOOL b = CH341ReadI2C(g_iIndex, g_iDevice, reg, &data);
   ReadBytesFromAddr(reg, &data, 1);
#else
	uint8_t wr[2] = {g_iDevice<<1, reg};
	uint8_t rd[1] = {0};
   BOOL b = CH341StreamI2C(g_iIndex, 2, &wr[0], 1, &rd[0]);
   data = rd[0];
#endif
   return data;
}

bool WriteReg(uint8_t reg, uint8_t value)
{
    //printf("Writing %02x to %02x\n",value,reg);
#if 0
	return CH341WriteI2C(g_iIndex, g_iDevice, reg, value);
#else
    return WriteBytesToAddr(reg, &value, 1);
#endif
}
