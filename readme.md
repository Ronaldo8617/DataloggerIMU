# 🌦️ Estação Meteorológica Inteligente com Raspberry Pi Pico W

## 📹 Demonstração
🎬 [Assista ao vídeo da demonstração](https://youtu.be/NvOw4scISNc)

O vídeo apresenta:
- Visão geral do projeto
- Interface local via OLED e controle por botões
- Interface web com atualização de dados em tempo real

---

## 🎯 Objetivo do Projeto

Desenvolver uma estação meteorológica capaz de:
- Medir temperatura, umidade, pressão e altitude
- Exibir os dados localmente via display OLED
- Publicar os dados via interface web hospedada no próprio dispositivo
- Emitir alertas visuais e sonoros quando os limites forem ultrapassados

---

## 🛠️ Funcionalidades Obrigatórias

- Leitura dos sensores AHT10 (temperatura/umidade) e BMP280 (temperatura/pressão)
- Cálculo de altitude com base na pressão
- Exibição dos dados no display OLED SSD1306
- Navegação de páginas via botões
- Conexão Wi-Fi e hospedagem de servidor web
- Alertas visuais (LEDs) e sonoros (buzzer)

---

## ✨ Funcionalidades Adicionais

- Interface web embarcada com HTML pré-carregado
- Histórico circular de leituras (com fallback para valores inválidos)
- Detecção de anomalias com notificação no display e LEDs

---

## 📦 Componentes Utilizados

| Componente          | Função                                   |
|---------------------|------------------------------------------|
| Raspberry Pi Pico W | Microcontrolador com Wi-Fi               |
| AHT10               | Sensor de temperatura e umidade          |
| BMP280              | Sensor de pressão e temperatura          |
| OLED SSD1306 (I2C)  | Exibição local das leituras              |
| LED RGB             | Indicação visual de alertas              |
| Matriz de LEDs 5x5  | Indicadores personalizados               |
| Buzzer              | Alerta sonoro                            |
| Push Buttons        | Navegação e controle via interrupção     |

---

## ⚙️ Compilação e Gravação

```bash
git clone https://github.com/Ronaldo8617/EMeteorologica.git
cd EMeteorologica
mkdir build
cd build
cmake ..
make
```
## 🔧 Configuração Wi-Fi
No código, defina as credenciais da sua rede:
#define WIFI_SSID "SUA_REDE"
#define WIFI_PASS "SUA_SENHA"
## 🚀 Gravação na Placa
Compile e execute no VSCode com a placa bitdoglab conectada.
Ou conecte o RP2040 segurando o botão BOOTSEL e copie o arquivo .uf2 da pasta build para o dispositivo montado.
.

## 📂 Estrutura do Projeto
```plaintext
EMeteorologica/
├── lib/
│   ├── font.h               # Fonte para o display
│   ├── ssd1306.c/h          # Driver do display OLED
│   ├── perifericos.h        # Controle dos periefericos 
│   ├── display.h            # Funções de exibição
│   ├── html.h               # Interface web (HTML embarcado)
│   ├── webpage.h            # Gerenciamento da página web
│   ├── matrixws.h           # Controle da matriz de LEDs
│   ├── aht20.c/h            # Biblioteca do sensor AHT20
│   ├── bmp280.c/h           # Biblioteca do sensor BMP280
├── EMeteorologica.c         # Código principal
├── CMakeLists.txt           # Configuração do projeto
└── README.md                # Este arquivo
```
## 👨‍💻 Autor
Nome: Ronaldo César Santos Rocha
GitHub: @Ronaldo8617

