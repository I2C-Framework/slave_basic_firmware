#include "i2c_framework.h"
#include <cstdio>

I2C_Framework::I2C_Framework(PinName sda, PinName scl) : slave(sda, scl), master(sda, scl), flash()
{
    i2c_register = 0;

    // Get unique ID by unreference pointer
    id = *((uint32_t *)UNIQUE_ID_ADDR);

    // Get access to application header in flash
    active_app_header = (app_header_t *)APPLICATION_HEADER_ADDRESS;

    // Get access to application metadata in flash
    active_app_metadata_flash = (app_metadata_t *)APPLICATION_METADATA_ADDRESS;
}

void I2C_Framework::init()
{
    // Print Unique ID of MCU
    printf("ID : 0x%x\n", id);

    flash.init();

    // Copy metadata from flash to RAM
    rc = flash.read((char *) &active_app_metadata_ram, APPLICATION_METADATA_ADDRESS, sizeof(app_metadata_t));
    if(rc != 0){
        printf("Error reading metadata from flash\r\n");
    }

    setup_i2c();

    printf("I2C Framework ready with I2C address 0x%x\n", slave_addr);
}

void I2C_Framework::loop_iteration()
{
    // Check if i2c slave has been addressed
    int i = slave.receive();
    switch (i) {

        case I2CSlave::ReadAddressed:
            printf("i2c_register : 0x%x\n", i2c_register);
            

            switch (i2c_register){
                case UID_REG: // Write Unique ID
                    slave.write((char*) &id, 4);
                    break;

                case MAJOR_VERSION_REG: // Write major version of firmware
                    slave.write(active_app_header->major_version);
                    break;
                
                case MINOR_VERSION_REG: // Write minor version of firmware
                    slave.write(active_app_header->minor_version);
                    break;
                
                case FIX_VERSION_REG: // Write fix version of firmware
                    slave.write(active_app_header->fix_version);
                    break;
                
                case GROUP_REG: // Write group of sensor
                    slave.write(active_app_metadata_ram.group);
                    break;

                case SENSOR_TYPE_REG: // Write sensor type
                    slave.write((char*) active_app_metadata_ram.sensor_type, 32);
                    break;
                
                case NAME_REG: // Write name of sensor
                    slave.write((char*) active_app_metadata_ram.name, 32);
                    break;

                default: // Register not set with write before, return default value
                    printf("Default value, 0x%x\n", I2C_READ_DEFAULT_VALUE);
                    int data = I2C_READ_DEFAULT_VALUE;
                    slave.write(data);
                    break;
            }

            i2c_register = 0;
            break;

        case I2CSlave::WriteGeneral:
            // Do nothing
            break;

        case I2CSlave::WriteAddressed:
            // Clear buffer
            memset(buffer, 0, 33);

            rc = slave.read(buffer, 33);

            printf("Register : 0x%x\n", buffer[0]);

            // Set register for next read
            i2c_register = buffer[0];

            switch (buffer[0]){
                case GROUP_REG:
                    if(buffer[1] > 0){
                        active_app_metadata_ram.group = buffer[1];
                        save_metadata_to_flash();
                    }
                    break;

                case FIRMWARE_REG:
                    active_app_metadata_ram.magic_firmware_need_update = MAGIC_FIRMWARE_NEED_UPDATE;
                    save_metadata_to_flash();
                    // Restart MCU to update firmware from bootloader
                    NVIC_SystemReset();
                    break;

                case SENSOR_TYPE_REG:
                    if(buffer[1] > 0){
                        memcpy(&active_app_metadata_ram.sensor_type, &buffer[1], 32);
                        save_metadata_to_flash();
                    }
                    break;

                case NAME_REG:
                    if(buffer[1] > 0){
                        memcpy(&active_app_metadata_ram.name, &buffer[1], 32);
                        save_metadata_to_flash();
                    }
                    break;

                default:
                    break;
            }
            
            break;
    }
}

void I2C_Framework::save_metadata_to_flash()
{
    // Erase sector on metadata address
    rc = flash.erase(APPLICATION_METADATA_ADDRESS, 2048);
    if(rc != 0){
        printf("Erase metadata from flash failed\n");
    }
    // Set metadata from RAM to flash
    rc = flash.program((char *) &active_app_metadata_ram, APPLICATION_METADATA_ADDRESS, sizeof(app_metadata_t));
    if(rc != 0){
        printf("Error writing metadata from flash\r\n");
    }
    // Copy metadata from flash to RAM
    rc = flash.read((char *) &active_app_metadata_ram, APPLICATION_METADATA_ADDRESS, sizeof(app_metadata_t));
    if(rc != 0){
        printf("Error reading metadata from flash\r\n");
    }
}

void I2C_Framework::setup_i2c()
{
    // Generate a random slave address with unique ID
    slave_addr = (id) % 95 + 0x10;

    // Generate a random wait time with unique ID
    uint16_t wait_time = (id) % 1000;

    
    // Set slave address to 0x00 to disable slave for now
    slave.address(0);
    slave.frequency(I2C_FREQ);

    // Wait for a random time to avoid collision
    //thread_sleep_for(wait_time);
    HAL_Delay(wait_time);

    master.frequency(I2C_FREQ);

    // Create a buffer to store data
    char data[1];

    // Check if slave address is already in use
    do{
        rc = master.read(slave_addr << 1, data, 1, false);

        if(rc != 0){
            // If slave address is free, set slave address
            slave.address(slave_addr << 1);
        } else {
            // If slave address is not free
            if(slave_addr == 0x6F){
                // If slave address is too high, reset to 0x10
                slave_addr = 0x10;
            } else {
                slave_addr++;
            }
        }
    
    // Loop until slave address is busy
    } while (rc == 0);
}