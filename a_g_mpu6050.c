#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "font.h"

// Definição dos pinos I2C para o MPU6050
#define I2C_PORT i2c0 // i2c0 usa pinos 0 e 1, i2c1 usa pinos 2 e 3
#define I2C_SDA 0     // SDA pode ser 0 ou 2
#define I2C_SCL 1     // SCL pode ser 1 ou 3

// Definição dos pinos I2C para o display OLED
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C // Endereço I2C do display

// Endereço padrão do MPU6050 (IMU)
static int addr = 0x68;

// Função para resetar o MPU6050
static void mpu6050_reset()
{
    // Dois bytes para reset: primeiro o registrador, segundo o dado
    // Existem muitas outras opções de configuração do dispositivo, se necessário
    uint8_t buf[] = {0x6B, 0x80};
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(100); // Aguarda o reset e estabilização do dispositivo

    // Sai do modo sleep (registrador 0x6B, valor 0x00)
    buf[1] = 0x00; // Sai do sleep escrevendo 0x00 no registrador 0x6B
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    sleep_ms(10); // Aguarda estabilização após acordar
}

// Função para ler dados crus (raw) do MPU6050: aceleração, giroscópio e temperatura
static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp)
{
    // Para este dispositivo, enviamos o endereço do registrador a ser lido
    // e depois lemos os dados. O registrador auto-incrementa, então basta enviar o inicial.

    uint8_t buffer[6];

    // Lê os dados de aceleração a partir do registrador 0x3B (6 bytes)
    uint8_t val = 0x3B;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true); // true mantém o controle do barramento
    i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);

    for (int i = 0; i < 3; i++)
    {
        accel[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);
    }

    // Lê dados do giroscópio a partir do registrador 0x43 (6 bytes)
    // O registrador auto-incrementa a cada leitura
    val = 0x43;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 6, false);

    for (int i = 0; i < 3; i++)
    {
        gyro[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);
    }

    // Lê temperatura a partir do registrador 0x41 (2 bytes)
    val = 0x41;
    i2c_write_blocking(I2C_PORT, addr, &val, 1, true);
    i2c_read_blocking(I2C_PORT, addr, buffer, 2, false);

    *temp = buffer[0] << 8 | buffer[1];
}

// Trecho para modo BOOTSEL usando o botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Inicialização do modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // Fim da configuração do BOOTSEL

    stdio_init_all();
    sleep_ms(5000);

    // Inicializa a I2C do Display OLED em 400kHz
    i2c_init(I2C_PORT_DISP, 400 * 1000);

    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);    // Define função I2C no pino SDA
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);    // Define função I2C no pino SCL
    gpio_pull_up(I2C_SDA_DISP);                        // Habilita pull-up no SDA
    gpio_pull_up(I2C_SCL_DISP);                        // Habilita pull-up no SCL
    ssd1306_t ssd;                                     // Estrutura para o display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP); // Inicializa o display
    ssd1306_config(&ssd);                              // Configura o display
    ssd1306_send_data(&ssd);                           // Envia dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicialização da I2C do MPU6050
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Declara os pinos como I2C na Binary Info para depuração
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    mpu6050_reset();

    int16_t aceleracao[3], gyro[3], temp;  // Variáveis para armazenar os dados lidos do MPU6050
    char str_tmp[5];    // String para a temperatura
    char str_acel_x[5]; // String para aceleração X
    char str_acel_y[5]; // String para aceleração Y
    char str_acel_z[5]; // String para aceleração Z
    char str_giro_x[5]; // String para giroscópio X
    char str_giro_y[5]; // String para giroscópio Y
    char str_giro_z[5]; // String para giroscópio Z    

    bool cor = true;
    while (1)
    {
        // Lê dados de aceleração, giroscópio e temperatura do MPU6050
        mpu6050_read_raw(aceleracao, gyro, &temp);

        sprintf(str_tmp, "%.1fC", (temp / 340.0) + 36.53); // Converte o valor inteiro de temperatura em string
        sprintf(str_acel_x, "%d", aceleracao[0]);           // Converte aceleração X para string
        sprintf(str_acel_y, "%d", aceleracao[1]);           // Converte aceleração Y para string
        sprintf(str_acel_z, "%d", aceleracao[2]);           // Converte aceleração Z para string

        // Atualiza o conteúdo do display com informações e gráficos
        ssd1306_fill(&ssd, !cor);                            // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);        // Desenha um retângulo
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);             // Desenha uma linha horizontal
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);             // Desenha outra linha horizontal
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);   // Escreve texto no display
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);    // Escreve texto no display
        ssd1306_draw_string(&ssd, "IMU    MPU6050", 10, 28); // Escreve texto no display
        ssd1306_line(&ssd, 63, 25, 63, 60, cor);             // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, str_tmp, 14, 41);          // Exibe temperatura
        ssd1306_draw_string(&ssd, str_acel_x, 14, 52);       // Exibe aceleração X
        ssd1306_draw_string(&ssd, str_acel_y, 73, 41);       // Exibe aceleração Y
        ssd1306_draw_string(&ssd, str_acel_z, 73, 52);       // Exibe aceleração Z
        ssd1306_send_data(&ssd);                             // Atualiza o display

        sleep_ms(500); // Aguarda 500ms antes da próxima leitura/atualização
    }
}
