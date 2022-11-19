
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pio_i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "registers.h"
#include "apds_registers.h"
#include "adafruit_qtpy_rp2040.h"
#include "ws2812.h"
#include "ws2812.pio.h"


#define PIN_SDA 22
#define PIN_SCL 23

#define IS_RGBW true
#define NUM_PIXELS 150
#define PICO_DEFAULT_WS2812_POWER_PIN 11

#ifdef PICO_DEFAULT_WS2812_PIN
#define WS2812_PIN PICO_DEFAULT_WS2812_PIN
#else
#define WS2812_PIN 2
#endif

void config_neopixel(uint32_t color){
    uint32_t red = color >> 16;
    uint32_t gre = color << 16;
    gre = gre >> 24;
    uint32_t blu = color << 24;
    blu = blu >> 24;
    uint32_t result = ((red << 8) |
                            (gre << 16)|
                            (blu ));
    pio_sm_put_blocking(pio0, 0, result << 8u);
}
void config_apds(PIO pio, uint sm){
    // to Power on APDS Sensor
    uint8_t tx_buffer[2] = {0};

    // 
    tx_buffer[0] = ATIME_REGISTER;
    tx_buffer[1] = (uint8_t)(0x81);
    pio_i2c_write_blocking(pio, sm, APDS_ADDRESS, tx_buffer, 2);

    //Cotrol Register Configure
    tx_buffer[0] = APDS_CONTROL_ONE_REGISTER;
    tx_buffer[1] = APDS_CONTROL_ONE_AGAIN;
    pio_i2c_write_blocking(pio, sm, APDS_ADDRESS, tx_buffer, 2);

    tx_buffer[0] = APDS_ENABLE_REGISTER;
    tx_buffer[1] = APDS_ENABLE_PON | APDS_ENABLE_AEN | APDS_ENABLE_PEN;
    pio_i2c_write_blocking(pio, sm, APDS_ADDRESS, tx_buffer, 2);
}

void adpsread(PIO pio, uint sm, uint8_t reg_addr, uint8_t *rx_buffer, uint num_bytes) {
    // Read the reg_addr.
    pio_i2c_write_blocking(pio, sm, APDS_ADDRESS, &reg_addr, 1);  
    pio_i2c_read_blocking(pio, sm, APDS_ADDRESS, rx_buffer, num_bytes);
}
uint32_t read_prox_color(PIO pio, uint sm){
    // Check the status register, to know if we can read the values from the ALS and Proximity engine.
    // read the values from ALS and Proximity engine check status register
    uint8_t rx_buffer[1] = {0};
    adpsread(pio, sm, STATUS_REGISTER, rx_buffer, 1);
    adpsread(pio, sm, ID_REGISTER, rx_buffer, 1);
    //Initialize the orig color data
    uint16_t c = 0;
    uint16_t r = 0;
    uint16_t g = 0;
    uint16_t b = 0;
    // Mask to check Proximity and color data to read
    uint8_t data_arr[8] = {0};
    if ((rx_buffer[0] & STATUS_REGISTER_PVALID) == STATUS_REGISTER_PVALID) {
        adpsread(pio, sm, PROXIMITY_DATA_REGISTER, data_arr, 1);
        printf("Proximity : %d\n", data_arr[0]); 
    } 
    if ((rx_buffer[0] & STATUS_REGISTER_AVALID) == STATUS_REGISTER_AVALID) {
        adpsread(pio, sm, RGBC_DATA_REGISTER_CDATAL, data_arr, 8);
        c = (data_arr[1] << 8 | data_arr[0]); 
        r = (data_arr[3] << 8 | data_arr[2]); 
        g = (data_arr[5] << 8 | data_arr[4]); 
        b = (data_arr[7] << 8 | data_arr[6]); 
        printf("Color Value : (%d, %d, %d, %d)\n", r, g, b, c);
    }
    uint32_t color_packet = 0;
    if ((r> g) && (r> b)) {
        color_packet = ((uint8_t)((r*255) / 65536) << 16);
    } else if ((g > r) && (g > b)) {
        color_packet = ((uint8_t)((g*255) / 65536) << 8);
    } else if ((b > r) && (b > r)) {
        color_packet = (uint8_t)((b*255) / 65536);
    }
    return color_packet;
}

int main() {
    const uint POWER_PIN_NAME = PICO_DEFAULT_WS2812_POWER_PIN;
    gpio_init(POWER_PIN_NAME);
    gpio_set_dir(POWER_PIN_NAME, GPIO_OUT);
    gpio_put(POWER_PIN_NAME, 1);
    stdio_init_all();


    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    PIO pio_1 = pio1;//changed from pio0 to pio1
    uint sm_1 = 1;
    uint offset_1 = pio_add_program(pio_1, &i2c_program);
    i2c_program_init(pio_1, sm_1, offset_1, PIN_SDA, PIN_SCL);    



    //wait until usb is connected
    while(!stdio_usb_connected());
    printf("Welcome to PIO I2C ADPS9960\n");;
    // Configure the APDS snesor
    config_apds(pio_1, sm_1);
    uint32_t color_result =0;

    while(1) {
        // Call the read_prox_and_color function
        color_result = read_prox_color(pio_1, sm_1);
        printf("The Color to pio: 0x%06x\n", color_result);
        config_neopixel(color_result);
        sleep_ms(300);
        config_neopixel(0);
    }
 
    return 0;
}