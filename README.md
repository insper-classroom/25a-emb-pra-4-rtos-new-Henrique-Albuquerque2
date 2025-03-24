# Controle Customizado para Shell Shockers usando uma pisctola fisica

## Jogo
**Shell Shockers** – Um jogo de tiro multiplayer em primeira pessoa, jogado diretamente no navegador, em que ovos armados participam de partidas um contra o outro, o objetivo é atirar para matar os outros participantes. 

## Ideia do Controle
O controle foi desenvolvido no formato de uma pistola, para criar um entreterimento maior podendo apontar a pistola na direção da sua mira. A mira é controlada por uma IMU (sensor de movimento), que substitui os movimentos do mouse. Os botões físicos da pistola são usados para ações principais do jogo, como atirar, recarregar, pular e trocar de arma.

## Inputs e Outputs

### **Entradas (Inputs)**

- **IMU (MPU6050):**
  - Controle da mira com base na inclinação da pistola (substitui o mouse)

- **4x Entradas Digitais:**
  - **Gatilho:** Tiro (mapeado para clique esquerdo do mouse)
  - **Botão de recarregar:** Tecla “R”
  - **Botão de pulo:** Tecla “Espaço”
  - **Botão de troca de arma:** Tecla “E”

### **Saídas (Outputs)**
- **LED indicador azul:** Conectado ao computador via Bluetooth
- **Vibração:** Ao ser atingido ou recarregar

## Protocolo Utilizado

- **UART** para comunicação serial com o computador.
- **IMU** para leitura de orientação.
- **GPIO** para os botões digitais.

## Diagrama de Blocos Explicativo do Firmware

### **Estrutura Geral**
---

*(inserir imagem do diagrama aqui)*

---

#### **Principais Componentes do RTOS**
- **Tasks:**
  - Task de leitura da IMU
  - Task de leitura dos botões
  - Task de controle visual (LEDs)
  - Task de vibração

- **Filas:**
  - Fila de eventos de entrada (botões)
  - Fila de dados de orientação (IMU)
  - Fila de comandos para feedback

 **Semáforos:**

- Controle do LED de conexão e vibração

- **Interrupts:**
  - Callbacks para os botões digitais

## Imagens do Controle

### **Proposta Inicial**

*(inserir render 3D ou mockup com botões identificados, posição da IMU, LEDs etc)*

