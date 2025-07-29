# Datalogger IMU para Raspberry Pi Pico W

## 📹 Demonstração
🎬 [Assista ao vídeo da demonstração](https://youtu.be/NvOw4scISNc)

---

## 🎯 Objetivo do Projeto

- Desenvolver um dispositivo portátil (datalogger) capaz de:

- Capturar continuamente dados de movimento (aceleração e giroscópio) de um sensor IMU MPU6050.

- Armazenar esses dados de forma estruturada em arquivos .csv em um cartão MicroSD.

- Fornecer feedback em tempo real ao usuário através de um display OLED, LEDs RGB e um buzzer.

- Permitir o controle da gravação e a navegação na tela via botões.

- Possibilitar a posterior análise gráfica dos dados em um computador usando Python.

---

## 🛠️ Funcionalidades Obrigatórias

- Captura de Dados IMU: Leitura contínua dos dados de aceleração (eixos X, Y, Z) e giroscópio (eixos X, Y, Z) do sensor MPU6050 via I2C0.

- Armazenamento em Cartão SD: Salvamento dos dados em formato .csv em arquivos sequenciais (ex: log_000.csv, log_001.csv) no cartão MicroSD, utilizando a biblioteca FatFs.

- Interface Local (Display OLED SSD1306): Exibição de informações cruciais em tempo real, como:

- Status do sistema (Ex: "Inicializando", "Aguardando", "Gravando...", "SD Nao Detectado").

- Contador de amostras coletadas e tempo de gravação.

- Dados brutos de aceleração, giroscópio e temperatura do IMU.

- Feedback de ações do usuário (Ex: "Dados Salvos!").

- Feedback Visual (LED RGB): Sinalização visual dos principais estados de operação do sistema:

- Amarelo: Sistema inicializando / Montando cartão SD.

- Verde: Sistema pronto para iniciar a captura / Dados salvos.

- Vermelho: Captura de dados em andamento.

- Azul (piscando): Acessando o cartão SD (leitura/gravação).

- Roxo (piscando): Erro (Ex: Falha ao montar o cartão SD).

- Alertas Sonoros (Buzzer): Emissão de bipes curtos para confirmar ações do usuário:

- Um beep curto: Para "iniciar captura".

- Dois beeps curtos: Para "parar captura".

- Controle por Botões: Utilização de push buttons para controle total do dispositivo:

- Botão A: Iniciar/Parar gravação de dados e alternar entre as páginas do display OLED.

- Botão B: Entrar no modo BOOTSEL do Raspberry Pi Pico W para regravação do firmware.

---

## 📦 Componentes Utilizados

- Raspberry Pi Pico W        
- MPU6050             
- OLED SSD1306 (I2C)              
- LED RGB                       
- Buzzer                                      
- Push Buttons                           
- Cartão MicroSD                      
- Módulo Leitor SD          
---

## ⚙️ Compilação e Gravação

```bash
git clone https://github.com/Ronaldo8617/DataloggerIMU.git
cd DataloggerIMU
mkdir build
cd build
cmake ..
make
```

## 🚀 Gravação na Placa
Compile e execute no VSCode com a placa bitdoglab conectada.
Ou conecte o RP2040 segurando o botão BOOTSEL e copie o arquivo .uf2 da pasta build para o dispositivo montado.
.

## 📂 Estrutura do Projeto
```plaintext
EMeteorologica/
├── lib/
│   ├── font.h              # Fonte para o display OLED
│   ├── ssd1306.c/h         # Driver do display OLED
│   ├── hw_config.h         # Configuração de hardware para o SD (SPI)
│   ├── my_debug.h          # Funções de depuração
│   ├── sd_card.h           # Driver para o cartão SD
│   ├── ff.h                # Biblioteca FatFs (sistema de arquivos)
│   ├── diskio.h            # Funções de E/S de disco para FatFs
│   └── f_util.h            # Utilitários para FatFs
├── DataloggerIMU.c         # Código principal do datalogger
├── CMakeLists.txt          # Configuração do projeto (CMake)
└── README.md               # Este arquivo
```
## 👨‍💻 Autor
Nome: Ronaldo César Santos Rocha
GitHub: @Ronaldo8617

