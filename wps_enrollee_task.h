/*******************************************************************************
* File Name: wps_enrollee_task.h
*
* Description:This file includes the macros, enumerations, and function
* prototypes used in wps_enrollee_task.c
*
*
* Related Document: See README.md
*
********************************************************************************
* Copyright 2020-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
 *  Include guard
 ******************************************************************************/
#ifndef SOURCE_WPS_ENROLLEE_TASK_H_
#define SOURCE_WPS_ENROLLEE_TASK_H_


/*******************************************************************************
 * Header file includes
 ******************************************************************************/
/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"

/* Wi-Fi Connection Manager includes */
#include "cy_wcm.h"

#include <stdio.h>

/*******************************************************************************
 * Macros
 ******************************************************************************/
/* Set WPS_MODE_CONFIG to one of the values provided by the enumeration
 * cy_wcm_wps_mode_t in cy_wcm.h. The default mode used in this example is 
 * CY_WCM_WPS_PBC_MODE (push button based WPS).
 */
#define WPS_MODE_CONFIG                     CY_WCM_WPS_PBC_MODE

/* The value of this macro specifies the maximum number of Wi-Fi networks the
 * device can join through WPS in one call to cy_wcm_wps_enrollee. This value is
 * 2 for dual band AP. Note that the device can obtain Wi-Fi credentials of only
 * one network and hence can receive a maximum of 1 for single band Wi-Fi AP or
 * 2 for dual band AP.
 */
#define MAX_WIFI_CREDENTIALS_COUNT          (2)

/* Maximum number of attempts at retrying to connect to AP. */
#define MAX_WIFI_RETRY_COUNT                (3)

/* Delay in milliseconds between successive Wi-Fi connection attempts. */
#define WIFI_CONN_RETRY_INTERVAL_MSEC       (100u)

/* The size of the cy_wcm_ip_address_t array that is passed to 
 * cy_wcm_get_ip_addr API. In the case of stand-alone AP or STA mode the size of
 * the array is 1. In concurrent AP/STA mode the size of the array is 2 where
 * the zeroth index stores the IP address of the STA and the first index
 * stores the IP address of the AP.
 */
#define SIZE_OF_IP_ARRAY_STA                (1)

#define WPS_ENROLLEE_TASK_STACK_SIZE        (4096u)
#define WPS_ENROLLEE_TASK_PRIORITY          (3u)

#define GPIO_INTERRUPT_PRIORITY             (7u)
#define MAX_SECURITY_TYPE_STRING_LENGTH     (15)

#define APP_INFO( x )                       do { printf("Info: "); printf x;} while(0);
#define ERR_INFO( x )                       do { printf("Error: "); printf x;} while(0);

#define SECURITY_OPEN                       "OPEN"
#define SECURITY_WEP_PSK                    "WEP-PSK"
#define SECURITY_WEP_SHARED                 "WEP-SHARED"
#define SECURITY_WEP_TKIP_PSK               "WEP-TKIP-PSK"
#define SECURITY_WPA_TKIP_PSK               "WPA-TKIP-PSK"
#define SECURITY_WPA_AES_PSK                "WPA-AES-PSK"
#define SECURITY_WPA_MIXED_PSK              "WPA-MIXED-PSK"
#define SECURITY_WPA2_AES_PSK               "WPA2-AES-PSK"
#define SECURITY_WPA2_TKIP_PSK              "WPA2-TKIP-PSK"
#define SECURITY_WPA2_MIXED_PSK             "WPA2-MIXED-PSK"
#define SECURITY_WPA2_FBT_PSK               "WPA2-FBT-PSK"
#define SECURITY_WPA3_SAE                   "WPA3-SAE"
#define SECURITY_WPA3_WPA2_PSK              "WPA3-WPA2-PSK"
#define SECURITY_IBSS_OPEN                  "IBSS-OPEN"
#define SECURITY_WPS_SECURE                 "WPS-SECURE"
#define SECURITY_UNKNOWN                    "UNKNOWN"


/*******************************************************************************
 * Global Variables
 ******************************************************************************/
/* WPS Enrollee task handle */
extern TaskHandle_t wps_enrollee_task_handle;
extern bool is_retarget_io_initialized;
extern bool is_led_initialized;


/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void wps_enrollee_task(void* arg);
void error_handler(cy_rslt_t result, char* message);

#endif /*SOURCE_WPS_ENROLLEE_TASK_H_*/


/* [] END OF FILE */
