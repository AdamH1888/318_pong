#include "i2c_bus.h"

static bool s_initialised = false; //Static boolean flag and it keeps track of whether the I2C bus has been initialised

//This function is used to initialise the I2C bus
//Communication protocol used to transfer data between MCU and OLED/LCD
//It is a two-wire interface that allows multiple devices to communicate with each other
//SDA: This wire is used to transfer data between the devices
//SCL: This wire is used to synchroniSe data transfer
//I2C supports multiple devices on the same two wires
//If s_inited is true, the function exits early, the I2C bus is already initialised
//
void i2c_bus_init(void)
{
    if (s_initialised) return;
    s_initialised = true;

    //Declares a variable 'cfg' which is a structure that holds the configuration settings for the I2C bus
    //Fills the 'cfg' structure with default settings for the I2C bus (like clock speed, I2C mode etc)
    lpi2c_master_config_t cfg;
    LPI2C_MasterGetDefaultConfig(&cfg);

    //Sets the baud rate, this is the speed at which the I2C devices will communicate (100kHz)
    cfg.baudRate_Hz = 100000u;

    //Initialises the I2C bus for communication with the devices connected to it
    LPI2C_MasterInit(I2C_SHARED, &cfg, I2C_SHARED_CLOCK_HZ);
}


