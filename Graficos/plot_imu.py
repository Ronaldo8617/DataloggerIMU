import numpy as np
import matplotlib.pyplot as plt

# Nome do arquivo de dados gerado pelo Pico
# Certifique-se de que este arquivo esteja na mesma pasta do script Python
file_name = 'log_001.csv'

# Carregar os dados do arquivo CSV
# np.loadtxt é usado para carregar dados de um arquivo de texto
# delimiter=',' especifica que os valores são separados por vírgulas
# skiprows=1 ignora a primeira linha, que é o cabeçalho
try:
    data = np.loadtxt(file_name, delimiter=',', skiprows=1)
except FileNotFoundError:
    print(f"Erro: O arquivo '{file_name}' não foi encontrado.")
    print("Certifique-se de que o arquivo está na mesma pasta do script Python.")
    exit()
except Exception as e:
    print(f"Ocorreu um erro ao carregar o arquivo: {e}")
    exit()

# Extrair as colunas para variáveis separadas
# data[:, 0] pega todas as linhas da primeira coluna (índice 0)
# e assim por diante para as outras colunas
sample = data[:, 0]
accel_x = data[:, 1]
accel_y = data[:, 2]
accel_z = data[:, 3]
gyro_x = data[:, 4]
gyro_y = data[:, 5]
gyro_z = data[:, 6]

# --- Gerar gráficos individuais para cada variável ---

# Aceleração no eixo X
plt.figure(figsize=(10, 6)) # Define o tamanho da figura
plt.plot(sample, accel_x, 'b-') # 'b-' para linha azul contínua
plt.title('Aceleração no Eixo X (AccelX)')
plt.xlabel('Amostra')
plt.ylabel('Aceleração (unidades brutas)')
plt.grid(True)

# Aceleração no eixo Y
plt.figure(figsize=(10, 6))
plt.plot(sample, accel_y, 'g-') # 'g-' para linha verde contínua
plt.title('Aceleração no Eixo Y (AccelY)')
plt.xlabel('Amostra')
plt.ylabel('Aceleração (unidades brutas)')
plt.grid(True)

# Aceleração no eixo Z
plt.figure(figsize=(10, 6))
plt.plot(sample, accel_z, 'r-') # 'r-' para linha vermelha contínua
plt.title('Aceleração no Eixo Z (AccelZ)')
plt.xlabel('Amostra')
plt.ylabel('Aceleração (unidades brutas)')
plt.grid(True)

# Giroscópio no eixo X
plt.figure(figsize=(10, 6))
plt.plot(sample, gyro_x, 'c-') # 'c-' para linha ciano contínua
plt.title('Giroscópio no Eixo X (GyroX)')
plt.xlabel('Amostra')
plt.ylabel('Velocidade Angular (unidades brutas)')
plt.grid(True)

# Giroscópio no eixo Y
plt.figure(figsize=(10, 6))
plt.plot(sample, gyro_y, 'm-') # 'm-' para linha magenta contínua
plt.title('Giroscópio no Eixo Y (GyroY)')
plt.xlabel('Amostra')
plt.ylabel('Velocidade Angular (unidades brutas)')
plt.grid(True)

# Giroscópio no eixo Z
plt.figure(figsize=(10, 6))
plt.plot(sample, gyro_z, 'y-') # 'y-' para linha amarela contínua
plt.title('Giroscópio no Eixo Z (GyroZ)')
plt.xlabel('Amostra')
plt.ylabel('Velocidade Angular (unidades brutas)')
plt.grid(True)

# Ajustar o layout para evitar sobreposição de títulos/rótulos
plt.tight_layout()

# Mostrar todos os gráficos
plt.show()
