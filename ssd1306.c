/**
 * @file ssd1306.c
 * @brief Implementación de las funciones de bajo nivel para el controlador de pantalla OLED SSD1306.
 *
 * Este archivo contiene las definiciones de las funciones para inicializar y controlar
 * una pantalla OLED basada en el chip SSD1306 a través de I2C. Incluye operaciones
 * para escribir comandos, enviar datos, gestionar un búfer de pantalla,
 * dibujar píxeles, caracteres y cadenas de texto.
 *
 * @authors Gabriel Restrepo, Andrés Rodríguez, Alejandro Petit
 * @date 17 de julio de 2025
 */

#include "ssd1306.h"        // Incluye la cabecera para las definiciones específicas del SSD1306
#include "hardware/gpio.h"  // Funciones para control de GPIO de la Raspberry Pi Pico
#include "pico/stdlib.h"    // Funciones estándar de la Raspberry Pi Pico SDK
#include <string.h>         // Para funciones de manipulación de memoria como memset y memcpy
#include "font5x7.h"        // Incluye la definición de la fuente de caracteres 5x7

/// Búfer de memoria estático para almacenar el estado de los píxeles de la pantalla.
/// El tamaño es Ancho * Alto / 8 porque cada byte representa 8 píxeles verticales.
static uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
/// Puntero estático a la instancia I2C utilizada para comunicarse con el SSD1306.
static i2c_inst_t *ssd1306_i2c;

/**
 * @brief Envía un comando de control al controlador SSD1306 a través de I2C.
 *
 * Los comandos se envían con un byte de control 0x00 para indicar que el siguiente byte
 * es un comando.
 *
 * @param cmd El byte de comando a enviar.
 */
static void ssd1306_write_cmd(uint8_t cmd) {
    uint8_t buf[] = {0x00, cmd}; // El primer byte es 0x00 para comandos
    // Envía el comando al controlador SSD1306. El último argumento 'false' indica
    // que la transacción I2C debe detenerse después de esta escritura.
    i2c_write_blocking(ssd1306_i2c, SSD1306_I2C_ADDR, buf, 2, false);
}

/**
 * @brief Envía datos al controlador SSD1306 a través de I2C.
 *
 * Los datos se envían con un byte de control 0x40 para indicar que los siguientes bytes
 * son datos para la pantalla.
 *
 * @param data Puntero a la matriz de bytes de datos a enviar.
 * @param len El número de bytes de datos a enviar.
 */
static void ssd1306_write_data(uint8_t *data, size_t len) {
    uint8_t buf[len + 1]; // Crea un búfer temporal para incluir el byte de control
    buf[0] = 0x40;        // El primer byte es 0x40 para datos
    memcpy(buf + 1, data, len); // Copia los datos proporcionados después del byte de control
    // Envía los datos al controlador SSD1306. El último argumento 'false' indica
    // que la transacción I2C debe detenerse después de esta escritura.
    i2c_write_blocking(ssd1306_i2c, SSD1306_I2C_ADDR, buf, len + 1, false);
}

/**
 * @brief Inicializa la pantalla OLED SSD1306.
 *
 * Configura el periférico I2C con la velocidad de baudios especificada,
 * asigna las funciones GPIO para SDA y SCL, y envía una secuencia de comandos
 * de inicialización al controlador SSD1306 para configurar la pantalla.
 *
 * @param i2c Puntero a la instancia I2C a utilizar (ej. `i2c0` o `i2c1`).
 * @param sda El número de pin GPIO para la línea de datos I2C (SDA).
 * @param scl El número de pin GPIO para la línea de reloj I2C (SCL).
 */
void ssd1306_init(i2c_inst_t *i2c, uint8_t sda, uint8_t scl) {
    ssd1306_i2c = i2c; // Guarda la instancia I2C para uso global
    i2c_init(i2c, 400 * 1000); // Inicializa I2C a 400 kHz (modo rápido)

    // Configura los pines GPIO para I2C
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    // Habilita las resistencias pull-up internas para las líneas I2C,
    // que son requeridas por el protocolo I2C.
    gpio_pull_up(sda);
    gpio_pull_up(scl);

    sleep_ms(100); // Pequeña pausa después de la inicialización de hardware

    // Secuencia de comandos de inicialización para el SSD1306.
    // Estos comandos configuran varios parámetros de la pantalla como:
    // Display OFF, modo de direccionamiento de memoria, mapeo de segmentos,
    // multiplexado, contraste, etc.
    ssd1306_write_cmd(0xAE); // Display OFF
    ssd1306_write_cmd(0x20); // Set Memory Addressing Mode
    ssd1306_write_cmd(0x00); // 00b,Horizontal Addressing Mode; 01b,Vertical Addressing Mode;
                             // 10b,Page Addressing Mode (RESET); 11b,Invalid
    ssd1306_write_cmd(0xB0); // Set Page Start Address for Page Addressing Mode,0-7
    ssd1306_write_cmd(0xC8); // Set COM Output Scan Direction
    ssd1306_write_cmd(0x00); // ---set low column address
    ssd1306_write_cmd(0x10); // ---set high column address
    ssd1306_write_cmd(0x40); // --set start line address
    ssd1306_write_cmd(0x81); // --set contrast control register
    ssd1306_write_cmd(0xFF); // Contraste máximo
    ssd1306_write_cmd(0xA1); // --set segment re-map 0 to 127
    ssd1306_write_cmd(0xA6); // --normal / reverse
    ssd1306_write_cmd(0xA8); // --set multiplex ratio(1 to 64)
    ssd1306_write_cmd(0x3F); // Ciclo de multiplexado 64
    ssd1306_write_cmd(0xA4); // 0xa4,Output follows RAM content; 0xa5,Output ignores RAM content
    ssd1306_write_cmd(0xD3); // -set display offset
    ssd1306_write_cmd(0x00); // -no offset
    ssd1306_write_cmd(0xD5); // --set display clock divide ratio/oscillator frequency
    ssd1306_write_cmd(0xF0); // --set divide ratio
    ssd1306_write_cmd(0xD9); // --set pre-charge period
    ssd1306_write_cmd(0x22); //
    ssd1306_write_cmd(0xDA); // --set com pins hardware configuration
    ssd1306_write_cmd(0x12); //
    ssd1306_write_cmd(0xDB); // --set vcomh
    ssd1306_write_cmd(0x20); // 0x20,0.77xVcc
    ssd1306_write_cmd(0x8D); // --set DC-DC enable
    ssd1306_write_cmd(0x14); //
    ssd1306_write_cmd(0xAF); // --turn on SSD1306 panel

    ssd1306_clear(); // Limpia el búfer de la pantalla
    ssd1306_show();  // Muestra el búfer (pantalla en blanco)
}

