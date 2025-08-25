/*
 * FreeRTOS V202112.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

 /***************************************************************************************
 * Programa:            De manera periódica se genera un archivo de texto               *
 *                      con nombre aleatorio en el que se escriben números              *
 *                      decimales que siguen una distribución normal con media          *
 *                      tendiente a cero y desviación estándar unitaria.                *
 *                                                                                      *
 *                      Ese archivo se distribuye entre varias tareas cuya              *
 *                      labor es contar si los valores absolutos de los números         *
 *                      superan cierto un umbral y devolver un resultado binario        *
 *                      en base a la cantidad de valores que superan la prueba.         *
 *                                                                                      *
 *                      Posteriormente, se identifica el valor ganador de entre         *
 *                      todos los valores devueltos y se presenta un resultado          *
 *                      indicando el consenso al que se ha llegado.                     *
 *                                                                                      *
 * Autor:               Juan Misael Sánchez Pacheco                                     *
 * Fecha:               16 de mayo de 2025                                              *
 * Versión:             1.0                                                             *
 ****************************************************************************************/




/*-----------------------------------------------------------*/




/* BIBLIOTECAS */

/* Bibliotecas utilizadas */
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h> // Es necesario incluir -lm al compilar.

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Local includes. */
#include "console.h"




/*-----------------------------------------------------------*/




/* CONSTANTES. */

// Prioridad base para todas las tareas.
#define PRIORIDAD_BASE 1 

// Prioridad máxima para el planificador LLF. 
#define PRIORIDAD_CONTROLADOR   configMAX_PRIORITIES - 1 

// Periodos de las tareas (T) periódicas
#define PERIODO_T1  pdMS_TO_TICKS( 1000UL ) // 1000 ms.
#define PERIODO_T4  pdMS_TO_TICKS( 2000UL ) // 2000 ms.
#define PERIODO_LLF pdMS_TO_TICKS( 1UL )    // 1 ms.

// Plazos de ejecución para las tareas (D).
#define PLAZO_T1    pdMS_TO_TICKS( 300UL )  // 300 ms.
#define PLAZO_T2    pdMS_TO_TICKS( 1000UL ) // 1000 ms.
#define PLAZO_T3x   pdMS_TO_TICKS( 500UL )  // 500 ms.
#define PLAZO_T4    pdMS_TO_TICKS( 2000UL ) // 2000 ms.

// Tiempos de ejecución (C)
#define EJECUCION_T1    pdMS_TO_TICKS( 100UL )  // 100 ms.
#define EJECUCION_T2    pdMS_TO_TICKS( 100UL )  // 100 ms.
#define EJECUCION_T3x   pdMS_TO_TICKS( 50UL )   // 50 ms.
#define EJECUCION_T4    pdMS_TO_TICKS( 500UL  ) // 500 ms.

// Cantidad total de tareas (T1, T2, T3.x y T4).
#define TOTAL_TAREAS 12 

// Cantidad de tareas T3.x
#define TAREAS_SECUNDARIAS 9 

// Índice para recoger las tareas T3.x 
// para el planificador LLF.
#define POS_TAREAS_SECUNDARIAS 3 

// Cantidad total de caracteres para los 
// nombres de los archivos.
#define TOTAL_CARACTERES 13 

// Cantidad de caracteres para el nombre 
// de las tareas T3.x
#define CARACTERES_TAREA 5 

// Cantidad de número decimales que se 
// imprimen en el archivo.
#define NUMEROS_DECIMALES 200 

// // Media para la distribución normal.
#define MEDIA 0 

// Desviación estándar objetivo de la 
// distribución normal.
#define DESVIACION 1 

// Umbral que deben superar los valores 
// impresos en el archivo.
#define UMBRAL 2 

// Cantidad de valores mínimo que deben 
// superar el umbral en un T3.x
#define MIN_POSITIVOS 10 

// // 80% de probabilidad de enviar correctamente 
// el resultado desde T3.x
#define PROBABILIDAD_EXITO 0.8




/*-----------------------------------------------------------*/




/* VARIABLES Y DATOS. */

