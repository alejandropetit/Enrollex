/**
 * @file main.c
 * @brief Programa principal para la Máquina Bobinadora de Hilo.
 *
 * Este archivo contiene la lógica principal para controlar una máquina bobinadora de hilo
 * utilizando una Raspberry Pi Pico. Integra una pantalla OLED, un encoder óptico,
 * un servomotor, un motor de corriente continua, un encoder rotatorio con un botón,
 * y una celda de carga simulada (HX711) para monitoreo de tensión.
 *
 * @authors Gabriel Restrepo, Andrés Rodríguez, Alejandro Petit
 * @date 17 de julio de 2025
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include <math.h>
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "ssd1306.h"    // Librería de la pantalla OLED
#include <string.h>

// --- Definiciones de Pines ---
/** @defgroup PinDefinitions Definiciones de Pines
 * @{
 */
#define HX711_DT    16  ///< Pin GPIO para el Dato del HX711
#define HX711_SCK   17  ///< Pin GPIO para el Reloj Serial del HX711

#define SERVO_PWM   18  ///< Pin GPIO para la salida PWM del Servo

#define OPT_ENCODER_DT 15 ///< Pin GPIO para el Dato del Encoder Óptico

#define OLED_SCL 13     ///< Pin GPIO para el Reloj Serial I2C del OLED
#define OLED_SDA 12     ///< Pin GPIO para el Dato Serial I2C del OLED

#define ROT_SW   11     ///< Pin GPIO para el Interruptor del Encoder Rotatorio
#define ROT_DT   10     ///< Pin GPIO para el Dato (DT) del Encoder Rotatorio
#define ROT_CLK  9      ///< Pin GPIO para el Reloj (CLK) del Encoder Rotatorio

#define MOTOR_EN  0     ///< Pin GPIO para Habilitar el Motor (control de Puente H)
/** @} */ // fin de PinDefinitions

// --- Constantes de Calibración y Conversión ---
/** @defgroup Constantes Constantes
 * @{
 */
#define PULSOS_POR_VUELTA 100         ///< Número de pulsos por vuelta del encoder óptico.
#define DIAMETRO_TAMBOR_CM 1.4        ///< Diámetro del tambor de bobinado en centímetros.
#define CM_POR_VUELTA (3.1416 * DIAMETRO_TAMBOR_CM) ///< Centímetros de hilo bobinados por vuelta del tambor.
#define PULSOS_POR_CM (PULSOS_POR_VUELTA / CM_POR_VUELTA) ///< Pulsos requeridos por centímetro de hilo.
#define PULSOS_POR_METRO (PULSOS_POR_CM * 100) ///< Pulsos requeridos por metro de hilo.
#define I2C_PORT i2c0                 ///< Instancia del periférico I2C utilizado para el OLED.
#define PWM_DUTY_CYCLE 30             ///< No se usa explícitamente en la configuración del servo, pero es una constante PWM común.
/** @} */ // fin de Constantes

// --- Variables Globales ---
/** @defgroup GlobalVariables Variables Globales
 * @{
 */
volatile int menu_state = 0;    ///< Estado actual del menú principal (0: Hilo, 1: Cobre).
volatile int sub_state = 0;     ///< Estado actual del submenú (0: Manual, 1: Auto, 2: Volver).
volatile int pulsos_encoder = 0; ///< Contador de pulsos del encoder óptico.
/** @} */ // fin de GlobalVariables

// --- Prototipos de Funciones ---
void init_gpio();
void setup_servo();
void set_servo_angle(int angle);
void mover_servo_oscilando(int min_angle, int max_angle, int pause_ms);
int leer_fuerza();
void enrollar_auto();
int seleccionar_mHenrios();
int seleccionar_metros();
void enrollar_hasta(int metros_deseados);
int calcular_vueltas_para_mH(int milihenrios);
void enrollar_cobre_manual(int milihenrios);
void enrollar_cobre_auto();
void mostrar_menu();
void mostrar_submenu();

// --- Rutinas de Servicio de Interrupción (ISR) ---
/**
 * @brief Rutina de Servicio de Interrupción (ISR) GPIO para el encoder óptico.
 *
 * Esta función es llamada cuando se detecta un flanco de bajada en el pin de datos
 * del encoder óptico. Incrementa el contador `pulsos_encoder`.
 * @param gpio El pin GPIO que activó la interrupción.
 * @param events El tipo de evento que activó la interrupción.
 */
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == OPT_ENCODER_DT && (events & GPIO_IRQ_EDGE_FALL)) {
        pulsos_encoder++;
    }
}

