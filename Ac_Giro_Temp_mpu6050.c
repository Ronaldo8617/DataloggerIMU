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

    // --- Variáveis para as Barras de Giroscópio (Parte Superior da Tela) ---
    int center_x_gyro_bar = DISP_W / 2;    // Centro horizontal para todas as barras de giro
    int bar_width_max = (DISP_W / 2) - 10; // Largura máxima de cada barra de giro

    // Posições Y para as barras de Giroscópio
    int y_pos_gx = 12; 
    int y_pos_gy = 23; 
    int y_pos_gz = 34; 

    // Escala para os valores do Giroscópio (mapear para pixels)
    float gyro_scale_factor = (float)bar_width_max / 32767.0f; // Mapeia o max. int16_t para bar_width_max

    // Variáveis de limiar e last_value para giroscópio
    float last_gx = 0.0f, last_gy = 0.0f, last_gz = 0.0f;
    const float gyro_limiar = 100.0f; // Zona morta para giroscópio (em LSB)

    // --- Variáveis para as Linhas de Pitch/Roll (Parte Inferior da Tela) ---
    int center_x_accel_line = DISP_W / 2;
    // As linhas de Pitch/Roll serão desenhadas mais abaixo no display
    int y_pos_accel_ref = 55; 

    // Escala para Pitch/Roll: quantos pixels por grau
    float pixels_per_degree_roll = 0.8f;  // Ajuste conforme a sensibilidade desejada
    float pixels_per_degree_pitch = 0.8f; 

    // Comprimento da linha indicadora de Pitch/Roll
    int accel_line_length = 25;

    float last_roll = 0.0f, last_pitch = 0.0f;
    const float accel_limiar = 1.0f; // Zona morta para Pitch/Roll (em graus)

    while (1)
    {
        mpu6050_read_raw(aceleracao, gyro, &temp);

        // --- Cálculos para Giroscópio ---
        float gx = (float)gyro[0];
        float gy = (float)gyro[1];
        float gz = (float)gyro[2];

        // --- Cálculos para Acelerômetro (Pitch/Roll) ---
        float ax = aceleracao[0] / 16384.0f; // Escala para 'g'
        float ay = aceleracao[1] / 16384.0f;
        float az = aceleracao[2] / 16384.0f;

        float roll = atan2(ay, az) * 180.0f / M_PI;
        float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / M_PI;

        // Atualiza o display apenas se houver mudança significativa em qualquer sensor
        if (fabs(gx - last_gx) > gyro_limiar || fabs(gy - last_gy) > gyro_limiar || fabs(gz - last_gz) > gyro_limiar ||
            fabs(roll - last_roll) > accel_limiar || fabs(pitch - last_pitch) > accel_limiar)
        {

            last_gx = gx;
            last_gy = gy;
            last_gz = gz;
            last_roll = roll;
            last_pitch = pitch;

            ssd1306_fill(&ssd, false); // Limpa o display a cada atualização

            // --- Seção do Giroscópio (Top da Tela) ---
            ssd1306_draw_string(&ssd, "GIRO:", 2, 0); // Título para Giro

            // Barra de GX
            int bar_end_x_gx = center_x_gyro_bar + (int)(gx * gyro_scale_factor);
            if (bar_end_x_gx < 0)
                bar_end_x_gx = 0;
            if (bar_end_x_gx > DISP_W - 1)
                bar_end_x_gx = DISP_W - 1;
            ssd1306_line(&ssd, center_x_gyro_bar, y_pos_gx, bar_end_x_gx, y_pos_gx, true);
            // ssd1306_draw_string(&ssd, "GX", 2, y_pos_gx - 4); // Rótulo do eixo

            // Barra de GY
            int bar_end_x_gy = center_x_gyro_bar + (int)(gy * gyro_scale_factor);
            if (bar_end_x_gy < 0)
                bar_end_x_gy = 0;
            if (bar_end_x_gy > DISP_W - 1)
                bar_end_x_gy = DISP_W - 1;
            ssd1306_line(&ssd, center_x_gyro_bar, y_pos_gy, bar_end_x_gy, y_pos_gy, true);
            // ssd1306_draw_string(&ssd, "GY", 2, y_pos_gy - 4);

            // Barra de GZ
            int bar_end_x_gz = center_x_gyro_bar + (int)(gz * gyro_scale_factor);
            if (bar_end_x_gz < 0)
                bar_end_x_gz = 0;
            if (bar_end_x_gz > DISP_W - 1)
                bar_end_x_gz = DISP_W - 1;
            ssd1306_line(&ssd, center_x_gyro_bar, y_pos_gz, bar_end_x_gz, y_pos_gz, true);
            // ssd1306_draw_string(&ssd, "GZ", 2, y_pos_gz - 4);

            // Linha de divisão entre Giro e Acelerômetro
            ssd1306_line(&ssd, 0, 42, DISP_W - 1, 42, true);

            // --- Seção do Acelerômetro (Bottom da Tela) ---
            ssd1306_draw_string(&ssd, "ACEL:", 2, 45); // Título para Acelerômetro

            // Mapeamento do Roll para a posição X da linha
            int offset_x_roll = (int)(roll * pixels_per_degree_roll);
            int line_x_roll = center_x_accel_line + offset_x_roll;

            if (line_x_roll < 0)
                line_x_roll = 0;
            if (line_x_roll > DISP_W - 1)
                line_x_roll = DISP_W - 1;

            // Mapeamento do Pitch para a posição Y da linha
            int offset_y_pitch = (int)(pitch * pixels_per_degree_pitch);
            int line_y_pitch = y_pos_accel_ref + offset_y_pitch;

            if (line_y_pitch < 43)
                line_y_pitch = 43; // Garante que a linha não invada a área do giro
            if (line_y_pitch > DISP_H - 1)
                line_y_pitch = DISP_H - 1;

            // Linha de referência central para Pitch/Roll
            ssd1306_line(&ssd, center_x_accel_line - (accel_line_length / 8), y_pos_accel_ref, center_x_accel_line + (accel_line_length / 8), y_pos_accel_ref, true); // Eixo X central
            ssd1306_line(&ssd, center_x_accel_line, y_pos_accel_ref - 2, center_x_accel_line, y_pos_accel_ref + 2, true);                                             // Eixo Y central

            // Desenha a linha indicadora do Roll (vertical que se move horizontalmente)
            ssd1306_line(&ssd, line_x_roll, y_pos_accel_ref - (accel_line_length / 2) + 5, line_x_roll, y_pos_accel_ref + (accel_line_length / 2) - 5, true);

            // Desenha a linha indicadora do Pitch (horizontal que se move verticalmente)
            ssd1306_line(&ssd, center_x_accel_line - (accel_line_length / 2) + 5, line_y_pitch, center_x_accel_line + (accel_line_length / 2) - 5, line_y_pitch, true);

            // Exibe os valores numéricos de Roll e Pitch
            char str_roll[20], str_pitch[20];
            snprintf(str_roll, sizeof(str_roll), "R:%5.1f", roll);
            snprintf(str_pitch, sizeof(str_pitch), "P:%5.1f", pitch);
            // ssd1306_draw_string(&ssd, str_roll, 5, 54);
            // ssd1306_draw_string(&ssd, str_pitch, 68, 54);

            // Exibe os valores para a temperatura
            char str_tmp[5];                                   // String para a temperatura
            sprintf(str_tmp, "%.1fC", (temp / 340.0) + 15); // Temperatura em Celsius
            ssd1306_draw_string(&ssd, str_tmp, 80, 0); // Exibe a temperatura no display


            ssd1306_send_data(&ssd);                   // Envia os dados para o display
        }

        sleep_ms(50); // Reduzindo o delay para uma atualização mais fluida
    }
}