// Estructura para agrupar los datos de cada tarea necesarios 
// para la planificación LLF.
typedef struct {

    TaskHandle_t handle; // Handle de cada tarea.
    TickType_t instante_activacion; // Instante de activación de cada tarea.
    TickType_t plazo_ejecucion; // Plazo de ejecución de cada tarea.
    TickType_t ejecucion_restante; // Tiempo de ejecución restante por completar.
    TickType_t holgura; // Holgura actual de la tarea.
    bool activa; // Indicativo de activación de la tarea.

} DatosTarea;

// Información de las tareas.
DatosTarea datos_tareas[TOTAL_TAREAS];

// Registro de la última tarea ejecutada.
TaskHandle_t tarea_ejecutada = NULL, tarea_LLF = NULL;
TickType_t inicio_ejecucion = 0;

// Colas de comunicación entre tareas.
QueueHandle_t cola_T1_T2 = NULL, 
              cola_T2_T3x = NULL, 
              cola_T3x_T2 = NULL;

// Semáforo para el LLF.
SemaphoreHandle_t semaforo = NULL;


/*-----------------------------------------------------------*/




// PROTOTIPOS DE FUNCIONES.

 // Tareas.
static void xLLFCode( void * pvParameters );
static void xT1Code( void * pvParameters );
static void xT2Code( void * pvParameters );
static void xT3Code( void * pvParameters );
static void xT4Code( void * pvParameters );

// Funciones auxiliares.
static int calcularHolgura(DatosTarea, TickType_t);
static void recalcularPrioridades(void);
static void generarNombreAleatorio(char*);
static double generarAleatorioNormal(void);
static void imprimirResultado(int);




/*-----------------------------------------------------------*/




/*
 * Rutina principal.
 */
void main_base( void )
{
    // Semilla de aleatoriedad.
    srand(time(NULL));

    // Creación del semáforo para el LLF.
    semaforo = xSemaphoreCreateMutex();

    // Inicialización de los datos de las tareas a cero.
    for (int i = 0; i < TOTAL_TAREAS; i++)
        memset(&datos_tareas[i], 0, sizeof(DatosTarea));
    
    // Creación de las tareas principales.
    xTaskCreate( xT1Code, "T1", configMINIMAL_STACK_SIZE, &datos_tareas[0], PRIORIDAD_BASE, &datos_tareas[0].handle );
    xTaskCreate( xT2Code, "T2", configMINIMAL_STACK_SIZE, &datos_tareas[1], PRIORIDAD_BASE, &datos_tareas[1].handle );
    xTaskCreate( xT4Code, "T4", configMINIMAL_STACK_SIZE, &datos_tareas[2], PRIORIDAD_BASE, &datos_tareas[2].handle );

    // configMAX_PRIORITIES se modifica en FreeRTOSConfig.h para poder asignar una prioridad a cada tarea.
    xTaskCreate( xLLFCode, "LLF", configMINIMAL_STACK_SIZE, NULL, PRIORIDAD_CONTROLADOR, &tarea_LLF );

    // Creación de las colas de comunicación.
    cola_T1_T2 = xQueueCreate(1, sizeof(char)*TOTAL_CARACTERES);
    cola_T2_T3x = xQueueCreate(TAREAS_SECUNDARIAS, sizeof(char)*TOTAL_CARACTERES);
    cola_T3x_T2 = xQueueCreate(TAREAS_SECUNDARIAS, sizeof(bool));

    // Arranque del planificador.
    vTaskStartScheduler();

    for( ; ; ) { }
}




/*-----------------------------------------------------------*/




/*
 * Tarea:       Planificador personalizado que utiliza
 *              un sistema de planificación LLF (Least Laxity 
 *              First).

 * Autor:       Juan Misael Sánchez Pacheco
 * Fecha:       16 de mayo de 2025
 * Versión:     1.0
 */