// --- Funciones de Inicialización ---
/**
 * @brief Inicializa todos los pines GPIO necesarios.
 *
 * Configura los pines para el HX711, habilitación del motor, encoder óptico (con interrupción),
 * y encoder rotatorio. Establece las direcciones de entrada/salida y las resistencias pull-up
 * donde sea necesario.
 */
void init_gpio() {
    gpio_init(HX711_DT);
    gpio_set_dir(HX711_DT, GPIO_IN);
    gpio_init(HX711_SCK);
    gpio_set_dir(HX711_SCK, GPIO_OUT);

    gpio_init(MOTOR_EN);
    gpio_set_dir(MOTOR_EN, GPIO_OUT);

    // Configura la interrupción para el encoder óptico
    gpio_set_irq_enabled_with_callback(OPT_ENCODER_DT, GPIO_IRQ_EDGE_FALL, true, gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);

    gpio_init(ROT_CLK);
    gpio_set_dir(ROT_CLK, GPIO_IN);
    gpio_init(ROT_DT);
    gpio_set_dir(ROT_DT, GPIO_IN);
    gpio_init(ROT_SW);
    gpio_set_dir(ROT_SW, GPIO_IN);
    gpio_pull_up(ROT_SW); // Habilita pull-up para el interruptor del encoder rotatorio
}

/**
 * @brief Configura el PWM para el servomotor.
 *
 * Configura el pin PWM del servo especificado para una operación PWM de 50 Hz.
 * El valor de 'wrap' se calcula para lograr 50 Hz con un divisor de reloj de 64.
 */
void setup_servo() {
    gpio_set_function(SERVO_PWM, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(SERVO_PWM);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 64.0f);     // Divisor de reloj para PWM
    pwm_config_set_wrap(&config, 39062);        // Valor de 'wrap' para 50 Hz (125MHz / 64 / 50Hz = 39062.5)
    pwm_init(slice_num, &config, true);         // Inicializa y habilita el PWM
}

/**
 * @brief Establece el ángulo del servomotor.
 *
 * Convierte el ángulo deseado (0-180 grados) en un ancho de pulso PWM correspondiente
 * y establece la posición del servo.
 * @param angle El ángulo deseado en grados (0-180).
 */
void set_servo_angle(int angle) {
    // Ancho de pulso para 0 grados (mínimo) y 180 grados (máximo)
    // Correspondiendo a un ancho de pulso de 1ms y 2ms a 50Hz (39062.5 cuentas totales)
    // 1ms = 39062.5 / 20 = 1953 cuentas
    // 2ms = 39062.5 / 10 = 3906 cuentas
    uint16_t pulse = 1953 + ((angle * (3906 - 1953)) / 180);
    pwm_set_gpio_level(SERVO_PWM, pulse);
}

/**
 * @brief Hace que el servomotor oscile entre dos ángulos.
 *
 * El servo se mueve de `min_angle` a `max_angle` y luego regresa, con una pausa
 * entre cada cambio de ángulo incremental. Esto simula la distribución del hilo.
 * @param min_angle El ángulo mínimo para la oscilación.
 * @param max_angle El ángulo máximo para la oscilación.
 * @param pause_ms La pausa en milisegundos entre cada paso de ángulo.
 */
void mover_servo_oscilando(int min_angle, int max_angle, int pause_ms) {
    for (int angle = min_angle; angle <= max_angle; angle++) {
        set_servo_angle(angle);
        sleep_ms(pause_ms);
    }
    for (int angle = max_angle; angle >= min_angle; angle--) {
        set_servo_angle(angle);
        sleep_ms(pause_ms);
    }
}

// --- Funciones de Lectura de Sensores ---
/**
 * @brief Lee la fuerza simulada de la celda de carga (HX711).
 *
 * Actualmente, esta función es una simulación y siempre devuelve 0.
 * En una implementación real, leería datos del sensor HX711.
 * @return Un valor de fuerza simulado (0 en esta versión).
 */
int leer_fuerza() {
    // Simulación: Aún no conectado al HX711.
    // En un escenario real, esto implicaría leer datos del HX711.
    return 0;
}

