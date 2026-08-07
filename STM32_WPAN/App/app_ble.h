/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_ble.h
  * @author  MCD Application Team
  * @brief   Header for ble application
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef APP_BLE_H
#define APP_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "ble_types.h"
/* USER CODE BEGIN Includes */
#include "stm_list.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/

/**
  * HCI Event Packet Types
  */

typedef __PACKED_STRUCT
{
  uint8_t   evtcode;
  uint8_t   plen;
  uint8_t   payload[1];
} BleEvt_t;

typedef __PACKED_STRUCT
{
  uint8_t   type;
  BleEvt_t  evt;
} BleEvtSerial_t;

/**
  * Event type
  */

/**
  * This the payload of TL_Evt_t for a command complete event
  */
typedef __PACKED_STRUCT
{
  uint8_t   numcmd;
  uint16_t  cmdcode;
  uint8_t   payload[1];
} TL_CcEvt_t;

/**
  * LHCI Command Types
  */

typedef __PACKED_STRUCT
{
  uint16_t   cmdcode;
  uint8_t   plen;
  uint8_t   payload[255];
} BleCmd_t;

typedef __PACKED_STRUCT
{
  uint8_t   type;
  BleCmd_t  cmd;
} BleCmdSerial_t;

/* USER CODE BEGIN ET */
#define HCI_PACKET_MAX_SIZE     315
#define HCI_PACKET_NUM          16
typedef struct
{
    tListNode node;

    uint16_t  length;
    uint8_t   buf[HCI_PACKET_MAX_SIZE];
} HCI_Packet_t;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
#define TL_LOCCMD_PKT_TYPE             ( 0x20 )
#define TL_LOCRSP_PKT_TYPE             ( 0x21 )
#define TL_EVT_CS_PAYLOAD_SIZE         ( 4 )
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void APP_BLE_Init(void);
void BleStack_Process_BG(void);
/* USER CODE BEGIN EFP */
void HCI_PoolInit(void);
HCI_Packet_t *HCI_AllocPacket(void);
void HCI_FreePacket(HCI_Packet_t *packet);
void HCI_QueueEvent(HCI_Packet_t *packet);
HCI_Packet_t *HCI_GetEvent(void);
uint16_t HCI_GetEventCount(void);
uint16_t HCI_GetFreePacketCount(void);
uint8_t *HCI_GetPacketData(HCI_Packet_t *packet);
uint16_t HCI_GetPacketLength(HCI_Packet_t *packet);
tBleStatus HCI_SendPacket(HCI_Packet_t *packet);
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*APP_BLE_H */