static void xLLFCode(void * pvParameters )
{
    ( void ) pvParameters;

    // Se actualiza con el último instante de activación
    // para poder controlar su periodo.
    TickType_t ultima_activacion = xTaskGetTickCount();

    while(true)
    { 
        // Instante de tiempo actual.
        TickType_t t_actual = xTaskGetTickCount();

        // Acceso seguro a los datos de las tareas.
        if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
        {
            // Calcular holgura para cada tarea activa y actualización del tiempo de ejecución
            // restante para la última tarea que se estaba ejecutando.
            for (int i = 0; i < TOTAL_TAREAS; i++) 
            {
                if(datos_tareas[i].activa)
                {
                    // Actualización del tiempo de ejecución restante de la tarea actual.
                    if(datos_tareas[i].handle == tarea_ejecutada)
                        datos_tareas[i].ejecucion_restante--;

                    // Se calcula la holgura para las tareas activas.    
                    datos_tareas[i].holgura = calcularHolgura(datos_tareas[i], t_actual);
                }  
            }

            // Se establecen las prioridades tras actualizar cada holgura.
            recalcularPrioridades();

            // Se libera el semáforo.
            xSemaphoreGive(semaforo); 
        }

        // Periodo de activación.
        vTaskDelayUntil( &ultima_activacion, PERIODO_LLF);
    }
}

/*-----------------------------------------------------------*/

/*
 * Tarea:           Genera un archivo con números decimales
 *                  que siguen una distribución normal con
 *                  media cero y desviación estándar y le
 *                  envía el nombre del archivo a la tarea T2.
 *
 * Autor:           Juan Misael Sánchez Pacheco
 * Fecha:           16 de mayo de 2025
 * Versión:         1.0
 * Tipo de tarea:   Periódica
 */
static void xT1Code(void * pvParameters )
{
    // Se establecen los datos de la tarea T1.
    DatosTarea *datos = ( DatosTarea * ) pvParameters;

    // Importante para usarlo con vTaskDelayUntil
    TickType_t siguiente_activacion = 0; 
    
    // Actualización segura.
    if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
    {
        datos->plazo_ejecucion = PLAZO_T1;
        datos->instante_activacion = xTaskGetTickCount();
        siguiente_activacion = datos->instante_activacion; // Inicialización.
        xSemaphoreGive(semaforo); // Se libera el semáforo.
    }

    while(true)
    {
        // Se toma el semáforo.
        if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
        {
            // Se actualiza el instante de activación.
            datos->instante_activacion = siguiente_activacion;

            // Se marca como tarea activa.
            datos->activa = true;

            // Reinicio de la ejecución restante en este periodo.
            datos->ejecucion_restante = EJECUCION_T1;

            // Se libera el semáforo.
            xSemaphoreGive(semaforo); 
        }

        // Generar nombre aleatorio para el archivo.
        char nombre_archivo[TOTAL_CARACTERES] = {0};
        generarNombreAleatorio( nombre_archivo );

        // Creación/apertura del archivo en modo escritura.
        FILE *archivo = fopen( nombre_archivo, "w" );

        // Verificar si se pudo abrir correctamente
        if (archivo == NULL) 
        { 
            perror( "No se pudo abrir el archivo." ); 
            if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
            {
                datos->activa = false; // Se marca como inactiva.
                xSemaphoreGive(semaforo); // Se libera el semáforo.
            }
            vTaskDelayUntil( &siguiente_activacion, PERIODO_T1 );
            continue; // Se salta a la siguiente activación.
        }

        // Se generan y escriben los números decimales aleatorios que siguen una
        // distribución normal de media cero y desviación unitaria.
        for(int i = 0; i < NUMEROS_DECIMALES; i++)
            fprintf( archivo, "%f\n", generarAleatorioNormal() );
        
        // Cierre del fichero.
        fclose( archivo );

        // Envío del nombre del archivo a la cola para ejecutar T2.
        // Espera indefinida ya que el planificador LLF gestiona el tiempo de ejecución.
        xQueueSend( cola_T1_T2, nombre_archivo, portMAX_DELAY ); 

        // Se toma el semáforo para actualizar.
        if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
        {
            // Se marca como tarea inactiva hasta el siguiente periodo de activación.
            datos->activa = false;
            // Reinicio de su prioridad.
            vTaskPrioritySet(datos->handle, PRIORIDAD_BASE);
            // Se libera el semáforo.
            xSemaphoreGive(semaforo); 
        }

        // Periodo de activación.
        vTaskDelayUntil( &siguiente_activacion, PERIODO_T1 );
    }
}

