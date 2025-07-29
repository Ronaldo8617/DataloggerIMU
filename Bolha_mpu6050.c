#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "font.h"

#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define ENDERECO_DISP 0x3C
#define DISP_W 128
#define DISP_H 64

static int addr = 0x68;

static void mpu6050_reset()
{
    uint8_t buf[] = {0x6B, 0x80};
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(100);
    buf[1] = 0x00;
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(10);
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp)
{
    uint8_t buffer[6];
    uint8_t val = 0x3B;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);
    for (int i = 0; i < 3; i++)
        accel[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];

    val = 0x43;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);
    for (int i = 0; i < 3; i++)
        gyro[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];

    val = 0x41;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 2, false);
    *temp = (buffer[0] << 8) | buffer[1];
}

#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    stdio_init_all();
    sleep_ms(5000);

    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    ssd1306_t ssd;
    ssd1306_init(&ssd, DISP_W, DISP_H, false, ENDERECO_DISP, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    mpu6050_reset();

    int16_t aceleracao[3], gyro[3], temp;
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;
    int range = 20;
    float sens = 2.0f;

    float last_roll = 0.0f, last_pitch = 0.0f;
    const float limiar = 1.0f; // zona morta: mínimo de variação em graus

    while (1)
    {
        mpu6050_read_raw(aceleracao, gyro, &temp);

        float ax = aceleracao[0] / 16384.0f;
        float ay = aceleracao[1] / 16384.0f;
        float az = aceleracao[2] / 16384.0f;

        float roll  = atan2(ay, az) * 180.0f / M_PI;
        float pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0f / M_PI;

        if (fabs(roll - last_roll) > limiar || fabs(pitch - last_pitch) > limiar) {
            last_roll = roll;
            last_pitch = pitch;

            int x = cx + (int)(roll / sens);
            int y = cy + (int)(pitch / sens);

            // Limites da bolha 5x5 (centro entre 2 e 125; 2 e 61)
            if (x < 2) x = 2;
            if (x > 125) x = 125;
            if (y < 2) y = 2;
            if (y > 61) y = 61;

            char str_roll[20], str_pitch[20];
            snprintf(str_roll, sizeof(str_roll), "R:%3.0f", roll);
            snprintf(str_pitch, sizeof(str_pitch), "P:%3.0f", pitch);

            ssd1306_fill(&ssd, false);

            // Cruz central de referência
            ssd1306_line(&ssd, cx-range, cy, cx+range, cy, true);
            ssd1306_line(&ssd, cx, cy-range, cx, cy+range, true);

            // Bolha IMU
            ssd1306_rect(&ssd, y-2, x-2, 5, 5, true, true);

            ssd1306_draw_string(&ssd, "Bolha IMU", 31, 2);
            ssd1306_draw_string(&ssd, str_roll,  5, 54);
            ssd1306_draw_string(&ssd, str_pitch, 68, 54);

            ssd1306_send_data(&ssd);
        }

        sleep_ms(100);
    }
}
