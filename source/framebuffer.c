#include "framebuffer.h"
#include "oled.h"

//This is an array of 8-bit unsigned integers
//It represents the frame buffer that stores the pixel data for the OLED screen
//A frame buffer is a block of memory used to store the pixel data for a screen
//It holds each pixel that will be shown on screen temporarily before it is displayed
//FB_SIZE determines how much memory is needed for the frame buffer based on width and height
static uint8_t s_frameBuffer[FB_SIZE];

//Clears entire frame buffer by setting all pixels to a set value (0)
//s_frameBuffer gives the total size in bytes
//Loops through each byte of frame buffer and sets it to 'value' (0xFF)
void fb_clear(uint8_t value)
{
    for (int i = 0; i < (int)sizeof(s_frameBuffer); i++)
    {
        s_frameBuffer[i] = value;
    }
}

//Sets a pixel at a specific (x,y) position in the frame buffer
void fb_set_pixel(int x, int y)
{

	//Checks if coordinates are within the screen boundaries (128x64)
    if (x < 0 || x >= FB_WIDTH)  return;
    if (y < 0 || y >= FB_HEIGHT) return;

    //The OLED screen is split into pages (8 horizontal rows)
    //This calculates which page the pixel belongs to based on the y-coordinate (Can be page 0-7)
    //The bit position of the pixel within its page is also calculated, so how many pixels down within that page
    int pageIndex = y / 8;
    int bitIndex  = y % 8;

    //Calculates the index of the pixel in the frame buffer array based on its x and pageIndex
    //In memory 128x64 pixels are stored in a 1D array which is basically a long list of numbers
    //Each index in this list corresponds to a pixel
    //Then it sets the corresponding bit in the frame buffer array for that pixel, by accessing the byte and shifting '1' bit index spaces to the left
    int bufferIndex = pageIndex * FB_WIDTH + x;
    s_frameBuffer[bufferIndex] |= (uint8_t)(1u << bitIndex);
}

//Sends the frame buffer data to the OLED screen
void fb_flush_to_oled(void)
{

	//Loops through all 8 pages
	//Sets the current page to which the data will be written
	//Sets the segment (column) to start at which is 0
	//Sends the pixel data for the current page, from the frame buffer to the OLED screen
	//Runs 8 times for each page on the screen
    for (uint8_t page = 0; page < 8; page++)
    {
        setPage(page);
        setSeg(0);
        sendOLED(&s_frameBuffer[(uint32_t)page * FB_WIDTH], FB_WIDTH, OLED_DATA); //Address of data for specific page from frame buffer
    }
}
