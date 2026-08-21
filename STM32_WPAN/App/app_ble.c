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

#include "btstack_interface.h"
#include "btstack_hal.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Maximum size of data buffer (Rx or Tx) */
#define HCI_DATA_MAX_SIZE         315

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
static BleStack_init_t pInitParams;
/* USER CODE BEGIN PV */
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

/* Internal wrapper structure combining packet with list node */
typedef struct
{
    tListNode node;
    HCI_Packet_t packet;
} HCI_PacketWrapper_t;

/* Packet memory pool */
static HCI_PacketWrapper_t HCI_PacketPoolMem[HCI_PACKET_NUM];

/* Free packet pool (linked list) */
static tListNode HCI_FreePacketPool;

/* Received event queue (linked list) */
static tListNode HCI_EventQueue;

/**
 * @brief Initialize HCI packet pool and event queue
 */
void HCI_PoolInit(void)
{
    uint32_t i;

    /* Initialize list heads */
    LST_init_head(&HCI_FreePacketPool);
    LST_init_head(&HCI_EventQueue);

    /* Add all packets to free pool */
    for (i = 0; i < HCI_PACKET_NUM; i++)
    {
        HCI_PacketPoolMem[i].packet.length = 0;
        memset(HCI_PacketPoolMem[i].packet.buf, 0, HCI_DATA_MAX_SIZE);

        LST_insert_tail(
            &HCI_FreePacketPool,
            (tListNode *)&HCI_PacketPoolMem[i]);
    }
}

/**
 * @brief Allocate a packet from the free pool
 */
HCI_Packet_t *HCI_AllocPacket(void)
{
    HCI_PacketWrapper_t *wrapper = NULL;

    os_disable_isr();

    if (LST_get_size(&HCI_FreePacketPool) != 0)
    {
        LST_remove_head(
            &HCI_FreePacketPool,
            (tListNode **)&wrapper);
    }

    os_enable_isr();

    if (wrapper != NULL)
    {
        wrapper->packet.length = 0;
        memset(wrapper->packet.buf, 0, HCI_DATA_MAX_SIZE);
        return &wrapper->packet;
    }

    return NULL;
}

/**
 * @brief Return a packet to the free pool
 */
void HCI_FreePacket(HCI_Packet_t *packet)
{
    HCI_PacketWrapper_t *wrapper;

    if (packet == NULL)
    {
        return;
    }

    /* Get wrapper from packet address */
    wrapper = (HCI_PacketWrapper_t *)((uint8_t *)packet - offsetof(HCI_PacketWrapper_t, packet));

    packet->length = 0;
    memset(packet->buf, 0, HCI_DATA_MAX_SIZE);

    os_disable_isr();

    LST_insert_tail(
        &HCI_FreePacketPool,
        (tListNode *)wrapper);

    os_enable_isr();
    // RF_BTSTACK_RUN_LOOP_EMBEDDED_ISR_POLL();
}

/**
 * @brief Put a received event into the event queue
 */
void HCI_QueueEvent(HCI_Packet_t *packet)
{
    HCI_PacketWrapper_t *wrapper;

    if (packet == NULL)
    {
        return;
    }

    /* Get wrapper from packet address */
    wrapper = (HCI_PacketWrapper_t *)((uint8_t *)packet - offsetof(HCI_PacketWrapper_t, packet));

    os_disable_isr();

    LST_insert_tail(
        &HCI_EventQueue,
        (tListNode *)wrapper);

    os_enable_isr();
}

/**
 * @brief Get the oldest event from the event queue
 */
HCI_Packet_t *HCI_GetEvent(void)
{
    HCI_PacketWrapper_t *wrapper = NULL;

    os_disable_isr();

    if (LST_get_size(&HCI_EventQueue) != 0)
    {
        LST_remove_head(
            &HCI_EventQueue,
            (tListNode **)&wrapper);
    }

    os_enable_isr();

    if (wrapper != NULL)
    {
        return &wrapper->packet;
    }

    return NULL;
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
    count = LST_get_size(&HCI_FreePacketPool);
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

int HCI_SendPacket(HCI_Packet_t *packet)
{
    uint16_t response_length;

    if (packet == NULL)
    {
        return -1;
    }

    response_length = BleStack_Request(packet->buf);

    if (response_length == 0)
    {
        HCI_FreePacket(packet);
        return -1;
    }

    packet->length = response_length;

    HCI_QueueEvent(packet);

    return 0;
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
