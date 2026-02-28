#include "lcd_score.h"
#include "lcd.h"
#include "fsl_clock.h"
#include "fsl_common.h"

//Function to print a 16-bit unsigned integer as two digits on the LCD
static void lcd_print_u16_2digits(uint16_t v)
{
    uint16_t x = (uint16_t)(v % 100u);				//Get the last 2 digits of the number, clean way of doing it
    pcf8574_send_data((uint8_t)('0' + (x / 10u)));	//Send the tens place, ASCII value for the character '0' is 48, add 48 to get ACSII value for character
    pcf8574_send_data((uint8_t)('0' + (x % 10u)));	//Send the ones place
}

//Initialise the LCD display
void lcd_score_init(void)
{
    pcf8574_init();
    pcf8574_send_cmd(0x01);	//Clear the display
    SDK_DelayAtLeastUs(2000u, CLOCK_GetFreq(kCLOCK_CoreSysClk)); //Delay to wait for the LCD to be ready
}

//Display the score on the LCD
void lcd_show_score(uint16_t left, uint16_t right)
{
    pcf8574_cursor(0, 0);			//Set the cursor to the top-left corner of LCD
    pcf8574_send_string("L:");
    lcd_print_u16_2digits(left);	//Display the left score (2 digits)
    pcf8574_send_string("  R:");
    lcd_print_u16_2digits(right);	//Display the right score (2 digits)
    pcf8574_send_string("      ");	//Clear extra spaces
}

//Display countdown timer on LCD row 2 — format "Time: M:SS      " (16 chars)
//seconds_remaining: value from 60 down to 0
void lcd_show_timer(uint32_t seconds_remaining)
{
    uint32_t mins = seconds_remaining / 60u;
    uint32_t secs = seconds_remaining % 60u;

    pcf8574_cursor(1, 0);                               //Row 2, column 0
    pcf8574_send_string("Time: ");                      //6 chars
    pcf8574_send_data((uint8_t)('0' + (mins % 10u)));   //Minutes digit
    pcf8574_send_data(':');
    pcf8574_send_data((uint8_t)('0' + (secs / 10u)));   //Seconds tens
    pcf8574_send_data((uint8_t)('0' + (secs % 10u)));   //Seconds ones
    pcf8574_send_string("      ");                      //6 trailing spaces = 16 total
}

//Clear row 2 of the LCD — call when returning to menu so timer doesn't linger
void lcd_clear_timer(void)
{
    pcf8574_cursor(1, 0);
    pcf8574_send_string("                ");  //16 spaces
}