/*-----------------------------------------------------------*/

/*
 * Tarea:           Recibe el nombre del archivo que generó
 *                  T1, crea o activa las tareas T3.x enviándoles
 *                  el nombre del archivo y espera el resultado
 *                  de estas para poder decidir el resultado que
 *                  se imprime según el valor binario más recurrente.
 *
 * Autor:           Juan Misael Sánchez Pacheco
 * Fecha:           16 de mayo de 2025
 * Versión:         1.0
 * Tipo de tarea:   Esporádica
 */
static void xT2Code(void * pvParameters )
{
    DatosTarea *datos = (DatosTarea *) pvParameters;

    if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
    {
        datos->plazo_ejecucion = PLAZO_T2;
        xSemaphoreGive(semaforo); // Se libera el semáforo.
    }

    // Almacenamiento del nombre del archivo que le envía T1
    // y que se envía a los T3.x.
    char nombre_archivo[TOTAL_CARACTERES] = {0};

    while(true)
    {
        // Se espera la recepción del nombre del archivo de forma indefinida para activarse la tarea.
        // Es el evento de activación.
        if( xQueueReceive(cola_T1_T2, nombre_archivo, portMAX_DELAY) == pdTRUE )
        {
            if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
            {
                // Instante de activación en cuanto se recibe el nombre del archivo.
                datos->instante_activacion = xTaskGetTickCount();

                // Reinicio de la ejecución restante en el periodo de activación.
                datos->ejecucion_restante = EJECUCION_T2;

                // Se marca como tarea activa.
                datos->activa = true;

                // Se libera el semáforo.
                xSemaphoreGive(semaforo); // Se libera el semáforo.
            }
            

            // Resultado temporal recibido por una tarea T3.x
            bool resultado = false;

            // Cantidad de resultados que son true o false, depende
            // del valor mayoritario.
            int recuento = 0;

            // Se crean o activan las tareas T3.x. Cada una recibe su copia del nombre del archivo.
            for(int i = POS_TAREAS_SECUNDARIAS; i < TOTAL_TAREAS; i++)
            {
                if(datos_tareas[i].handle == NULL) // Cuando no se han creado aún las tareas.
                {
                    // Se establece el nombre para la tarea T3.x
                    char nombre_tarea[CARACTERES_TAREA];
                    snprintf(nombre_tarea, CARACTERES_TAREA, "T3.%d", i - POS_TAREAS_SECUNDARIAS + 1);

                    if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
                    {
                        // Creación de la tarea T3.x
                        xTaskCreate( xT3Code, nombre_tarea, configMINIMAL_STACK_SIZE, &datos_tareas[i], PRIORIDAD_BASE, &datos_tareas[i].handle );
                        
                        // Se libera el semáforo.
                        xSemaphoreGive(semaforo); 
                    }
                }

                // Se envía una copia del nombre del archivo a la cola.
                xQueueSend( cola_T2_T3x, nombre_archivo, portMAX_DELAY );
            }

            // Se reciben los datos de cada T3.x.
            for(int i = 0; i < TAREAS_SECUNDARIAS; i++)
            {
                // Espera a recibir los valores de T3.x
                if( xQueueReceive(cola_T3x_T2, &resultado, portMAX_DELAY) == pdTRUE )
                {
                    // Se incrementa si el valor es verdadero.
                    if(resultado) recuento++;
                }
            }

            // Imprime el resultado final.
            imprimirResultado(recuento);

            // Elimina el archivo una vez procesados los datos.
            if (remove(nombre_archivo) != 0)
                perror("Error al eliminar el archivo");
            
            if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
            {
                // Se desactiva la tarea.
                datos->activa = false;
                // Reinicio de su prioridad.
                vTaskPrioritySet(datos->handle, PRIORIDAD_BASE);
                // Se libera el semáforo.
                xSemaphoreGive(semaforo); 
            }
        }
    }
}

/*-----------------------------------------------------------*/

