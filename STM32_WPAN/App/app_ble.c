/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_ble.c
  * @author  MCD Application Team
  * @brief   BLE Application
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_common.h"
#include "log_module.h"
#include "ble_core.h"
#include "uuid.h"
#include "svc_ctl.h"
#include "baes.h"
#include "pka_ctrl.h"
#include "ble_timer.h"
#include "app_ble.h"
#include "host_stack_if.h"
#include "ll_sys_if.h"
#include "stm32_rtos.h"
#include "otp.h"
#include "stm32_timer.h"
#include "stm_list.h"
#include "advanced_memory_manager.h"
#include "blestack.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "os_wrapper.h"
#include "stm32_lpm.h"
#include "stm32_lpm_if.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  LOW_POWER_MODE_DISABLE,
  LOW_POWER_MODE_STOP,
  LOW_POWER_MODE_STDBY,
} LowPowerModeStatus_t;

/* USER CODE END PTD */

/* Maximum size of data buffer (Rx or Tx) */
#define HCI_DATA_MAX_SIZE         315
#define NUM_OF_TX_SYNCHRO         2
#define NUM_OF_TX_ASYNCHRO        50
#define NUM_OF_RX_BUFFER          12
#define NUM_OF_TX_BUFFER           (NUM_OF_TX_ASYNCHRO + NUM_OF_TX_SYNCHRO)
typedef struct
{
  tListNode                 node;  /* Actual node in the list */
  uint8_t buf[HCI_DATA_MAX_SIZE];  /* Memory buffer */
} UART_node;

/* Global variables structure */
typedef struct
{
  volatile uint8_t tm_tx_on;
  uint8_t  rx_state;
  uint8_t  rxReceivedState;
  UART_node buff_node[NUM_OF_RX_BUFFER+NUM_OF_TX_BUFFER];
} HciTransport_var_t;

/* Private defines -----------------------------------------------------------*/
/* GATT buffer size (in bytes)*/


#define MBLOCK_COUNT              (BLE_MBLOCKS_CALC(PREP_WRITE_LIST_SIZE, \
                                                    CFG_BLE_ATT_MTU_MAX, \
                                                    CFG_BLE_NUM_LINK) \
                                   + CFG_BLE_MBLOCK_COUNT_MARGIN)

#define BLE_DYN_ALLOC_SIZE \
        (BLE_TOTAL_BUFFER_SIZE(CFG_BLE_NUM_LINK, MBLOCK_COUNT, (CFG_BLE_EATT_BEARER_PER_LINK * CFG_BLE_NUM_LINK)))

/* 2 words are reserved for SNVMA management */

#define BLE_DEFAULT_PIN            (111111) /* Default PIN code for pairing */

/* Definitions for "tm_rx_state" */
#define HCI_RX_STATE_WAIT_TYPE    0
#define HCI_RX_STATE_WAIT_HEADER  1
#define HCI_RX_STATE_WAIT_PAYLOAD 2

/* Definition for "hci_event_type" */
#define HCI_EVENT_SYNCHRO         0
#define HCI_EVENT_ASYNCHRO        1

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
static BleStack_init_t pInitParams;
/* USER CODE BEGIN PV */
#if (CFG_LPM_LEVEL != 0)
static LowPowerModeStatus_t LowPowerModeStatus;
#endif /* (CFG_LPM_LEVEL != 0) */

/* USER CODE END PV */

/* Global variables ----------------------------------------------------------*/

/* USER CODE BEGIN GV */

/* USER CODE END GV */

/* Private function prototypes -----------------------------------------------*/
static uint8_t HOST_BLE_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* External functions prototypes ---------------------------------------------*/

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Functions Definition ------------------------------------------------------*/
void APP_BLE_Init(void)
{
  /* USER CODE BEGIN APP_BLE_Init_1 */

  /* USER CODE END APP_BLE_Init_1 */

  /* Register BLE Host tasks */
  UTIL_SEQ_RegTask(1U << CFG_TASK_BLE_HOST, UTIL_SEQ_RFU, BleStack_Process_BG);

  /* Initialise NVM RAM buffer, invalidate it's content before restoration */

  /* USER CODE BEGIN APP_BLE_Init_Buffers */

  /* USER CODE END APP_BLE_Init_Buffers */

  /* Initialize BLE related modules */
  BAES_Reset( );
  PKACTRL_Reset();
  BLE_TIMER_Init();

  /* Initialize the BLE Host */
  if (HOST_BLE_Init() == 0u)
  {

    /* Initialize Transparent Mode Application */
    HCI_PoolInit();
  }
  /* USER CODE BEGIN APP_BLE_Init_2 */

  /* USER CODE END APP_BLE_Init_2 */

  return;
}

