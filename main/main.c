#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

// Constantes fornecidas (mantidas)
const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

// Pinos do sensor ultrassônico
const uint PIN_ECHO = 7;
const uint PIN_TRIGGER = 6;

// Filas e semáforo
QueueHandle_t queueMedidasTempo;
QueueHandle_t queueDistancias;
SemaphoreHandle_t semaforoDisparo;

// Callback de interrupção no ECHO
void tratador_interrupcao_echo(uint gpio, uint32_t eventos) {
    int timestamp = to_us_since_boot(get_absolute_time());

    // Armazena qualquer borda (subida ou descida)
    xQueueSendFromISR(queueMedidasTempo, &timestamp, NULL);
}

// Tarefa que dispara o sinal ultrassônico
void tarefa_disparo_ultrassom(void *param) {

    while (true) {
        if (xSemaphoreTake(semaforoDisparo, pdMS_TO_TICKS(200))) {
            gpio_put(PIN_TRIGGER, 1);
            sleep_us(10); // Pulso de 10us
            gpio_put(PIN_TRIGGER, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Tarefa que mede a distância
void tarefa_medicao_echo(void *param) {

    int tempo1 = 0;
    int tempo2 = 0;

    while (true) {
        // Aguarda dois tempos na fila: subida e descida
        if (xQueueReceive(queueMedidasTempo, &tempo1, pdMS_TO_TICKS(100))) {
            if (xQueueReceive(queueMedidasTempo, &tempo2, pdMS_TO_TICKS(100))) {
                if (tempo2 > tempo1) {
                    int duracao = tempo2 - tempo1;
                    float distanciaCM = (duracao * 0.0343f) / 2.0f;

                    if (distanciaCM < 2 || distanciaCM > 400) {
                        distanciaCM = -1.0f;
                    }

                    xQueueSend(queueDistancias, &distanciaCM, 0);
                }
                xSemaphoreGive(semaforoDisparo); // Pronto para nova medição
            }
        }
    }
}

// Tarefa que exibe a distância no OLED
void tarefa_oled_display(void *param) {
    ssd1306_t display;
    ssd1306_init();
    gfx_init(&display, 128, 32);

    float distancia = 0;
    char texto[32];

    while (true) {
        if (xQueueReceive(queueDistancias, &distancia, pdMS_TO_TICKS(100))) {
            gfx_clear_buffer(&display);

            if (distancia >= 2 && distancia <= 400) {
                snprintf(texto, sizeof(texto), "Distancia: %.2f cm", distancia);
                gfx_draw_string(&display, 0, 10, 1, texto);
            } else {
                gfx_draw_string(&display, 0, 10, 1, "Falha: fora de alcance");
            }

            int barra = (distancia > 112) ? 112 : (int)distancia;
            gfx_draw_line(&display, 0, 27, barra, 27);
        } else {
            gfx_clear_buffer(&display);
            gfx_draw_string(&display, 0, 10, 1, "Falha: erro na leitura");
        }

        gfx_show(&display);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Função principal
int main() {

    gpio_init(PIN_TRIGGER);
    gpio_set_dir(PIN_TRIGGER, GPIO_OUT);

    gpio_init(PIN_ECHO);
    gpio_set_dir(PIN_ECHO, GPIO_IN);
    gpio_set_irq_enabled_with_callback(PIN_ECHO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &tratador_interrupcao_echo);


    stdio_init_all();

    queueMedidasTempo = xQueueCreate(32, sizeof(int));
    queueDistancias = xQueueCreate(32, sizeof(float));
    semaforoDisparo = xSemaphoreCreateBinary();

    xSemaphoreGive(semaforoDisparo);

    xTaskCreate(tarefa_disparo_ultrassom, "Disparo", 4096, NULL, 1, NULL);
    xTaskCreate(tarefa_medicao_echo, "Echo", 4096, NULL, 1, NULL);
    xTaskCreate(tarefa_oled_display, "Display", 4096, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true) {}
}