/*
 * Tarea:           Se activa al recibir el nombre de un archivo
 *                  y lo analiza contando si hay más de un número
 *                  determinado de valores cuyos valores absolutos
 *                  superan un umbral. En caso afirmativo, el resultado
 *                  que pretende devolver es true, en caso contrario, es
 *                  false, pero existe un margen de error en el que puede
 *                  darse que devuelva el resultado contrario al real.
 *
 * Autor:           Juan Misael Sánchez Pacheco
 * Fecha:           16 de mayo de 2025
 * Versión:         1.0
 * Tipo de tarea:   Esporádica
 */
static void xT3Code(void * pvParameters )
{
    DatosTarea *datos = (DatosTarea *) pvParameters;

    if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
    {
        datos->plazo_ejecucion = PLAZO_T3x;
        xSemaphoreGive(semaforo); // Se libera el semáforo.
    }
    
    char nombre_archivo[TOTAL_CARACTERES] = {0};

    while(true)
    {
        // Es el inicio de la tarea.
        if( xQueueReceive( cola_T2_T3x, nombre_archivo, portMAX_DELAY ) == pdTRUE)
        {
            if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
            {
                // Instante de activación de la tarea.
                datos->instante_activacion = xTaskGetTickCount();

                // Reinicio de la ejecución restante.
                datos->ejecucion_restante = EJECUCION_T3x;

                // Activación de la tarea.
                datos->activa = true;

                // Se libera el semáforo.
                xSemaphoreGive(semaforo); 
            }

            // Apertura del archivo en modo lectura.
            FILE *archivo = fopen(nombre_archivo, "r");

            // Verificar si se pudo abrir correctamente
            if (archivo == NULL) 
            { 
                perror( "No se pudo abrir el archivo." ); 
                if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
                {
                    datos->activa = false; // Se marca como inactiva.
                    xSemaphoreGive(semaforo); // Se libera el semáforo.
                }
                continue; 
            }

            // Valor que se lee.
            double valor = 0.0;

            // Cantidad de valores que superan el umbral.
            int contador_positivos = 0;

            // Resultado que se devolverá.
            bool resultado = false;

            // Hasta que no haya un resultado o se termine el archivo.
            while (!resultado && fscanf(archivo, "%lf", &valor) == 1)
            {
                // Valores que superan el umbral.
                if(fabs(valor) > UMBRAL) contador_positivos++; 

                // Corte para evitar seguir leyendo si se supera o iguala la 
                // cantidad mínima de positivos necesarios.
                resultado = contador_positivos >= MIN_POSITIVOS; 
            }

            // Número aleatorio entre 0.0 y 1.0.
            double probabilidad = (double)rand() / (double)(RAND_MAX);

            // Simulación del posible error con una probabilidad de 20%.
            if(probabilidad > PROBABILIDAD_EXITO) resultado = !resultado;

            // Envío del resultado a T2.
            xQueueSend( cola_T3x_T2, &resultado, portMAX_DELAY );

            // Cierre del archivo.
            fclose(archivo);

            if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
            {
                // Se marca como inactiva.
                datos->activa = false; 
                // Reinicio de su prioridad.
                vTaskPrioritySet(datos->handle, PRIORIDAD_BASE);
                // Se libera el semáforo.
                xSemaphoreGive(semaforo);
            }
        }
    }
}

/*-----------------------------------------------------------*/

/*
 * Tarea:           Simula el consumo de CPU de forma
 *                  periódica tomando el tiempo de ejecución
 *                  previsto para el peor caso.
 *
 * Autor:           Juan Misael Sánchez Pacheco
 * Fecha:           16 de mayo de 2025
 * Versión:         1.0
 * Tipo de tarea:   Periódica
 */
