/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *  Direto do Pico Examples do Raspberry Pi Pico SDK
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include <stdio.h>
 #include <string.h>
 #include "pico/stdlib.h"
 #include "pico/binary_info.h"
 #include "hardware/i2c.h"
 
    // MPU6050 I2C address
#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                  // 1 ou 3

 
 // O endereço padrao deste IMU é o 0x68
 static int addr = 0x68;
 

 static void mpu6050_reset() {
     // Two byte reset. First byte register, second byte data
     // There are a load more options to set up the device in different ways that could be added here
     uint8_t buf[] = {0x6B, 0x80};
     i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
     sleep_ms(100); // Allow device to reset and stabilize
 
     // Clear sleep mode (0x6B register, 0x00 value)
     buf[1] = 0x00;  // Clear sleep mode by writing 0x00 to the 0x6B register
     i2c_write_blocking(I2C_PORT, addr, buf, 2, false); 
     sleep_ms(10); // Allow stabilization after waking up
 }
 
 static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
     // For this particular device, we send the device the register we want to read
     // first, then subsequently read from the device. The register is auto incrementing
     // so we don't need to keep sending the register we want, just the first.
 
     uint8_t buffer[6];
 
     // Start reading acceleration registers from register 0x3B for 6 bytes
     uint8_t val = 0x3B;
     i2c_write_blocking(I2C_PORT, addr, &val, 1, true); // true to keep master control of bus
     i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);
 
     for (int i = 0; i < 3; i++) {
         accel[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);
     }
 
     // Now gyro data from reg 0x43 for 6 bytes
     // The register is auto incrementing on each read
     val = 0x43;
     i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
     i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);  // False - finished with bus
 
     for (int i = 0; i < 3; i++) {
         gyro[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);;
     }
 
     // Now temperature from reg 0x41 for 2 bytes
     // The register is auto incrementing on each read
     val = 0x41;
     i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
     i2c_read_blocking(I2C_PORT, addr, buffer, 2, false);  // False - finished with bus
 
     *temp = buffer[0] << 8 | buffer[1];
 }

 
// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
   // Fim do trecho para modo BOOTSEL com botão B
 


    stdio_init_all();
    sleep_ms(5000);
 
     printf("Hello, MPU6050! Reading raw data from registers...\n");
 
     // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
     i2c_init(I2C_PORT, 400 * 1000);
     gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
     gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
     gpio_pull_up(I2C_SDA);
     gpio_pull_up(I2C_SCL);
     // Make the I2C pins available to picotool
     printf("Antes do bi_decl...\n");
     bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
     printf("Antes do reset MPU...\n");
     mpu6050_reset();
 
     int16_t acceleration[3], gyro[3], temp;
 
     while (1) {
         mpu6050_read_raw(acceleration, gyro, &temp);
 
         // These are the raw numbers from the chip, so will need tweaking to be really useful.
         // See the datasheet for more information
         printf("Acc. X = %d, Y = %d, Z = %d\n", acceleration[0], acceleration[1], acceleration[2]);
         printf("Gyro. X = %d, Y = %d, Z = %d\n", gyro[0], gyro[1], gyro[2]);
         // Temperature is simple so use the datasheet calculation to get deg C.
         // Note this is chip temperature.
         printf("Temp. = %f\n", (temp / 340.0) + 36.53);
 
         sleep_ms(100);
     }

 }