// --- Funciones de Bobinado (Hilo) ---
/**
 * @brief Realiza el bobinado automático de hilo.
 *
 * El motor funciona continuamente y el servo oscila para distribuir el hilo.
 * El bobinado puede detenerse presionando el interruptor del encoder rotatorio o
 * si se detecta una tensión excesiva (simulado con `leer_fuerza() > 3000`).
 * Muestra los metros actuales bobinados en el OLED.
 */
void enrollar_auto() {
    pulsos_encoder = 0; // Reinicia el contador del encoder

    ssd1306_clear();
    ssd1306_draw_string(0, 0, "Auto Enrollando...");
    ssd1306_draw_string(0, 10, "Presiona SW para");
    ssd1306_draw_string(0, 20, "detener.");
    ssd1306_show();

    gpio_put(MOTOR_EN, 1); // Activa el motor

    int ultimo_pulso_mostrado = 0; // Rastrea el último conteo de pulsos mostrado

    while (1) {
        int pulsos_actuales = pulsos_encoder;

        // Actualiza la pantalla cada 100 pulsos nuevos
        if (pulsos_actuales - ultimo_pulso_mostrado >= 100) {
            ultimo_pulso_mostrado = pulsos_actuales;

            float metros_actuales = (float)pulsos_actuales / PULSOS_POR_METRO;
            ssd1306_clear();
            char msg[32];
            sprintf(msg, "Metros: %.2f", metros_actuales);
            ssd1306_draw_string(0, 0, "Enrollando (Auto)...");
            ssd1306_draw_string(0, 10, msg);
            ssd1306_draw_string(0, 20, "Presiona SW");
            ssd1306_show();
        }

        mover_servo_oscilando(50, 130, 15); // Oscila el servo para la distribución del hilo

        if (leer_fuerza() > 3000) { // Verifica si hay tensión excesiva (simulada)
            gpio_put(MOTOR_EN, 0); // Detiene el motor
            ssd1306_clear();
            ssd1306_draw_string(0, 0, "TENSION EXCESIVA!");
            ssd1306_draw_string(0, 10, "Motor detenido.");
            ssd1306_show();
            return; // Sale del bobinado
        }

        if (!gpio_get(ROT_SW)) { // Verifica si se presiona el interruptor del encoder rotatorio
            sleep_ms(200); // Debounce
            gpio_put(MOTOR_EN, 0); // Detiene el motor
            ssd1306_clear();
            ssd1306_draw_string(0, 0, "Enrollado detenido.");
            float metros_finales = (float)pulsos_encoder / PULSOS_POR_METRO;
            char final_msg[32];
            sprintf(final_msg, "Total: %.2f m", metros_finales);
            ssd1306_draw_string(0, 10, final_msg);
            ssd1306_show();
            sleep_ms(2000);
            return; // Sale del bobinado
        }
    }
}

/**
 * @brief Permite al usuario seleccionar una longitud deseada en metros usando el encoder rotatorio.
 *
 * Muestra los metros seleccionados en el OLED y espera a que se presione el interruptor del
 * encoder rotatorio para confirmar la selección.
 * @return La longitud seleccionada en metros.
 */
int seleccionar_metros() {
    int metros = 1; // Valor inicial en metros
    int last_clk = gpio_get(ROT_CLK); // Estado inicial del pin CLK

    while (1) {
        int clk = gpio_get(ROT_CLK);
        int dt = gpio_get(ROT_DT);
        if (clk != last_clk) { // Encoder rotatorio girado
            if (dt != clk) { // Rotación en sentido horario
                metros++;
            } else { // Rotación en sentido antihorario
                metros--;
            }

            if (metros < 1) metros = 1; // Asegura valor mínimo
            if (metros > 999) metros = 999; // Asegura valor máximo

            // Muestra los metros en el OLED
            ssd1306_clear();
            char buffer[32];
            sprintf(buffer, "Metros: %d", metros);
            ssd1306_draw_string(0, 0, "HILO MANUAL");
            ssd1306_draw_string(0, 10, buffer);
            ssd1306_draw_string(0, 20, "Presiona SW");
            ssd1306_show();

            last_clk = clk;
            sleep_ms(100); // Debounce para el encoder rotatorio
        }

        if (!gpio_get(ROT_SW)) { // Interruptor del encoder rotatorio presionado
            sleep_ms(200); // Debounce
            return metros; // Devuelve los metros seleccionados
        }

        sleep_ms(20);
    }
}

