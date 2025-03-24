#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

// Constantes dos botões e LEDs (mantidas conforme solicitado)
const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

// Pinos do sensor ultrassônico
const uint PIN_ECHO = 7;
const uint PIN_TRIGGER = 6;

// Filas e semáforo para comunicação entre tarefas
QueueHandle_t queueMedidasTempo;
QueueHandle_t queueDistancias;
SemaphoreHandle_t semaforoDisparo;

// Flag de estado para borda de subida/descida
volatile bool aguardandoSubida = true;

// Interrupção do pino de ECHO
void tratador_interrupcao_echo(uint gpio, uint32_t eventos) {
    static int instanteSubida = 0;
    int tempoAtual = to_us_since_boot(get_absolute_time());

    if (aguardandoSubida && (eventos & GPIO_IRQ_EDGE_RISE)) {
        instanteSubida = tempoAtual;
        aguardandoSubida = false;
    } else if (!aguardandoSubida && (eventos & GPIO_IRQ_EDGE_FALL)) {
        int duracaoPulso = tempoAtual - instanteSubida;
        xQueueSendFromISR(queueMedidasTempo, &duracaoPulso, NULL);
        aguardandoSubida = true;
    }
}

// Tarefa que dispara o sinal ultrassônico
void tarefa_disparo_ultrassom(void *param) {
    while (true) {
        if (xSemaphoreTake(semaforoDisparo, pdMS_TO_TICKS(200))) {
            gpio_put(PIN_TRIGGER, 1);
            sleep_us(10); // Pulso de 10 microsegundos
            gpio_put(PIN_TRIGGER, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Tarefa que mede a resposta do sensor
void tarefa_medicao_echo(void *param) {
    int tempoPulso = 0;

    while (true) {
        if (xQueueReceive(queueMedidasTempo, &tempoPulso, pdMS_TO_TICKS(100))) {
            float distanciaCM = (tempoPulso * 0.0343f) / 2.0f;

            if (distanciaCM < 2 || distanciaCM > 400) {
                distanciaCM = -1.0f;
            }

            xQueueSend(queueDistancias, &distanciaCM, 0);
            xSemaphoreGive(semaforoDisparo);
        }
    }
}

// Tarefa que exibe as informações no display OLED
void tarefa_oled_display(void *param) {
    ssd1306_t display;
    ssd1306_init();
    gfx_init(&display, 128, 32);

    float valorDistancia = 0.0f;
    char mensagem[32];

    while (true) {
        if (xQueueReceive(queueDistancias, &valorDistancia, pdMS_TO_TICKS(100))) {
            gfx_clear_buffer(&display);

            if (valorDistancia >= 2 && valorDistancia <= 400) {
                snprintf(mensagem, sizeof(mensagem), "Distancia: %.2f cm", valorDistancia);
                gfx_draw_string(&display, 0, 10, 1, mensagem);
            } else {
                gfx_draw_string(&display, 0, 10, 1, "Erro: fora de alcance");
            }

            int tamanhoBarra = (valorDistancia > 112) ? 112 : (int)valorDistancia;
            gfx_draw_line(&display, 0, 27, tamanhoBarra, 27);
        } else {
            gfx_clear_buffer(&display);
            gfx_draw_string(&display, 0, 10, 1, "Erro: sem leitura");
        }

        gfx_show(&display);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int main() {
    stdio_init_all();

    gpio_init(PIN_ECHO);
    gpio_set_dir(PIN_ECHO, GPIO_IN);
    gpio_set_irq_enabled_with_callback(PIN_ECHO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &tratador_interrupcao_echo);

    gpio_init(PIN_TRIGGER);
    gpio_set_dir(PIN_TRIGGER, GPIO_OUT);

    // Inicialização das filas e semáforo
    queueMedidasTempo = xQueueCreate(32, sizeof(int));
    queueDistancias = xQueueCreate(32, sizeof(float));
    semaforoDisparo = xSemaphoreCreateBinary();

    // Libera o semáforo para o primeiro disparo
    xSemaphoreGive(semaforoDisparo);

    // Criação das tarefas do FreeRTOS
    xTaskCreate(tarefa_disparo_ultrassom, "DisparoUltrassom", 4096, NULL, 1, NULL);
    xTaskCreate(tarefa_medicao_echo, "MedicaoEcho", 4096, NULL, 1, NULL);
    xTaskCreate(tarefa_oled_display, "OLED_Display", 4096, NULL, 1, NULL);

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();

    while (true) {
        // Loop infinito caso o escalonador falhe
    }
}
