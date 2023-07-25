#ifndef I2C_FRAMEWORK_H
#define I2C_FRAMEWORK_H

#include "mbed.h"
#include "FlashIAP.h"
#include "BlockDevice.h"
#include <cstdio>

#if !DEVICE_I2CSLAVE
#error[NOT_SUPPORTED] I2C Slave is not supported
#endif

#if !DEVICE_I2C
#error[NOT_SUPPORTED] I2C is not supported
#endif

#if !DEVICE_FLASH
#error[NOT_SUPPORTED] FLASH is not supported
#endif

// I2C Frequency


// I2C Registers
#define FIRMWARE_REG (0xA0)
#define UID_REG (0xA1)
#define MAJOR_VERSION_REG (0xA2)
#define MINOR_VERSION_REG (0xA3)
#define FIX_VERSION_REG (0xA4)
#define GROUP_REG (0xA5)
#define SENSOR_TYPE_REG (0xA6)
#define NAME_REG (0xA7)

// Flash Addresses
#define FIRMWARE_STATUS_ADDRESS (0x0801FF00)
#define APPLICATION_HEADER_ADDRESS (0x08009800)
#define APPLICATION_METADATA_ADDRESS (0x08009000)
#define UNIQUE_ID_ADDR (0x1FFF7590)

// Values
#define MAGIC_FIRMWARE_NEED_UPDATE (0xDEADBEEF)
#define I2C_READ_DEFAULT_VALUE (0x42)
#define I2C_FREQ (100000)

class I2C_Framework
{

public:

    I2C_Framework(PinName sda, PinName scl);

    void init();

    void loop_iteration();
    
private:

    /**
     * Setup I2C slave with a random address
     */
    void setup_i2c();
    /**
     * Save metadata from RAM (active_app_metadata_save) to flash (active_app_metadata)
     */
    void save_metadata_to_flash();
    
    struct app_header_t{
        uint32_t magic;
        uint64_t firmware_size;
        uint32_t firmware_crc;
        uint8_t major_version;
        uint8_t minor_version;
        uint8_t fix_version;
    }__attribute__((__packed__));

    struct app_metadata_t{
        uint32_t magic_firmware_need_update;
        uint32_t group;
        char sensor_type[32];
        char name[32];
    };

    I2CSlave slave;
    I2C master;
    FlashIAP flash;
    app_header_t *active_app_header;
    app_metadata_t *active_app_metadata_flash;
    app_metadata_t active_app_metadata_ram;
    uint32_t id;
    uint16_t slave_addr;
    uint8_t i2c_register;
    int rc;
    char register_address[1];
    char buffer[33];
};


#endif // I2C_FRAMEWORK_H