/**
 * @brief Bobina hilo hasta un número específico de metros.
 *
 * El motor funciona hasta que `pulsos_encoder` alcanza el `pulsos_deseados` objetivo.
 * El servo oscila para la distribución del hilo. El bobinado se detiene si se detecta
 * una tensión excesiva. Muestra el progreso del bobinado en el OLED.
 * @param metros_deseados La longitud objetivo de hilo a bobinar en metros.
 */
void enrollar_hasta(int metros_deseados) {
    int pulsos_deseados = metros_deseados * PULSOS_POR_METRO;
    pulsos_encoder = 0; // Reinicia el contador global del encoder

    int ultimo_pulso_mostrado = 0;

    ssd1306_clear();
    ssd1306_draw_string(0, 0, "Enrollando...");
    ssd1306_show();

    gpio_put(MOTOR_EN, 1); // Activa el motor

    while (pulsos_encoder < pulsos_deseados) {
        // Actualiza la pantalla solo si hay un cambio significativo (cada 100 pulsos)
        if (pulsos_encoder - ultimo_pulso_mostrado >= 100) {
            ultimo_pulso_mostrado = pulsos_encoder;

            float metros_actuales = (float)pulsos_encoder / PULSOS_POR_METRO;
            char msg[32];
            sprintf(msg, "Metros: %.2f", metros_actuales);

            ssd1306_clear();
            ssd1306_draw_string(0, 0, "Enrollando...");
            ssd1306_draw_string(0, 10, msg);
            ssd1306_show();
        }

        mover_servo_oscilando(50, 130, 15); // Oscila el servo

        if (leer_fuerza() > 3000) { // Verifica si hay tensión excesiva (simulada)
            gpio_put(MOTOR_EN, 0); // Detiene el motor
            ssd1306_clear();
            ssd1306_draw_string(0, 0, "TENSION EXCESIVA!");
            ssd1306_draw_string(0, 10, "Motor detenido.");
            ssd1306_show();
            return; // Sale del bobinado
        }
    }

    gpio_put(MOTOR_EN, 0); // Detiene el motor cuando se alcanza el objetivo
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "Enrollado completo!");
    float metros_finales = (float)pulsos_encoder / PULSOS_POR_METRO;
    char final_msg[32];
    sprintf(final_msg, "Total: %.2f m", metros_finales);
    ssd1306_draw_string(0, 10, final_msg);
    ssd1306_show();
    sleep_ms(2000);
}

// --- Funciones de Bobinado (Cobre) ---
/**
 * @brief Calcula el número aproximado de vueltas requeridas para una inductancia dada.
 *
 * Este cálculo se basa en una fórmula simplificada para la inductancia de un solenoide:
 * $L = (\mu_0 * N^2 * A) / h$, donde:
 * - $L$ es la inductancia en Henrios.
 * - $\mu_0$ es la permeabilidad del espacio libre ($4\pi \times 10^{-7} H/m$).
 * - $N$ es el número de vueltas.
 * - $A$ es el área de la sección transversal de la bobina ($ \pi r^2 $).
 * - $h$ es la longitud de la bobina (asumida como la altura del tambor).
 *
 * Reorganizando para N: $N = \sqrt((L * h) / (\mu_0 * A))$
 *
 * @param milihenrios La inductancia deseada en milihenrios.
 * @return El número calculado de vueltas (redondeado al entero más cercano).
 */
int calcular_vueltas_para_mH(int milihenrios) {
    const float mu_0 = 4 * 3.1416e-7; // Permeabilidad del espacio libre
    const float radio = 0.014;       // Radio de la bobina (asumiendo que 1.4cm es el diámetro del tambor,
                                     // entonces el radio sería 0.7cm = 0.007m.
                                     // El valor 0.014 se mantiene como en el código original,
                                     // asumiendo que representa un 'radio' diferente para el cálculo de la bobina de cobre.)
    const float altura = 0.028;      // Altura de la bobina (asumiendo 2.8cm = 0.028m para el cálculo)
    float A = 3.1416 * radio * radio; // Área de la sección transversal de la bobina

    float L = (float)milihenrios / 1000.0; // Convierte milihenrios a Henrios

    float N = sqrt((L * altura) / (mu_0 * A)); // Calcula el número de vueltas

    return (int)(N + 0.5); // Redondea al entero más cercano
}

