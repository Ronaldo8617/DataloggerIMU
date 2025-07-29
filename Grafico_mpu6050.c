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
    
    // Variáveis para os centros de desenho das linhas
    int center_x_roll = DISP_W / 2;
    int center_y_roll = (DISP_H / 2) - 10; 
    int center_x_pitch = DISP_W / 2;
    int center_y_pitch = (DISP_H / 2) + 10; 

    // Escala para a linha: quantos pixels por grau
    float pixels_per_degree_roll = 1.0f; 
    float pixels_per_degree_pitch = 1.0f; 
    
    // Comprimento da linha indicadora
    int line_length = 30; 

    float last_roll = 0.0f, last_pitch = 0.0f;
    const float limiar = 1.0f; // Zona morta: mínimo de variação em graus

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

            // Mapeamento do Roll para a posição X da linha
            // A linha vertical se move horizontalmente.
            // O valor 'offset_x' é o quanto a linha se desloca do centro.
            int offset_x = (int)(roll * pixels_per_degree_roll);
            int line_x = center_x_roll + offset_x;

            // Limitar a posição X da linha para ficar dentro dos limites do display
            if (line_x < 0) line_x = 0;
            if (line_x > DISP_W - 1) line_x = DISP_W - 1;

            // Mapeamento do Pitch para a posição Y da linha
            // A linha horizontal se move verticalmente.
            // O valor 'offset_y' é o quanto a linha se desloca do centro.
            int offset_y = (int)(pitch * pixels_per_degree_pitch);
            int line_y = center_y_pitch + offset_y;

            // Limitar a posição Y da linha para ficar dentro dos limites do display
            if (line_y < 0) line_y = 0;
            if (line_y > DISP_H - 1) line_y = DISP_H - 1;

            char str_roll[20], str_pitch[20];
            snprintf(str_roll, sizeof(str_roll), "R:%3.0f", roll); // Número de casas decimais e precisão
            snprintf(str_pitch, sizeof(str_pitch), "P:%3.0f", pitch); 

            ssd1306_fill(&ssd, false); // Limpa o display antes de desenhar

            // Desenha a linha de referência central para o Roll
            ssd1306_line(&ssd, center_x_roll, center_y_roll - 5, center_x_roll, center_y_roll + 5, true); 
            // Desenha a linha indicadora do Roll (vertical que se move horizontalmente)
            ssd1306_line(&ssd, line_x, center_y_roll - (line_length / 2), line_x, center_y_roll + (line_length / 2), true);
            // Exibe o valor do Roll
            ssd1306_draw_string(&ssd, str_roll, 2, 2);

            // Desenha a linha de referência central para o Pitch
            ssd1306_line(&ssd, center_x_pitch - 5, center_y_pitch, center_x_pitch + 5, center_y_pitch, true); 
            // Desenha a linha indicadora do Pitch (horizontal que se move verticalmente)
            ssd1306_line(&ssd, center_x_pitch - (line_length / 2), line_y, center_x_pitch + (line_length / 2), line_y, true);
            // Exibe o valor do Pitch
            ssd1306_draw_string(&ssd, str_pitch, 2, 55);

            ssd1306_send_data(&ssd); // Envia os dados para o display
        }

        sleep_ms(100);
    }
}