void BleStack_Process_BG(void)
{
  if (BleStack_Process() == 0x0)
  {
    BleStackCB_Process();
  }
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
static uint8_t HOST_BLE_Init(void)
{
  tBleStatus return_status;

  pInitParams.options                 = CFG_BLE_OPTIONS;
  pInitParams.debug                   = 0U;
/* USER CODE BEGIN HOST_BLE_Init_Params */

/* USER CODE END HOST_BLE_Init_Params */
  return_status = BleStack_Init(&pInitParams);
/* USER CODE BEGIN HOST_BLE_Init */

/* USER CODE END HOST_BLE_Init */
  return ((uint8_t)return_status);
}

/* USER CODE BEGIN FD_WRAP_FUNCTIONS */

/* Packet memory */
static HCI_Packet_t HCI_PacketPoolMem[HCI_PACKET_NUM];

/* Free packet list */
static tListNode HCI_PacketPool;

/* Received event list */
static tListNode HCI_EventQueue;

/**
 * @brief Initialize HCI packet pool and event queue
 */
void HCI_PoolInit(void)
{
    LST_init_head(&HCI_PacketPool);
    LST_init_head(&HCI_EventQueue);

    for (uint32_t i = 0; i < HCI_PACKET_NUM; i++)
    {
        HCI_PacketPoolMem[i].length = 0;

        memset(HCI_PacketPoolMem[i].buf,
               0,
               HCI_DATA_MAX_SIZE);

        LST_insert_tail(
            &HCI_PacketPool,
            (tListNode *)&HCI_PacketPoolMem[i]);
    }
}

/**
 * @brief Allocate a packet from the free pool
 */
HCI_Packet_t *HCI_AllocPacket(void)
{
    HCI_Packet_t *packet = NULL;

    os_disable_isr();

    if (LST_get_size(&HCI_PacketPool) != 0)
    {
        LST_remove_head(
            &HCI_PacketPool,
            (tListNode **)&packet);
    }

    os_enable_isr();

    if (packet != NULL)
    {
        packet->length = 0;

        memset(packet->buf,
               0,
               HCI_DATA_MAX_SIZE);
    }

    return packet;
}

/**
 * @brief Return a packet to the free pool
 */
void HCI_FreePacket(HCI_Packet_t *packet)
{
    if (packet == NULL)
    {
        return;
    }

    packet->length = 0;

    memset(packet->buf,
           0,
           HCI_DATA_MAX_SIZE);

    os_disable_isr();

    LST_insert_tail(
        &HCI_PacketPool,
        (tListNode *)packet);

    os_enable_isr();
}

/**
 * @brief Put a received event into the event queue
 */
void HCI_QueueEvent(HCI_Packet_t *packet)
{
    if (packet == NULL)
    {
        return;
    }

    os_disable_isr();

    LST_insert_tail(
        &HCI_EventQueue,
        (tListNode *)packet);

    os_enable_isr();
}

/**
 * @brief Get the oldest event from the event queue
 */
HCI_Packet_t *HCI_GetEvent(void)
{
    HCI_Packet_t *packet = NULL;

    os_disable_isr();

    if (LST_get_size(&HCI_EventQueue) != 0)
    {
        LST_remove_head(
            &HCI_EventQueue,
            (tListNode **)&packet);
    }

    os_enable_isr();

    return packet;
}

/**
 * @brief Get number of pending events
 */
uint16_t HCI_GetEventCount(void)
{
    uint16_t count;

    os_disable_isr();

    count = LST_get_size(&HCI_EventQueue);

    os_enable_isr();

    return count;
}

/**
 * @brief Get number of free packets
 */
uint16_t HCI_GetFreePacketCount(void)
{
    uint16_t count;

    os_disable_isr();

    count = LST_get_size(&HCI_PacketPool);

    os_enable_isr();

    return count;
}

uint8_t *HCI_GetPacketData(HCI_Packet_t *packet)
{
    if (packet == NULL)
    {
        return NULL;
    }

    return packet->buf;
}

uint16_t HCI_GetPacketLength(HCI_Packet_t *packet)
{
    if (packet == NULL)
    {
        return 0;
    }

    return packet->length;
}

tBleStatus HCI_SendPacket(HCI_Packet_t *packet)
{
    uint16_t response_length;

    if (packet == NULL)
    {
        return BLE_STATUS_FAILED;
    }

    response_length = BleStack_Request(packet->buf);

    if (response_length == 0)
    {
        HCI_FreePacket(packet);
        return BLE_STATUS_FAILED;
    }

    packet->length = response_length;

    HCI_QueueEvent(packet);

    return BLE_STATUS_SUCCESS;
}

/**
  * @brief Callback called by the BLE stack (from BleStack_Process() context)
  * to send an indication to the application. The indication is a BLE standard
  * packet that can be either an ACI/HCI event or an ACL data.
  * @param data: pointer to the data of the packet
  * @param length: length of the data of the packet
  * @param ext_data: pointer to the extended data
  * @param ext_length: extended data length
  * @retval Status of the operation
  */

tBleStatus BLECB_Indication( const uint8_t* data,
                          uint16_t length,
                          const uint8_t* ext_data,
                          uint16_t ext_length )
{
  HCI_Packet_t *packet = HCI_AllocPacket();

  if (packet != NULL)
  {
    memcpy(packet->buf, data, length);
    packet->length = length;

    HCI_QueueEvent(packet);
  }

  return BLE_STATUS_SUCCESS;
}

/* USER CODE END FD_WRAP_FUNCTIONS */
