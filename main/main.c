/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/timer.h"

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

// === Pinos do sensor ultrassônico ===
#define TRIGGER_PIN 3
#define ECHO_PIN 2

// === Definição dos recursos RTOS ===
SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;

typedef struct {
    uint64_t start;
    uint64_t end;
} echo_time_t;

volatile bool rising = true;
echo_time_t echo;

// === Callback do pino de interrupção (ECHO) ===
void pin_callback(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_RISE) {
        echo.start = to_us_since_boot(get_absolute_time());
        rising = false;
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        echo.end = to_us_since_boot(get_absolute_time());
        rising = true;

        xQueueSendFromISR(xQueueTime, &echo, NULL);
    }
}

// === Task: envia o pulso TRIGGER ===
void trigger_task(void *p) {
    while (1) {
        gpio_put(TRIGGER_PIN, 1);
        sleep_us(10);
        gpio_put(TRIGGER_PIN, 0);

        xSemaphoreGive(xSemaphoreTrigger); // Avisa o OLED que teve leitura

        vTaskDelay(pdMS_TO_TICKS(100)); // Espera entre medições
    }
}

// === Task: calcula a distância com base no tempo do Echo ===
void echo_task(void *p) {
    echo_time_t time;
    while (1) {
        if (xQueueReceive(xQueueTime, &time, portMAX_DELAY)) {
            uint64_t duration = time.end - time.start;
            float distance_cm = (duration * 0.0343f) / 2.0f;

            if (distance_cm < 400.0f) {
                xQueueSend(xQueueDistance, &distance_cm, portMAX_DELAY);
            } else {
                float erro = -1.0f;
                xQueueSend(xQueueDistance, &erro, portMAX_DELAY);
            }
        }
    }
}

// === Task: exibe a distância ou erro no display OLED ===
void oled_task(void *p) {
    ssd1306_t disp;
    ssd1306_init();
    gfx_init(&disp, 128, 32);

    float distance = 0;

    while (1) {
        if (xQueueReceive(xQueueDistance, &distance, pdMS_TO_TICKS(300))) {
            gfx_clear_buffer(&disp);

            if (distance < 0) {
                gfx_draw_string(&disp, 0, 0, 1, "Erro na leitura");
            } else {
                char buf[32];
                snprintf(buf, sizeof(buf), "Distancia: %.1f cm", distance);
                gfx_draw_string(&disp, 0, 0, 1, buf);

                int bar = distance > 100 ? 100 : (int)distance;
                gfx_draw_line(&disp, 0, 25, bar, 25);
            }

            gfx_show(&disp);
        }
    }
}

// === Função principal ===
int main() {
    stdio_init_all();

    // Configura os pinos do sensor
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_pull_down(ECHO_PIN);

    gpio_set_irq_enabled_with_callback(ECHO_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    // Inicializa recursos RTOS
    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xQueueTime = xQueueCreate(4, sizeof(echo_time_t));
    xQueueDistance = xQueueCreate(4, sizeof(float));

    // Cria as tasks
    xTaskCreate(trigger_task, "Trigger", 1024, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 1024, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 2048, NULL, 1, NULL);

    // Inicia o scheduler
    vTaskStartScheduler();

    // Nunca deve chegar aqui
    while (true)
        tight_loop_contents();
}