/**
 * @brief Limpia el búfer de la pantalla local.
 *
 * Establece todos los píxeles en el búfer a 'apagado' (0), pero no actualiza la pantalla física.
 * Se debe llamar a `ssd1306_show()` para reflejar los cambios en la pantalla.
 */
void ssd1306_clear(void) {
    // Rellena todo el búfer de memoria con ceros, apagando todos los píxeles.
    memset(buffer, 0, sizeof(buffer));
}

/**
 * @brief Envía el contenido del búfer de la pantalla local a la pantalla SSD1306.
 *
 * Itera a través de cada "página" (fila de 8 píxeles de alto) de la pantalla
 * y envía los datos correspondientes desde el búfer a la pantalla a través de I2C.
 */
void ssd1306_show(void) {
    // La pantalla SSD1306 se organiza en 8 "páginas" (filas de 8 píxeles).
    for (uint8_t page = 0; page < 8; page++) {
        // Establece la dirección de la página a la que se va a escribir.
        ssd1306_write_cmd(0xB0 + page);
        // Establece la columna de inicio para la escritura (columna 0).
        ssd1306_write_cmd(0x00); // Columna baja
        ssd1306_write_cmd(0x10); // Columna alta
        // Envía los datos de toda la página (128 píxeles de ancho) desde el búfer.
        ssd1306_write_data(&buffer[SSD1306_WIDTH * page], SSD1306_WIDTH);
    }
}

/**
 * @brief Dibuja un píxel individual en el búfer de la pantalla.
 *
 * Modifica el bit correspondiente en el búfer según las coordenadas (x, y)
 * y el color deseado. La pantalla física no se actualiza hasta que se llama a `ssd1306_show()`.
 *
 * @param x La coordenada X del píxel (columna).
 * @param y La coordenada Y del píxel (fila).
 * @param color `true` para encender el píxel (blanco), `false` para apagarlo (negro).
 */
void ssd1306_draw_pixel(uint8_t x, uint8_t y, bool color) {
    // Verifica que las coordenadas estén dentro de los límites de la pantalla.
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;

    // Calcula la posición en el búfer. Cada byte del búfer representa
    // 8 píxeles verticales en una columna. `y / 8` da la página (fila de bytes),
    // `y % 8` da el bit dentro de ese byte.
    if (color)
        buffer[x + (y / 8) * SSD1306_WIDTH] |= (1 << (y % 8)); // Enciende el píxel
    else
        buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8)); // Apaga el píxel
}

/**
 * @brief Dibuja un solo carácter en el búfer de la pantalla utilizando la fuente 5x7.
 *
 * El carácter se dibuja en las coordenadas (x, y) especificadas.
 * Los datos de los píxeles se obtienen de la matriz `font5x7`.
 *
 * @param x La coordenada X de inicio para el carácter.
 * @param y La coordenada Y de inicio para el carácter.
 * @param c El carácter ASCII a dibujar.
 */
void ssd1306_draw_char(uint8_t x, uint8_t y, char c) {
    // Itera a través de las 5 columnas del carácter de la fuente 5x7.
    for (uint8_t i = 0; i < 5; i++) {
        // Obtiene la línea (columna de píxeles) para el carácter actual.
        // Se resta 32 porque la fuente comienza en el carácter ASCII 32 (espacio).
        uint8_t line = font5x7[c - 32][i];
        // Itera a través de los 8 píxeles de alto de la columna.
        for (uint8_t j = 0; j < 8; j++) {
            // Dibuja cada píxel. Se verifica el bit menos significativo de 'line'
            // para determinar si el píxel está encendido o apagado.
            ssd1306_draw_pixel(x + i, y + j, line & 0x01);
            line >>= 1; // Desplaza el bit para verificar el siguiente píxel en la columna
        }
    }
}

/**
 * @brief Dibuja una cadena de texto en el búfer de la pantalla.
 *
 * Dibuja cada carácter de la cadena en secuencia, moviendo la posición X
 * para cada carácter subsiguiente.
 *
 * @param x La coordenada X de inicio para la cadena.
 * @param y La coordenada Y de inicio para la cadena.
 * @param str Puntero a la cadena de caracteres terminada en nulo a dibujar.
 */
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str) {
    // Itera mientras no se encuentre el carácter nulo de terminación de la cadena.
    while (*str) {
        ssd1306_draw_char(x, y, *str++); // Dibuja el carácter actual y avanza al siguiente
        x += 6; // Avanza la posición X por el ancho del carácter (5 píxeles) más 1 píxel de espacio.
    }
}