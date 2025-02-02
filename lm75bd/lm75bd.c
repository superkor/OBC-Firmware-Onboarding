#include "lm75bd.h"
#include "i2c_io.h"
#include "errors.h"
#include "logging.h"
#include "console.h"

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* LM75BD Registers (p.8) */
#define LM75BD_REG_CONF 0x01U  /* Configuration Register (R/W) */

error_code_t lm75bdInit(lm75bd_config_t *config) {
  error_code_t errCode;

  if (config == NULL) return ERR_CODE_INVALID_ARG;

  RETURN_IF_ERROR_CODE(writeConfigLM75BD(config->devAddr, config->osFaultQueueSize, config->osPolarity,
                                         config->osOperationMode, config->devOperationMode));

  // Assume that the overtemperature and hysteresis thresholds are already set
  // Hysteresis: 75 degrees Celsius
  // Overtemperature: 80 degrees Celsius

  return ERR_CODE_SUCCESS;
}

error_code_t readTempLM75BD(uint8_t devAddr, float *temp) {
  /* Implement this driver function */
  if (temp == NULL){
    return ERR_CODE_INVALID_ARG; //pointer should not be null
  }

  uint8_t tempPointer = 0x0; //temp pointer reg is set to 0 to specify temperature reading to the device

  error_code_t errCode = 0;

  RETURN_IF_ERROR_CODE(i2cSendTo(devAddr, &tempPointer, sizeof(tempPointer)));// write pointer byte 0x0 to the device address for temperature reading

  uint8_t buff[2] = {0, 0}; //0 is MSB, 1 is LSB

  RETURN_IF_ERROR_CODE(i2cReceiveFrom(devAddr, buff, sizeof(buff))); //get actual temperature reading

  int16_t val = ((int16_t)(buff[0] << 8 | buff[1]) >> 5);

  *temp = val * 0.125; //store result

  return ERR_CODE_SUCCESS;
}

#define CONF_WRITE_BUFF_SIZE 2U
error_code_t writeConfigLM75BD(uint8_t devAddr, uint8_t osFaultQueueSize, uint8_t osPolarity,
                                   uint8_t osOperationMode, uint8_t devOperationMode) {
  error_code_t errCode;

  // Stores the register address and data to be written
  // 0: Register address
  // 1: Data
  uint8_t buff[CONF_WRITE_BUFF_SIZE] = {0};

  buff[0] = LM75BD_REG_CONF;

  uint8_t osFaltQueueRegData = 0;
  switch (osFaultQueueSize) {
    case 1:
      osFaltQueueRegData = 0;
      break;
    case 2:
      osFaltQueueRegData = 1;
      break;
    case 4:
      osFaltQueueRegData = 2;
      break;
    case 6:
      osFaltQueueRegData = 3;
      break;
    default:
      return ERR_CODE_INVALID_ARG;
  }

  buff[1] |= (osFaltQueueRegData << 3);
  buff[1] |= (osPolarity << 2);
  buff[1] |= (osOperationMode << 1);
  buff[1] |= devOperationMode;

  errCode = i2cSendTo(LM75BD_OBC_I2C_ADDR, buff, CONF_WRITE_BUFF_SIZE);
  if (errCode != ERR_CODE_SUCCESS) return errCode;

  return ERR_CODE_SUCCESS;
}
