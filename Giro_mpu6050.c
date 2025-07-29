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
    
    // Variáveis para as Barras de Giroscópio
    int center_x_gyro_bar = DISP_W / 2; 
    int bar_width_max = DISP_W / 2 - 10; 
    
    // Posições Y para as barras de Giroscópio
    int y_pos_gx = 18; 
    int y_pos_gy = 33; 
    int y_pos_gz = 48; 

    // Escala para os valores do Giroscópio para mapear para pixels
    float gyro_scale_factor = (float)bar_width_max / 32767.0f; 
                                                              
    // Variáveis de limiar e last_value para giroscópio
    float last_gx = 0.0f, last_gy = 0.0f, last_gz = 0.0f;
    const float gyro_limiar = 100.0f; // Zona morta para giroscópio 

    while (1)
    {
        mpu6050_read_raw(aceleracao, gyro, &temp);

        // Giroscópio: valores brutos gx, gy, gz
        float gx = (float)gyro[0];
        float gy = (float)gyro[1];
        float gz = (float)gyro[2];

        // Atualiza apenas se houver mudança significativa no giroscópio
        if (fabs(gx - last_gx) > gyro_limiar || fabs(gy - last_gy) > gyro_limiar || fabs(gz - last_gz) > gyro_limiar) {
            last_gx = gx;
            last_gy = gy;
            last_gz = gz;

            char str_gx[20], str_gy[20], str_gz[20];
            snprintf(str_gx, sizeof(str_gx), "GX:%6d", (int)gx);
            snprintf(str_gy, sizeof(str_gy), "GY:%6d", (int)gy);
            snprintf(str_gz, sizeof(str_gz), "GZ:%6d", (int)gz);

            ssd1306_fill(&ssd, false); // Limpa o display a cada atualização

            // Título
            ssd1306_draw_string(&ssd, "Taxa Giro IMU", 10, 2);

            // --- Barra de GX ---
            // Linha de referência central para GX
            ssd1306_line(&ssd, center_x_gyro_bar, y_pos_gx, center_x_gyro_bar, y_pos_gx + 4, true); 
            
            int bar_end_x_gx = center_x_gyro_bar + (int)(gx * gyro_scale_factor);
            
            // Limitar a barra dentro dos limites do display
            if (bar_end_x_gx < 0) bar_end_x_gx = 0;
            if (bar_end_x_gx > DISP_W - 1) bar_end_x_gx = DISP_W - 1;

            // Desenha a barra de GX
            ssd1306_line(&ssd, center_x_gyro_bar, y_pos_gx + 2, bar_end_x_gx, y_pos_gx + 2, true);
            // Exibe o valor de GX
            ssd1306_draw_string(&ssd, "X", 5, y_pos_gx + 8); // Posiciona abaixo da barra

            // --- Barra de GY ---
            // Linha de referência central para GY
            ssd1306_line(&ssd, center_x_gyro_bar, y_pos_gy, center_x_gyro_bar, y_pos_gy + 4, true); 

            int bar_end_x_gy = center_x_gyro_bar + (int)(gy * gyro_scale_factor);
            
            if (bar_end_x_gy < 0) bar_end_x_gy = 0;
            if (bar_end_x_gy > DISP_W - 1) bar_end_x_gy = DISP_W - 1;

            // Desenha a barra de GY
            ssd1306_line(&ssd, center_x_gyro_bar, y_pos_gy + 2, bar_end_x_gy, y_pos_gy + 2, true);
            // Exibe o valor de GY
            ssd1306_draw_string(&ssd, "Y", 5, y_pos_gy + 8);

            // --- Barra de GZ ---
            // Linha de referência central para GZ
            ssd1306_line(&ssd, center_x_gyro_bar, y_pos_gz, center_x_gyro_bar, y_pos_gz + 4, true); 
            
            int bar_end_x_gz = center_x_gyro_bar + (int)(gz * gyro_scale_factor);
            
            if (bar_end_x_gz < 0) bar_end_x_gz = 0;
            if (bar_end_x_gz > DISP_W - 1) bar_end_x_gz = DISP_W - 1;

            // Desenha a barra de GZ
            ssd1306_line(&ssd, center_x_gyro_bar, y_pos_gz + 2, bar_end_x_gz, y_pos_gz + 2, true);
            // Exibe o valor de GZ
            ssd1306_draw_string(&ssd, "Z", 5, y_pos_gz + 8);

            ssd1306_send_data(&ssd); // Envia os dados para o display
        }

        sleep_ms(100);
    }
}