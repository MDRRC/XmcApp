/***********************************************************************************************************************
   @file   app_cfg.h
   @brief  Application specific settings like type of controller and used TFT pins.
 **********************************************************************************************************************/

#ifndef APP_CFG_H
#define APP_CFG_H

/***********************************************************************************************************************
   I N C L U D E S
 **********************************************************************************************************************/
#include <Arduino.h>

/***********************************************************************************************************************
   C L A S S E S
 **********************************************************************************************************************/

/**
 * Definitions for the possible micro controller.
 */
#define APP_CFG_UC_ESP8266 0
#define APP_CFG_UC_STM32 1
#define APP_CFG_UC_ATMEL 2

/**
 * Definition for the micro used in this application.
 */
#define APP_CFG_UC APP_CFG_UC_STM32

/**
 * Definitions for the TFT LCD pins.
 */

#define APP_CFG_RST PA15
#define APP_CFG_DC PA8
#define APP_CFG_CS PB3
#define APP_CFG_SCL PB13
#define APP_CFG_SDA PB15

#endif
