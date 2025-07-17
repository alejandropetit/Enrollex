/**
 * @file ssd1306.h
 * @brief Definiciones de la interfaz para el controlador de pantalla OLED SSD1306.
 *
 * Este archivo de encabezado define las constantes y las prototipos de funciones
 * para el manejo de pantallas OLED basadas en el chip SSD1306 a través de I2C.
 * Proporciona una API para la inicialización, limpieza, actualización y dibujo
 * de píxeles, caracteres y cadenas de texto en la pantalla.
 */

#ifndef SSD1306_H // Guarda de inclusión para evitar definiciones múltiples
#define SSD1306_H

#include "hardware/i2c.h" // Incluye la cabecera para tipos y funciones de I2C del SDK de Pico
#include <stdint.h>       // Incluye definiciones de tipos enteros de ancho fijo (ej. uint8_t)
#include <stdbool.h>      // Incluye la definición del tipo booleano (bool)

// --- Definiciones de Constantes ---
/** @defgroup Constants Constantes SSD1306
 * @{
 */
#define SSD1306_I2C_ADDR 0x3C ///< Dirección I2C predeterminada del dispositivo SSD1306.
#define SSD1306_WIDTH 128     ///< Ancho de la pantalla SSD1306 en píxeles.
#define SSD1306_HEIGHT 64     ///< Alto de la pantalla SSD1306 en píxeles.
/** @} */ // fin de Constantes

// --- Prototipos de Funciones Públicas ---
/** @defgroup PublicFunctions Funciones Públicas SSD1306
 * @{
 */

/**
 * @brief Inicializa la pantalla OLED SSD1306.
 *
 * Configura la interfaz I2C y envía la secuencia de comandos de inicialización
 * al controlador SSD1306. Esto debe llamarse antes de cualquier otra operación
 * de dibujo en la pantalla.
 *
 * @param i2c Puntero a la instancia I2C a utilizar (ej. `i2c0`, `i2c1`).
 * @param sda El número de pin GPIO para la línea de datos I2C (SDA).
 * @param scl El número de pin GPIO para la línea de reloj I2C (SCL).
 */
void ssd1306_init(i2c_inst_t *i2c, uint8_t sda, uint8_t scl);

/**
 * @brief Limpia el búfer de la pantalla.
 *
 * Establece todos los píxeles en el búfer de memoria a 'apagado' (negro).
 * La pantalla física no se actualizará hasta que se llame a `ssd1306_show()`.
 */
void ssd1306_clear(void);

/**
 * @brief Muestra el contenido del búfer de la pantalla en la pantalla OLED.
 *
 * Envía los datos del búfer de píxeles a la pantalla SSD1306 a través de I2C,
 * haciendo que los cambios dibujados previamente sean visibles.
 */
void ssd1306_show(void);

/**
 * @brief Dibuja o borra un píxel individual en el búfer de la pantalla.
 *
 * Este cambio solo es visible en la pantalla física después de llamar a `ssd1306_show()`.
 *
 * @param x La coordenada X (columna) del píxel. Debe estar entre 0 y `SSD1306_WIDTH - 1`.
 * @param y La coordenada Y (fila) del píxel. Debe estar entre 0 y `SSD1306_HEIGHT - 1`.
 * @param color `true` para encender el píxel (blanco), `false` para apagarlo (negro).
 */
void ssd1306_draw_pixel(uint8_t x, uint8_t y, bool color);

/**
 * @brief Dibuja un solo carácter en el búfer de la pantalla.
 *
 * Utiliza una fuente de 5x7 píxeles definida externamente (`font5x7.h`) para renderizar el carácter.
 * El carácter se dibujará en las coordenadas (x, y) especificadas.
 *
 * @param x La coordenada X de la esquina superior izquierda del carácter.
 * @param y La coordenada Y de la esquina superior izquierda del carácter.
 * @param c El carácter ASCII a dibujar.
 */
void ssd1306_draw_char(uint8_t x, uint8_t y, char c);

/**
 * @brief Dibuja una cadena de texto en el búfer de la pantalla.
 *
 * Dibuja cada carácter de la cadena secuencialmente, avanzando 6 píxeles en X
 * por cada carácter (5 píxeles de ancho del carácter + 1 píxel de espacio).
 *
 * @param x La coordenada X de la esquina superior izquierda del primer carácter de la cadena.
 * @param y La coordenada Y de la esquina superior izquierda de la cadena.
 * @param str Puntero a la cadena de caracteres terminada en nulo a dibujar.
 */
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str);

/** @} */ // fin de PublicFunctions

#endif // SSD1306_H