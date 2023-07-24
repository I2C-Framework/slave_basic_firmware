#include "mbed.h"
#include "i2c_framework.h"

I2C_Framework i2c_framework(I2C_FRAMEWORK_SDA, I2C_FRAMEWORK_SCL);

int main()
{
    i2c_framework.init();
    
    while (true)
    {
        i2c_framework.loop_iteration();
    }
}