# Enrollex

## Descripción

Enrollex es una máquina embobinadora inteligente y programable diseñada para facilitar la creación de bobinas de forma automatizada y precisa. Basada en una Raspberry Pi Pico, esta herramienta combina control digital, sensores y componentes mecánicos para trabajar con una amplia variedad de materiales, como hilo textil y alambre de cobre. Su diseño permite embobinar de forma uniforme o generar bobinas con una inductancia específica, expresada en Henrios, ideal para aplicaciones electrónicas.

La interfaz de usuario es intuitiva, permitiendo seleccionar el tipo de material, los parámetros de bobinado, el valor deseado de inductancia, y el patrón de distribución. Según el material elegido, Enrollex ajusta automáticamente su comportamiento, modificando la velocidad, tensión y patrón del embobinado. Además, incorpora un sistema de detección de errores, como rotura del hilo, tensión anormal o desplazamiento incorrecto, deteniendo el proceso o notificando al usuario para evitar desperdicios y asegurar la calidad de la bobina final.

Enrollex también cuenta con un servo motorizado encargado de distribuir el hilo o alambre de forma precisa sobre el carrete, adaptando el tipo de movimiento según el material y la geometría deseada. Su versatilidad lo convierte en una herramienta ideal tanto para pequeños talleres como para usos didácticos y prototipado en ingeniería.

## Motivación

El proyecto tiene como objetivo diseñar y desarrollar una maquina automatizada capaz de embobinar hilo o alambre de manera eficiente y precisa, minimizando la intervención humana. Para ello, se empleara un sistema basado en raspberry pi pico que controlara el proceso de embobinado, asegurando una distribución uniforme del material sobre el carrete. La motivación para proponer este proyecto surge de la necesidad de optimizar el proceso de embobinado de hilos y alambres en diversas aplicaciones industriales y artesanales. Actualmente, muchas soluciones manuales requieren tiempo y esfuerzo,  mientras que las máquinas comerciales pueden ser costosas y poco accesibles. Por ello, buscamos desarrollar un sistema automatizado, eficiente y de bajo costo que facilite esta tarea mediante el uso de tecnologías como Raspberry pi pico u otros MCU's, sensores de medición y piezas impresas en 3D. Además, este proyecto nos permite aplicar conocimientos de electrónica, programación y mecánica en un sistema real con aplicaciones prácticas.

## Requisitos Funcionales

### Procesamiento de datos:

El sistema deberá calcular el número de vueltas necesarias según los henrios ingresados por el usuario.

Deberá interpretar la selección del material y ajustar los parámetros de funcionamiento.

### Comunicación:

Deberá comunicarse con sensores (encoder, fotointerruptor, sensor de tensión) y actuadores (motor, servo) a través de GPIO y protocolos básicos.

### Control:

Deberá controlar la velocidad y giro del motor de bobinado dependiendo del tipo de material.

Controlar el servo para distribuir el material de forma precisa.

Detener el sistema en caso de atasco o ruptura.

### Interfaz de usuario:

Mostrar en pantalla OLED/LCD el tipo de material seleccionado, henrios deseados, vueltas actuales, modo de operación, y errores.

Permitir navegación mediante un potenciómetro rotativo con pulsador.

### Modos de operación:

Modo automático: el sistema hace todo según los valores ingresados.

Modo manual: el usuario puede girar la bobina hacia adelante/atrás desde la interfaz.

### Seguridad:

Deberá detener el proceso si se detecta una tensión fuera de rango o un atasco/ruptura del material.

### Tiempos de respuesta:

La respuesta a eventos críticos como ruptura debe ser inferior a 500 ms.

### Cumplimiento de estándares:

Uso de componentes seguros y operación dentro de los límites especificados por los fabricantes.

## Requisitos no Funcionales

### Usabilidad:

La interfaz debe ser fácil de comprender y utilizar sin necesidad de capacitación técnica.

### Fiabilidad:

El sistema no debe fallar durante ciclos de trabajo de hasta 3 horas continuas.

### Rendimiento:

Deberá controlar motores y sensores en tiempo real sin retardos apreciables.

### Mantenibilidad:

El código deberá estar modularizado y comentado para permitir futuras modificaciones.

### Consumo de energía:

El sistema deberá funcionar con una fuente de 12V/5A, sin sobrepasar los 50W.

### Interoperabilidad:

Será compatible con módulos y sensores estándar disponibles en el mercado (I2C, PWM, ADC).


## Estructura y componentes

Se utilizará una estructura física donde se pueda montar la bobina, el motor de embobinado, el servomotor y los sensores. Esta estructura puede estar fabricada con perfiles de aluminio o una carcasa impresa en 3D, simulando el entorno operativo real del dispositivo.

todo esto estará integrado en un mismo dispositivo y se presentará al profesor.

El dinero esperado para este proyecto se asimilará por parte de los tres estudiantes, hay que tener en cuenta que no todos los componentes serán comprados, si no reutilizados de proyectos anteriores.

Raspberry Pi Pico

Motor DC para bobinado

Servomotor

Encoder rotativo

Sensor de ruptura/atasco (fotointerruptor)

Sensor de tensión (potenciómetro angular o strain gauge)

Pantalla OLED o LCD

Potenciómetro rotativo con pulsador

Fuente de alimentación 12V/5A

PCB o protoboard, cables, resistencias, etc.