/**
 * @brief Permite al usuario seleccionar una inductancia deseada en milihenrios usando el encoder rotatorio.
 *
 * Muestra los mH seleccionados en el OLED y espera a que se presione el interruptor del
 * encoder rotatorio para confirmar la selección.
 * @return La inductancia seleccionada en milihenrios.
 */
int seleccionar_mHenrios() {
    int mH = 100; // Valor inicial en milihenrios
    int last_clk = gpio_get(ROT_CLK);

    while (1) {
        int clk = gpio_get(ROT_CLK);
        int dt = gpio_get(ROT_DT);
        if (clk != last_clk) { // Encoder rotatorio girado
            if (dt != clk) { // Rotación en sentido horario
                mH += 10;
            } else { // Rotación en sentido antihorario
                mH -= 10;
            }

            if (mH < 10) mH = 10;     // mH mínimo
            if (mH > 2000) mH = 2000; // mH máximo

            // Muestra los mH en el OLED
            ssd1306_clear();
            char buffer[32];
            sprintf(buffer, "Valor: %d mH", mH);
            ssd1306_draw_string(0, 0, "COBRE MANUAL");
            ssd1306_draw_string(0, 10, buffer);
            ssd1306_draw_string(0, 20, "Presiona SW");
            ssd1306_show();

            last_clk = clk;
            sleep_ms(100); // Debounce para el encoder rotatorio
        }

        if (!gpio_get(ROT_SW)) { // Interruptor del encoder rotatorio presionado
            sleep_ms(200); // Debounce
            return mH; // Devuelve los milihenrios seleccionados
        }

        sleep_ms(20);
    }
}

/**
 * @brief Realiza el bobinado manual de hilo de cobre para lograr una inductancia objetivo.
 *
 * Calcula las vueltas requeridas basándose en los `milihenrios` de entrada. El motor funciona
 * hasta que `pulsos_encoder` (representando vueltas) alcanza las `vueltas_objetivo`.
 * El servo oscila para la distribución del hilo. El bobinado puede detenerse presionando
 * el interruptor del encoder rotatorio o si se detecta tensión excesiva.
 * Muestra el progreso del bobinado (vueltas) en el OLED.
 * @param milihenrios La inductancia objetivo en milihenrios.
 */
void enrollar_cobre_manual(int milihenrios) {
    int vueltas_objetivo = calcular_vueltas_para_mH(milihenrios);
    pulsos_encoder = 0; // Reinicia el contador global del encoder

    int ultima_muestra = 0; // Rastrea el último conteo de pulsos mostrado

    ssd1306_clear();
    ssd1306_draw_string(0, 0, "Cobre Manual...");
    char msg[32];
    sprintf(msg, "%d mH -> %d vueltas", milihenrios, vueltas_objetivo);
    ssd1306_draw_string(0, 10, msg);
    ssd1306_draw_string(0, 20, "Presiona SW para parar");
    ssd1306_show();

    gpio_put(MOTOR_EN, 1); // Activa el motor

    while (pulsos_encoder < vueltas_objetivo) {
        // Actualiza la pantalla cada 20 pulsos (vueltas)
        if (pulsos_encoder - ultima_muestra >= 20) {
            ultima_muestra = pulsos_encoder;

            ssd1306_clear();
            char vueltas_msg[32];
            sprintf(vueltas_msg, "Vueltas: %d", pulsos_encoder);
            ssd1306_draw_string(0, 0, "Cobre Manual...");
            ssd1306_draw_string(0, 10, vueltas_msg);
            ssd1306_draw_string(0, 20, "Presiona SW");
            ssd1306_show();
        }

        mover_servo_oscilando(50, 130, 15); // Oscila el servo para la distribución del hilo

        if (leer_fuerza() > 3000) { // Verifica si hay tensión excesiva (simulada)
            gpio_put(MOTOR_EN, 0); // Detiene el motor
            ssd1306_clear();
            ssd1306_draw_string(0, 0, "TENSION EXCESIVA!");
            ssd1306_draw_string(0, 10, "Motor detenido.");
            ssd1306_show();
            return; // Sale del bobinado
        }

        if (!gpio_get(ROT_SW)) { // Verifica si se presiona el interruptor del encoder rotatorio
            sleep_ms(200); // Debounce
            gpio_put(MOTOR_EN, 0); // Detiene el motor
            ssd1306_clear();
            ssd1306_draw_string(0, 0, "Enrollado detenido.");
            char final_msg[32];
            sprintf(final_msg, "Vueltas: %d", pulsos_encoder);
            ssd1306_draw_string(0, 10, final_msg);
            ssd1306_show();
            sleep_ms(2000);
            return; // Sale del bobinado
        }
    }

    gpio_put(MOTOR_EN, 0); // Detiene el motor cuando se alcanza el objetivo
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "Bobina completada!");
    char final_msg[32];
    sprintf(final_msg, "%d mH", milihenrios);
    ssd1306_draw_string(0, 10, final_msg);
    ssd1306_show();
    sleep_ms(2000);
}

