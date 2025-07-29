#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h" // Para timestamp, se desejado
#include "pico/time.h"    // Para get_absolute_time
#include "pico/bootrom.h" //

// Incluir as bibliotecas do FatFs e do SD Card
#include "ff.h"
#include "diskio.h"
#include "f_util.h"
#include "hw_config.h" // Assumindo que este arquivo configura o SPI para o SD
#include "my_debug.h"  // Para DBG_PRINTF e myASSERT
#include "sd_card.h"

// Incluir as bibliotecas do display OLED
#include "ssd1306.h"
#include "font.h" // Assumindo que font.h define WIDTH e HEIGHT ou são passados

// --- Definições de Pinos ---

// MPU6050 (I2C0)
#define I2C_PORT_MPU i2c0
#define I2C_SDA_MPU 0
#define I2C_SCL_MPU 1
#define MPU6050_ADDR 0x68

// Display OLED SSD1306 (I2C1)
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define OLED_ADDR 0x3C
#define OLED_WIDTH 128 // Assumindo 128x64, ajuste se necessário
#define OLED_HEIGHT 64

// LEDs RGB (assumindo pinos do PERIFERICOS.H)
#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12

// Buzzer (assumindo pino do PERIFERICOS.H)
#define BUZZER_PIN 21

// Botões (assumindo pinos do PERIFERICOS.H)
#define BOTAO_A_PIN 5 // Para controle de funções (iniciar/parar/mudar tela)
#define BOTAO_B_PIN 6 // Para modo BOOTSEL

// --- Variáveis Globais ---
ssd1306_t ssd; // Instância do display OLED
FATFS fs;      // Instância do sistema de arquivos FatFs
FIL log_file;  // Instância do arquivo de log
bool sd_card_mounted = false;
uint32_t sample_counter = 0;
absolute_time_t recording_start_time;
bool recording_active = false;
uint8_t current_display_page = 0; // 0: Status, 1: Dados IMU

// --- Enumeração de Estados do Sistema ---
typedef enum {
    SYS_INITIALIZING,
    SYS_SD_NOT_DETECTED,
    SYS_READY,
    SYS_RECORDING,
    SYS_DATA_SAVED,
    SYS_ERROR
} SystemState;

SystemState current_system_state = SYS_INITIALIZING;

// --- Protótipos de Funções ---
// Funções de Periféricos (Botoes, LEDs, Buzzer)
void set_led_color(bool r, bool g, bool b);
void blink_led(bool r, bool g, bool b, uint32_t delay_ms, uint32_t num_blinks); 
void tocar_nota(int frequencia, int duracao);
void beep_curto();
void beep_duplo();
void init_peripherals();

// Funções do MPU6050
static void mpu6050_reset();
static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp);

// Funções do Cartão SD
char* get_next_log_filename();
bool mount_sd_card();
bool unmount_sd_card();

// Funções do Display OLED
void update_display();

// Funções de Controle de Botões
bool is_button_pressed(uint gpio_pin, uint64_t *last_press_time); 
void gpio_irq_handler_bootsel(uint gpio, uint32_t events); // Para o modo BOOTSEL

// --- Funções de Periféricos (Botoes, LEDs, Buzzer) ---

// Funções para LEDs
void set_led_color(bool r, bool g, bool b) {
    gpio_put(LED_RED_PIN, r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN, b);
}

void blink_led(bool r, bool g, bool b, uint32_t delay_ms, uint32_t num_blinks) { 
    for (uint32_t i = 0; i < num_blinks; i++) { 
        set_led_color(r, g, b);
        sleep_ms(delay_ms);
        set_led_color(false, false, false); // Desliga
        sleep_ms(delay_ms);
    }
}

// Funções para Buzzer (do PERIFERICOS.H)
void tocar_nota(int frequencia, int duracao) {
    if (frequencia == 0) {
        gpio_put(BUZZER_PIN, 0);
        return;
    }
    int periodo = 1000000 / frequencia; // Periodo em microssegundos
    int metade_periodo = periodo / 2;
    for (int i = 0; i < duracao * 1000L / periodo; i++) {
        gpio_put(BUZZER_PIN, 1);
        sleep_us(metade_periodo);
        gpio_put(BUZZER_PIN, 0);
        sleep_us(metade_periodo);
    }
}

