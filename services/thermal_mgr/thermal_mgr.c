#include "thermal_mgr.h"
#include "errors.h"
#include "lm75bd.h"
#include "console.h"

#include <FreeRTOS.h>
#include <os_task.h>
#include <os_queue.h>

#include <string.h>

#define THERMAL_MGR_STACK_SIZE 256U

static TaskHandle_t thermalMgrTaskHandle;
static StaticTask_t thermalMgrTaskBuffer;
static StackType_t thermalMgrTaskStack[THERMAL_MGR_STACK_SIZE];

#define THERMAL_MGR_QUEUE_LENGTH 10U
#define THERMAL_MGR_QUEUE_ITEM_SIZE sizeof(thermal_mgr_event_t)

static QueueHandle_t thermalMgrQueueHandle;
static StaticQueue_t thermalMgrQueueBuffer;
static uint8_t thermalMgrQueueStorageArea[THERMAL_MGR_QUEUE_LENGTH * THERMAL_MGR_QUEUE_ITEM_SIZE];

static void thermalMgr(void *pvParameters);

void initThermalSystemManager(lm75bd_config_t *config) {
  memset(&thermalMgrTaskBuffer, 0, sizeof(thermalMgrTaskBuffer));
  memset(thermalMgrTaskStack, 0, sizeof(thermalMgrTaskStack));
  
  thermalMgrTaskHandle = xTaskCreateStatic(
    thermalMgr, "thermalMgr", THERMAL_MGR_STACK_SIZE,
    config, 1, thermalMgrTaskStack, &thermalMgrTaskBuffer);

  memset(&thermalMgrQueueBuffer, 0, sizeof(thermalMgrQueueBuffer));
  memset(thermalMgrQueueStorageArea, 0, sizeof(thermalMgrQueueStorageArea));

  thermalMgrQueueHandle = xQueueCreateStatic(
    THERMAL_MGR_QUEUE_LENGTH, THERMAL_MGR_QUEUE_ITEM_SIZE,
    thermalMgrQueueStorageArea, &thermalMgrQueueBuffer);

}

error_code_t thermalMgrSendEvent(thermal_mgr_event_t *event) {
  if (event == 0){
    return ERR_CODE_INVALID_ARG;
  }

  /* Send an event to the thermal manager queue */
  if (xQueueSend(thermalMgrQueueHandle, event, (TickType_t)5) != pdPASS){
    return ERR_CODE_UNKNOWN; // failed to send message
  }

  return ERR_CODE_SUCCESS;
}

void osHandlerLM75BD(void) {
  /* Implement this function */
  thermal_mgr_event_t event = {};
  event.type = THERMAL_MGR_EVENT_OS_INTERRUPT;
  thermalMgrSendEvent(&event); //OS interrupt occurred, send OS interrupt event
}

static void thermalMgr(void *pvParameters) {
  /* Implement this task */

  thermal_mgr_event_t buffer = {};

  float tempC = 0;

  while (1) {
    if (xQueueReceive(thermalMgrQueueHandle, &buffer, (TickType_t) 5) != pdPASS){ 
      continue; 
    }  
    //received next event

    if (buffer.type == THERMAL_MGR_EVENT_MEASURE_TEMP_CMD){ //temperature reading event
      //read temperature
      if (readTempLM75BD(LM75BD_OBC_I2C_ADDR, &tempC) != ERR_CODE_SUCCESS){
        continue; //some error occurred
      }
      
      addTemperatureTelemetry(tempC); //add to temperature telemetry

      if (tempC >= LM75BD_DEFAULT_OT_THRESH){
        osHandlerLM75BD(); //must check if current temperature is over T_{th}; if it is, interrupt OS for overtemperature
      }

    } else if (buffer.type == THERMAL_MGR_EVENT_OS_INTERRUPT){
      //OS interrupt occurred => T_{th} was reached; Make sure temperature is below T_{hys}!
      if (readTempLM75BD(LM75BD_OBC_I2C_ADDR, &tempC) != ERR_CODE_SUCCESS){
        continue; //some error occurred
      }

      if (tempC >= LM75BD_DEFAULT_HYST_THRESH){
        overTemperatureDetected();
      } else {
        safeOperatingConditions();
      }
    }
  }
}

void addTemperatureTelemetry(float tempC) {
  printConsole("Temperature telemetry: %f deg C\n", tempC);
}

void overTemperatureDetected(void) {
  printConsole("Over temperature detected!\n");
}

void safeOperatingConditions(void) { 
  printConsole("Returned to safe operating conditions!\n");
}