/**
 * @brief Realiza el bobinado automático de hilo de cobre para una inductancia objetivo de 1 Henrio.
 *
 * Calcula las vueltas requeridas para 1 Henrio (1000 mH). El motor funciona
 * hasta que `pulsos_encoder` (representando vueltas) alcanza las `vueltas_objetivo`.
 * El servo oscila para la distribución del hilo. El bobinado puede detenerse presionando
 * el interruptor del encoder rotatorio o si se detecta tensión excesiva.
 * Muestra el progreso del bobinado (vueltas) en el OLED.
 */
void enrollar_cobre_auto() {
    int vueltas_objetivo = calcular_vueltas_para_mH(1000); // Objetivo 1 Henrio (1000 mH)
    pulsos_encoder = 0; // Reinicia el contador global del encoder

    int ultima_muestra = 0;

    ssd1306_clear();
    ssd1306_draw_string(0, 0, "Cobre Auto (1H)...");
    ssd1306_draw_string(0, 10, "Vueltas: ");
    char vueltas_msg[16];
    sprintf(vueltas_msg, "%d", vueltas_objetivo);
    ssd1306_draw_string(60, 10, vueltas_msg);
    ssd1306_show();

    gpio_put(MOTOR_EN, 1); // Activa el motor

    while (pulsos_encoder < vueltas_objetivo) {
        // Actualiza la pantalla cada 20 pulsos (vueltas)
        if (pulsos_encoder - ultima_muestra >= 20) {
            ultima_muestra = pulsos_encoder;

            ssd1306_clear();
            char msg[32];
            sprintf(msg, "Vueltas: %d", pulsos_encoder);
            ssd1306_draw_string(0, 0, "Cobre Auto (1H)...");
            ssd1306_draw_string(0, 10, msg);
            ssd1306_draw_string(0, 20, "Presiona SW para parar");
            ssd1306_show();
        }

        mover_servo_oscilando(50, 130, 15); // Oscila el servo

        if (leer_fuerza() > 3000) { // Verifica si hay tensión excesiva (simulada)
            gpio_put(MOTOR_EN, 0); // Detiene el motor
            ssd1306_clear();
            ssd1306_draw_string(0, 0, "TENSION EXCESIVA!");
            ssd1306_draw_string(0, 10, "Motor detenido.");
            ssd1306_show();
            return; // Sale del bobinado
        }

        if (!gpio_get(ROT_SW)) { // Verifica si se presiona el interruptor del encoder rotatorio
            sleep_ms(200); // Debounce
            gpio_put(MOTOR_EN, 0); // Detiene el motor
            ssd1306_clear();
            ssd1306_draw_string(0, 0, "Enrollado detenido.");
            char final_msg[32];
            sprintf(final_msg, "Vueltas: %d", pulsos_encoder);
            ssd1306_draw_string(0, 10, final_msg);
            ssd1306_show();
            sleep_ms(2000);
            return; // Sale del bobinado
        }
    }

    gpio_put(MOTOR_EN, 0); // Detiene el motor cuando se alcanza el objetivo
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "Bobina completa!");
    ssd1306_draw_string(0, 10, "Vueltas: 1 Henrio");
    ssd1306_show();
    sleep_ms(2000);
}


// --- Funciones de Visualización de Menú ---
/**
 * @brief Muestra el menú principal en el OLED.
 *
 * Resalta la opción actualmente seleccionada (`menu_state`).
 * Las opciones son "Hilo" y "Cobre".
 */
void mostrar_menu() {
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "Menu:");
    ssd1306_draw_string(0, 10, menu_state == 0 ? "> Hilo" : "  Hilo");
    ssd1306_draw_string(0, 20, menu_state == 1 ? "> Cobre" : "  Cobre");
    ssd1306_show();
}