static void xT4Code(void * pvParameters )
{
    DatosTarea *datos = (DatosTarea *) pvParameters;
    TickType_t siguiente_activacion = 0; 

    if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
    {
        datos->plazo_ejecucion = PLAZO_T4;
        datos->instante_activacion = xTaskGetTickCount();
        siguiente_activacion = datos->instante_activacion; // Inicialización.
        xSemaphoreGive(semaforo); // Se libera el semáforo.
    }
    
    
    while(true)
    { 
        // Se toma el semáforo.
        if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
        {
            // Se actualiza el instante de activación.
            datos->instante_activacion = siguiente_activacion;

            // Reinicio de la ejecución restante en el periodo de activación.
            datos->ejecucion_restante = EJECUCION_T4;

            // Se marca T4 como activa.   
            datos->activa = true;

            // Se libera el semáforo.
            xSemaphoreGive(semaforo); 
        }

        // Consumo de CPU según el tiempo de ejecución máximo previsto.
        while(xTaskGetTickCount() - siguiente_activacion < EJECUCION_T4);

        if( xSemaphoreTake(semaforo, portMAX_DELAY) == pdTRUE )
        {
            // Se marca como desactivada.
            datos->activa = false;
            // Reinicio de su prioridad.
            vTaskPrioritySet(datos->handle, PRIORIDAD_BASE);
            // Se libera el semáforo.
            xSemaphoreGive(semaforo); 
        }
        
        // Periodo de activación.
        vTaskDelayUntil( &siguiente_activacion, PERIODO_T4 );
    }
}




/*-----------------------------------------------------------*/




/*
 * Función:     Calcula la holgura para una tarea en un instante de tiempo.  
 * Autor:       Juan Misael Sánchez Pacheco
 * Fecha:       16 de mayo de 2025
 * Versión:     1.0
 */
static int calcularHolgura(DatosTarea tarea, TickType_t t_actual)
{
    return tarea.instante_activacion + tarea.plazo_ejecucion - t_actual - tarea.ejecucion_restante;
}

/*-----------------------------------------------------------*/

/*
 * Función:     Recalcula las prioridades de cada tarea según
 *              la holgura actual.
 *
 * Autor:       Juan Misael Sánchez Pacheco
 * Fecha:       16 de mayo de 2025
 * Versión:     1.0
 */
static void recalcularPrioridades(void)
{
    UBaseType_t prioridad = PRIORIDAD_CONTROLADOR - 1; // Prioridad máxima posible es una menor que la del LLF.
    bool asignada[TOTAL_TAREAS] = {false}; // Marca las tareas cuya prioridad ya ha sido recalculada.

    // Asignación de prioridades según la holgura. Si se incrementara el número de tareas habría
    // que buscar una mejor forma de recalcular prioridades, pero para esta práctica es suficiente
    // y es una implementación sencilla en la que se busca la tarea con menor prioridad en cada iteración
    // y se descarta la última encontrada, porque ya fue actualizada.
    for (int i = 0; i < TOTAL_TAREAS; i++) 
    {
        int indice_menor = -1; // Índice de la tarea menor.
        TickType_t holgura_menor = portMAX_DELAY; // Menor holgura encontrada.

        // Obtención de la tarea activa y cuya prioridad no se ha asignado que
        // tiene la menor holgura.
        for (int j = 0; j < TOTAL_TAREAS; j++) 
        {
            // Se omiten las tareas inactivas, ya asignadas o que ya se han completado.
            if (!datos_tareas[j].activa || asignada[j] || datos_tareas[j].ejecucion_restante <= 0) continue;

            if (datos_tareas[j].holgura < holgura_menor) 
            {
                holgura_menor = datos_tareas[j].holgura; // Actualización de menor holgura encontrada.
                indice_menor = j; // Índice de la tarea con menor holgura encontrada.
            }
        }

        // Asignación de la nueva prioridad que será 
        if (indice_menor > -1) 
        {
            // Tras establecer la nueva prioridad de decrementa la prioridad
            // para la siguiente tarea que tendrá una prioridad menor a la actual.
            vTaskPrioritySet(datos_tareas[indice_menor].handle, prioridad--);
            asignada[indice_menor] = true; // Se marca como asignada para no considerarla más.
        }
    }
}

/*-----------------------------------------------------------*/

/*
 * Función:     Genera un nombre aleatorio para un fichero
 *              con extensión .txt y contenido en un directorio /f.
 * Autor:       Juan Misael Sánchez Pacheco
 * Fecha:       16 de mayo de 2025
 * Versión:     1.0
 */
