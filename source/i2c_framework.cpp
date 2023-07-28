#include "i2c_framework.h"
#include <cstdio>

I2C_Framework::I2C_Framework(PinName sda, PinName scl) : slave(sda, scl), master(sda, scl), flash(), scl_status(scl), led_status(LED_STATUS)
{
    // Set i2c register to 0
    i2c_register = 0;

    // Set size of elements array to 0
    i2c_callback_array_size = 0;

    // Get unique ID by unreference pointer
    id = *((uint32_t *)UNIQUE_ID_ADDR);

    // Get access to application header in flash
    active_app_header = (app_header_t *)APPLICATION_HEADER_ADDRESS;

    // Get access to application metadata in flash
    active_app_metadata_flash = (app_metadata_t *)APPLICATION_METADATA_ADDRESS;

    // Clear buffer
    memset(buffer, 0, 33);
}

void I2C_Framework::init()
{
    // Print Unique ID of MCU
    //printf("ID : 0x%x\n", id);

    // Init flash class
    flash.init();

    // Copy metadata from flash to RAM
    rc = flash.read((char *) &active_app_metadata_ram, APPLICATION_METADATA_ADDRESS, sizeof(app_metadata_t));
    if(rc != 0){
        //printf("Error reading metadata from flash\r\n");
        led_status = 1;
    }

    // Setup i2c communication
    setup_i2c();

    // Get watchdog instance
    watchdog = &Watchdog::get_instance();
    // Start watchdog
    watchdog->start(WATCHDOG_TIMEOUT);

    //printf("I2C Framework ready with I2C address 0x%x\n", slave_addr);
}

void I2C_Framework::check_scl(){
    // If nothing appended to SCL, reset watchdog time
    if(scl_status == 1){
        watchdog->kick();
    }
}

void I2C_Framework::loop_iteration()
{
    // Check if SCL is stuck
    check_scl();
    
    // Check if i2c slave has been addressed
    slave_action = slave.receive();
    
    switch (slave_action) {
        case I2CSlave::ReadAddressed:
            
            //printf("i2c_register : 0x%x\n", i2c_register);

            for(int i = 0; i < i2c_callback_array_size; i++){
                if(i2c_callback_array[i][0] == i2c_register){
                    // Get read callback from array
                    char * (*read_callback)() = (char * (*)()) i2c_callback_array[i][1];
                    // Call read callback
                    char *data = read_callback();
                    // Write data to i2c slave
                    slave.write(data, i2c_callback_array[i][3]);
                    // Set register to 0
                    i2c_register = 0;
                    return;
                }
            }
            

            switch (i2c_register){
                case UID_REG: // Write Unique ID
                    slave.write((char *) &id, 4);
                    break;

                case VERSION_HASH_REG: // Write major version of firmware
                    slave.write((char*) active_app_header->firmware_version_hash, 32);
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
                    //printf("Default value, 0x%x\n", I2C_READ_DEFAULT_VALUE);
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
            rc = slave.read(buffer, 33);

            //printf("Register : 0x%x\n", buffer[0]);

            // Set register for next read
            i2c_register = buffer[0];

            switch (buffer[0]){
                case GROUP_REG: // If new group is received, save to flash
                    if(buffer[1] > 0){
                        active_app_metadata_ram.group = buffer[1];
                        save_metadata_to_flash();
                        i2c_register = 0;
                    }
                    break;

                case FIRMWARE_REG: // If it's firmware register, set flag to update firmware and restart MCU
                    active_app_metadata_ram.magic_firmware_need_update = MAGIC_FIRMWARE_NEED_UPDATE;
                    save_metadata_to_flash();
                    // Restart MCU to update firmware from bootloader
                    NVIC_SystemReset();
                    break;

                case SENSOR_TYPE_REG: // If new sensor type is received, save to flash
                    if(buffer[1] > 0){
                        memcpy(&active_app_metadata_ram.sensor_type, &buffer[1], 32);
                        save_metadata_to_flash();
                        i2c_register = 0;
                    }
                    break;

                case NAME_REG: // If new name is received, save to flash
                    if(buffer[1] > 0){
                        memcpy(&active_app_metadata_ram.name, &buffer[1], 32);
                        save_metadata_to_flash();
                        i2c_register = 0;
                    }
                    break;

                default:
                    break;
            }

            
            for(int i = 0; i < i2c_callback_array_size; i++){
                if(i2c_callback_array[i][0] == i2c_register){
                    // Get write callback from array
                    int (*write_callback)(char *) = (int (*)(char *)) i2c_callback_array[i][2];
                    // Call write callback
                    i2c_register = write_callback(buffer);
                }
            }

            // Clear buffer
            memset(buffer, 0, 33);
            
            break;
    }
    
}

void I2C_Framework::save_metadata_to_flash()
{
    // Erase sector on metadata address
    rc = flash.erase(APPLICATION_METADATA_ADDRESS, 2048);
    if(rc != 0){
        //printf("Erase metadata from flash failed\n");
        led_status = 1;
    }
    // Set metadata from RAM to flash
    rc = flash.program((char *) &active_app_metadata_ram, APPLICATION_METADATA_ADDRESS, sizeof(app_metadata_t));
    if(rc != 0){
        //printf("Error writing metadata from flash\r\n");
        led_status = 1;
    }
    // Copy metadata from flash to RAM
    rc = flash.read((char *) &active_app_metadata_ram, APPLICATION_METADATA_ADDRESS, sizeof(app_metadata_t));
    if(rc != 0){
        //printf("Error reading metadata from flash\r\n");
        led_status = 1;
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

void I2C_Framework::init_i2c_callback_size(int size){
    i2c_callback_array = new uint32_t*[size];
    for(int i = 0; i < size; i++){
        i2c_callback_array[i] = new uint32_t[4];
    }
}

void I2C_Framework::add_i2c_callback(int register_address, char * (*read_callback)(), int (*write_callback)(char *buffer), int data_size){
    i2c_callback_array[i2c_callback_array_size][0] = register_address;
    i2c_callback_array[i2c_callback_array_size][1] = reinterpret_cast<uint32_t>(read_callback);
    i2c_callback_array[i2c_callback_array_size][2] = reinterpret_cast<uint32_t>(write_callback);
    i2c_callback_array[i2c_callback_array_size][3] = data_size;
    i2c_callback_array_size++;
}