void beep_curto() {
    tocar_nota(500, 80);
    sleep_ms(50); // Pequena pausa para garantir o fim do beep
    tocar_nota(0, 0); // Desliga o buzzer
}

void beep_duplo() {
    tocar_nota(500, 80);
    sleep_ms(100);
    tocar_nota(500, 80);
    sleep_ms(50);
    tocar_nota(0, 0);
}

// Inicialização de botões, LEDs e buzzer
void init_peripherals() {
    // Botões
    gpio_init(BOTAO_A_PIN);
    gpio_set_dir(BOTAO_A_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_A_PIN);

    gpio_init(BOTAO_B_PIN);
    gpio_set_dir(BOTAO_B_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_B_PIN);

    // LEDs
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    set_led_color(false, false, false); // Desliga todos os LEDs

    // Buzzer
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, 0); // Garante que o buzzer esteja desligado
}

// --- Funções do MPU6050 ---

static void mpu6050_reset() {
    uint8_t buf[] = {0x6B, 0x80}; // Reset
    i2c_write_blocking(I2C_PORT_MPU, MPU6050_ADDR, buf, 2, false);
    sleep_ms(100);
    buf[1] = 0x00; // Sair do modo sleep
    i2c_write_blocking(I2C_PORT_MPU, MPU6050_ADDR, buf, 2, false);
    sleep_ms(10);
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    uint8_t buffer[6];
    uint8_t val;

    val = 0x3B; // Aceleração
    i2c_write_blocking(I2C_PORT_MPU, MPU6050_ADDR, &val, 1, true);
    i2c_read_blocking(I2C_PORT_MPU, MPU6050_ADDR, buffer, 6, false);
    for (int i = 0; i < 3; i++) {
        accel[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];
    }

    val = 0x43; // Giroscópio
    i2c_write_blocking(I2C_PORT_MPU, MPU6050_ADDR, &val, 1, true);
    i2c_read_blocking(I2C_PORT_MPU, MPU6050_ADDR, buffer, 6, false);
    for (int i = 0; i < 3; i++) {
        gyro[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];
    }

    val = 0x41; // Temperatura
    i2c_write_blocking(I2C_PORT_MPU, MPU6050_ADDR, &val, 1, true);
    i2c_read_blocking(I2C_PORT_MPU, MPU6050_ADDR, buffer, 2, false);
    *temp = (buffer[0] << 8) | buffer[1];
}

// --- Funções do Cartão SD ---

// Função auxiliar para obter a próxima numeração de arquivo de log
char* get_next_log_filename() {
    static char filename[32];
    uint32_t file_idx = 0;
    FIL test_file;
    FRESULT fr;

    do {
        sprintf(filename, "log_%03lu.csv", file_idx++);
        fr = f_open(&test_file, filename, FA_READ);
        if (fr == FR_OK) {
            f_close(&test_file);
        }
    } while (fr == FR_OK); // Continua procurando até encontrar um nome não existente

    return filename;
}