/**
 * @brief Muestra el submenú basándose en el `menu_state` actual.
 *
 * Resalta la opción actualmente seleccionada (`sub_state`).
 * Las opciones son "Manual", "Auto" y "Volver".
 */
void mostrar_submenu() {
    ssd1306_clear();
    ssd1306_draw_string(0, 0, menu_state == 0 ? "Hilo:" : "Cobre:"); // El título cambia según la selección del menú principal
    ssd1306_draw_string(0, 10, sub_state == 0 ? "> Manual" : "  Manual");
    ssd1306_draw_string(0, 20, sub_state == 1 ? "> Auto" : "  Auto");
    ssd1306_draw_string(0, 30, sub_state == 2 ? "> Volver" : "  Volver");
    ssd1306_show();
}

// --- Programa Principal ---
/**
 * @brief Punto de entrada principal del programa de la máquina bobinadora de hilo.
 *
 * Inicializa los periféricos, establece el bucle principal para la navegación del menú
 * y llama a las funciones de bobinado apropiadas basándose en las selecciones del usuario.
 */
int main() {
    stdio_init_all();      // Inicializa stdio (para depuración vía UART)
    init_gpio();           // Inicializa todos los pines GPIO
    ssd1306_init(I2C_PORT, OLED_SDA, OLED_SCL); // Inicializa la pantalla OLED
    setup_servo();         // Inicializa el PWM del servo

    while (true) {
        // Navegación del Menú Principal
        mostrar_menu();
        while (1) {
            if (!gpio_get(ROT_SW)) { // Interruptor del encoder rotatorio presionado para seleccionar
                sleep_ms(200); // Debounce
                break; // Sale del bucle de selección del menú principal
            }

            // El DT del encoder rotatorio cambia de estado al girar (simulado como lectura directa para simplificar)
            // En un encoder rotatorio real, ROT_CLK y ROT_DT se leen juntos para determinar la dirección.
            // Esta implementación asume que ROT_DT en bajo indica una selección "siguiente".
            if (!gpio_get(ROT_DT)) { // Simula rotación para la navegación del menú
                menu_state = (menu_state + 1) % 2; // Cicla entre 0 y 1 (Hilo, Cobre)
                mostrar_menu();
                sleep_ms(300); // Retraso para que el usuario vea el cambio y evite el ciclaje rápido
            }
        }

        // Navegación del Submenú (Manual, Auto, Volver)
        sub_state = 0; // Reinicia el estado del submenú al entrar
        mostrar_submenu();
        while (1) {
            if (!gpio_get(ROT_SW)) { // Interruptor del encoder rotatorio presionado para seleccionar
                sleep_ms(200); // Debounce

                if (sub_state == 2) {
                    // "Volver" seleccionado, regresa al menú principal
                    break;
                } else if (menu_state == 0 && sub_state == 0) {
                    // HILO -> MANUAL seleccionado
                    int metros = seleccionar_metros();
                    enrollar_hasta(metros);
                    break; // Sale del submenú después de la tarea
                } else if (menu_state == 0 && sub_state == 1) {
                    // HILO -> AUTO seleccionado
                    enrollar_auto();
                    break; // Sale del submenú después de la tarea
                } else if (menu_state == 1 && sub_state == 0) {
                    // COBRE -> MANUAL seleccionado
                    int mHenrios = seleccionar_mHenrios();
                    enrollar_cobre_manual(mHenrios);
                    break; // Sale del submenú después de la tarea
                } else if (menu_state == 1 && sub_state == 1) {
                    // COBRE -> AUTO seleccionado
                    enrollar_cobre_auto();
                    break; // Sale del submenú después de la tarea
                }
            }

            // El DT del encoder rotatorio cambia de estado al girar
            if (!gpio_get(ROT_DT)) { // Simula rotación para la navegación del submenú
                sub_state = (sub_state + 1) % 3; // Cicla entre 0, 1, 2 (Manual, Auto, Volver)
                mostrar_submenu();
                sleep_ms(300); // Retraso
            }
        }

        // Cuando una tarea finaliza o se selecciona "Volver", el bucle se reinicia para mostrar el menú principal.
    }

    return 0; // Teóricamente nunca debería alcanzarse en un bucle infinito
}