static void generarNombreAleatorio(char *nombre) 
{
    // Rango de 6 caracteres de 'a' a 'z'.
    int i = 0;

    // Directorio
    nombre[i++] = 'f';
    nombre[i++] = '/';

    // Generación de los caracteres aleatorios.
    for( ; i < TOTAL_CARACTERES - 5; i++)
        nombre[i] = (char)(rand() % 26 + 'a');

    // Extensión .txt
    nombre[i++] = '.';
    nombre[i++] = 't';
    nombre[i++] = 'x';
    nombre[i++] = 't';
    nombre[i] = '\0';
}

/*-----------------------------------------------------------*/

/*
 * Función:         Utilizando el método de transformación de Box-Muller se genera 
 *                  un valor decimal aleatorio que será miembro de una distribución
 *                  normal con media cero y desviación estándar unitaria.
 *
 * Autor:           Matt Ingenthron y Juan Misael Sánchez Pacheco.
 * Fecha:           16 de mayo de 2025
 * Versión:         1.0
 * Observaciones:   Las fuentes de las que se extrae la información para la
 *                  comprensión del problema y el algoritmo ya elaborado son
 *                  las siguientes:
 *                      - https://es.khanacademy.org/computing/computer-programming/programming-natural-simulations/programming-randomness/a/normal-distribution-of-random-numbers
 *                      - https://github.com/ingenthr/memcachetest/blob/master/boxmuller.c
 */
static double generarAleatorioNormal(void)
{				        
	double x1, x2, w, y1;
	static double y2;
	static bool usar_ultimo = false;

	if (usar_ultimo)		        
	{
		y1 = y2;
		usar_ultimo = false;
	}
	else
	{
		do 
        {
			x1 = 2.0 * ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0) - 1.0;
			x2 = 2.0 * ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0) - 1.0;
			w = x1 * x1 + x2 * x2;
		} 
        while ( w >= 1.0 );

		w = sqrt( (-2.0 * log( w ) ) / w );
		y1 = x1 * w;
		y2 = x2 * w;
		usar_ultimo = true;
	}
	return MEDIA + y1 * DESVIACION;
}

/*-----------------------------------------------------------*/

/*
 * Función:     Imprime los resultados según el recuento positivo
 *              de los resultados obtenidos por los T3.x en T2.
 *              El recuento que se hace en T2 es de los valores
 *              positivos (true).
 *
 * Autor:       Juan Misael Sánchez Pacheco
 * Fecha:       16 de mayo de 2025
 * Versión:     1.0
 */
static void imprimirResultado(int recuento)
{
    // La información para el resultado la contiene el valor de recuento.
    bool resultado = false;

    // Punto medio para la cantidad de resultados. El valor es 4 para 9 tareas.
    const int punto_medio = (int) ( TAREAS_SECUNDARIAS * 0.5f );
    
    // Mayoría es true.
    if(recuento > punto_medio) 
    {
        resultado = true;
    } 
    // Mayoría es false.
    else
    {
        recuento = TAREAS_SECUNDARIAS - recuento; // Se invierte el recuento.
        resultado = false;
    }
    
    console_print("Consenso alcanzado entre %d de %d tareas. Valor de consenso %s\n", 
        recuento, TAREAS_SECUNDARIAS, resultado ? "true" : "false");
    
}

/*-----------------------------------------------------------*/

/*
 * Función:         Obtiene la última tarea que se estaba
 *                  ejecutando.
 *
 * Autor:           Juan Misael Sánchez Pacheco
 * Fecha:           16 de mayo de 2025
 * Versión:         1.0
 * Observaciones:   Se extrae la forma de registrar la última
 *                  tarea ejecuta de la documentación de FreeRTOS:
 *                      - https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/09-RTOS-trace-feature
 */
static void actualizarTareaEjecutada(void *pxCurrentTCB)
{
    // No actualiza cuando es el propio planificador LLF
    if (pxCurrentTCB != tarea_LLF)
    {
        // Se actualiza con la tarea que se está ejecutando.
        tarea_ejecutada = (TaskHandle_t) pxCurrentTCB;
    }  
}