bool mount_sd_card() {
    FRESULT fr = f_mount(&fs, "0:", 1); // "0:" é o nome lógico do drive
    if (FR_OK != fr) {
        DBG_PRINTF("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        sd_card_mounted = false;
        return false;
    }
    sd_card_mounted = true;
    return true;
}

bool unmount_sd_card() {
    FRESULT fr = f_unmount("0:");
    if (FR_OK != fr) {
        DBG_PRINTF("f_unmount error: %s (%d)\n", FRESULT_str(fr), fr);
        return false;
    }
    sd_card_mounted = false;
    return true;
}

// --- Funções do Display OLED ---

void update_display() {
    ssd1306_fill(&ssd, false); // Limpa o display

    char line_buffer[25]; // Buffer para as linhas de texto

    if (current_display_page == 0) { // Página de Status
        ssd1306_draw_string(&ssd, "DATALOGGER IMU", 0, 0);
        ssd1306_draw_string(&ssd, "----------------", 0, 10);

        switch (current_system_state) {
            case SYS_INITIALIZING:
                ssd1306_draw_string(&ssd, "Inicializando...", 0, 25);
                break;
            case SYS_SD_NOT_DETECTED:
                ssd1306_draw_string(&ssd, "SD Nao Detectado", 0, 25);
                ssd1306_draw_string(&ssd, "Verifique Conexao", 0, 35);
                break;
            case SYS_READY:
                ssd1306_draw_string(&ssd, "Pronto para Gravar", 0, 25);
                ssd1306_draw_string(&ssd, "Pressione BOTAO A", 0, 35);
                break;
            case SYS_RECORDING:
                ssd1306_draw_string(&ssd, "Gravando...", 0, 25);
                sprintf(line_buffer, "Amostras: %lu", sample_counter);
                ssd1306_draw_string(&ssd, line_buffer, 0, 35);
                uint64_t elapsed_ms = absolute_time_diff_us(recording_start_time, get_absolute_time()) / 1000;
                sprintf(line_buffer, "Tempo: %lu s", elapsed_ms / 1000);
                ssd1306_draw_string(&ssd, line_buffer, 0, 45);
                break;
            case SYS_DATA_SAVED:
                ssd1306_draw_string(&ssd, "Dados Salvos!", 0, 25);
                sprintf(line_buffer, "Amostras: %lu", sample_counter);
                ssd1306_draw_string(&ssd, line_buffer, 0, 35);
                ssd1306_draw_string(&ssd, "Pressione BOTAO A", 0, 45);
                break;
            case SYS_ERROR:
                ssd1306_draw_string(&ssd, "ERRO NO SISTEMA!", 0, 25);
                ssd1306_draw_string(&ssd, "Reinicie o Pico", 0, 35);
                break;
        }
    } else if (current_display_page == 1) { // Página de Dados IMU (em tempo real)
        ssd1306_draw_string(&ssd, "DADOS IMU", 0, 0);
        ssd1306_draw_string(&ssd, "----------------", 0, 10);

        int16_t accel[3], gyro[3], temp;
        mpu6050_read_raw(accel, gyro, &temp);

        sprintf(line_buffer, "Ax: %d Ay: %d Az: %d", accel[0], accel[1], accel[2]);
        ssd1306_draw_string(&ssd, line_buffer, 0, 25);

        sprintf(line_buffer, "Gx: %d Gy: %d Gz: %d", gyro[0], gyro[1], gyro[2]);
        ssd1306_draw_string(&ssd, line_buffer, 0, 35);

        float temp_c = temp / 340.0 + 36.53; // Conversão típica para MPU6050
        sprintf(line_buffer, "Temp: %.1f C", temp_c);
        ssd1306_draw_string(&ssd, line_buffer, 0, 45);
    }

    ssd1306_send_data(&ssd);
}

// --- Funções de Controle de Botões ---

// Variáveis para debouncing
static uint64_t last_button_a_press_time = 0;
static uint64_t last_button_b_press_time = 0;
const uint64_t debounce_delay_ms = 200; // 200 ms

bool is_button_pressed(uint gpio_pin, uint64_t *last_press_time) { // CORRIGIDO: uint66_t para uint64_t
    if (!gpio_get(gpio_pin)) { // Botão pressionado (pull-up, então LOW quando pressionado)
        uint64_t current_time = to_ms_since_boot(get_absolute_time());
        if (current_time - *last_press_time > debounce_delay_ms) {
            *last_press_time = current_time;
            return true;
        }
    }
    return false;
}

// --- Handler para BOOTSEL (Botão B) ---
void gpio_irq_handler_bootsel(uint gpio, uint32_t events) {
    if (gpio == BOTAO_B_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        // Apenas se o botão B for pressionado (borda de descida)
        reset_usb_boot(0, 0);
    }
}

// --- Função Principal ---
int main() {
    stdio_init_all();
    sleep_ms(2000); // Pequeno atraso para inicialização serial

    // Inicializa periféricos (botões, LEDs, buzzer)
    init_peripherals();

    // Configura interrupção para BOOTSEL
    gpio_set_irq_enabled_with_callback(BOTAO_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler_bootsel);

    // Inicializa I2C para MPU6050
    i2c_init(I2C_PORT_MPU, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA_MPU, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_MPU, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_MPU);
    gpio_pull_up(I2C_SCL_MPU);
    mpu6050_reset(); // Reseta e inicializa o MPU6050

    // Inicializa I2C para Display OLED
    i2c_init(I2C_PORT_DISP, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    // Inicializa o display SSD1306
    ssd1306_init(&ssd, OLED_WIDTH, OLED_HEIGHT, false, OLED_ADDR, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // --- Loop Principal da Máquina de Estados ---
    while (true) {
        // Lógica de Botão A para controle de estado e navegação
        if (is_button_pressed(BOTAO_A_PIN, &last_button_a_press_time)) {
            if (current_system_state == SYS_READY) {
                // Inicia a gravação
                beep_curto();
                current_system_state = SYS_RECORDING;
                recording_active = true;
                sample_counter = 0;
                recording_start_time = get_absolute_time();
                // Abre o arquivo de log
                char* filename = get_next_log_filename();
                set_led_color(false, false, true); // Azul piscando para acesso ao SD
                blink_led(false, false, true, 100, 2); // 2 piscadas rápidas
                FRESULT fr = f_open(&log_file, filename, FA_WRITE | FA_CREATE_ALWAYS);
                if (fr == FR_OK) {
                    // Escreve o cabeçalho CSV
                    f_printf(&log_file, "Sample,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ\n");
                } else {
                    current_system_state = SYS_ERROR;
                    DBG_PRINTF("Erro ao abrir arquivo de log: %s (%d)\n", FRESULT_str(fr), fr);
                }
            } else if (current_system_state == SYS_RECORDING) {
                // Para a gravação
                beep_duplo();
                recording_active = false;
                set_led_color(false, false, true); // Azul piscando para acesso ao SD
                blink_led(false, false, true, 100, 2); // 2 piscadas rápidas
                f_close(&log_file); // Fecha o arquivo
                current_system_state = SYS_DATA_SAVED;
            } else if (current_system_state == SYS_DATA_SAVED || current_system_state == SYS_SD_NOT_DETECTED || current_system_state == SYS_ERROR) {
                // Volta para o estado READY ou tenta remontar SD
                current_system_state = SYS_INITIALIZING; // Tenta re-inicializar
            }

            // Lógica de navegação de página do display (sempre que o botão A for pressionado e não estiver gravando)
            if (!recording_active) {
                current_display_page = (current_display_page + 1) % 2; // Alterna entre página 0 e 1
            }
        }

        // --- Lógica da Máquina de Estados ---
        switch (current_system_state) {
            case SYS_INITIALIZING:
                set_led_color(true, true, false); // Amarelo
                update_display();
                if (mount_sd_card()) {
                    current_system_state = SYS_READY;
                } else {
                    current_system_state = SYS_SD_NOT_DETECTED;
                    blink_led(true, false, true, 200, 5); // Roxo piscando para erro
                }
                break;

            case SYS_SD_NOT_DETECTED:
                set_led_color(true, false, true); // Roxo
                update_display();
                sleep_ms(1000); // Espera antes de tentar novamente ou para o usuário ver
                // O usuário pode pressionar A para tentar re-inicializar
                break;

            case SYS_READY:
                set_led_color(false, true, false); // Verde
                update_display();
                // Espera pelo botão A para iniciar a gravação
                break;

            case SYS_RECORDING: {
                set_led_color(true, false, false); // Vermelho
                update_display(); // Atualiza display com contador

                int16_t accel[3], gyro[3], temp;
                mpu6050_read_raw(accel, gyro, &temp);

                char data_line[100];
                sprintf(data_line, "%lu,%d,%d,%d,%d,%d,%d\n",
                                    sample_counter, accel[0], accel[1], accel[2],
                                    gyro[0], gyro[1], gyro[2]);

                UINT bw;
                set_led_color(false, false, true); // Azul piscando para acesso ao SD
                FRESULT fr = f_write(&log_file, data_line, strlen(data_line), &bw);
                set_led_color(true, false, false); // Volta para vermelho
                if (fr != FR_OK || bw != strlen(data_line)) {
                    current_system_state = SYS_ERROR;
                    DBG_PRINTF("Erro ao escrever no SD: %s (%d)\n", FRESULT_str(fr), fr);
                    f_close(&log_file);
                } else {
                    sample_counter++;
                }
                sleep_ms(100); // Intervalo entre amostras (100ms)
                break;
            }

            case SYS_DATA_SAVED:
                set_led_color(false, true, false); // Verde
                update_display();
                sleep_ms(1000); // Exibe "Dados Salvos!" por um tempo
                // Espera pelo botão A para voltar ao READY
                break;

            case SYS_ERROR:
                set_led_color(true, false, true); // Roxo
                update_display();
                blink_led(true, false, true, 200, 1); // Piscada contínua de erro
                sleep_ms(500); // Pequeno delay para o piscar
                break;
        }
    }
    return 0;
}
