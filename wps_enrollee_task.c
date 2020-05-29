/*******************************************************************************
* File Name: wps_enrollee_task.c
*
* Description: This file contains the task definition for handling the WPS
* transaction and connecting to the AP by using the credentials obtained during
* WPS.
*
*
* Related Document: See README.md
*
********************************************************************************
* Copyright (2020), Cypress Semiconductor Corporation.
********************************************************************************
* This software, including source code, documentation and related materials
* (“Software”), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries (“Cypress”) and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software (“EULA”).
*
* If no EULA applies, Cypress hereby grants you a personal, nonexclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress’s integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death (“High Risk Product”). By
* including Cypress’s product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*****************************************​**************************************/

/*******************************************************************************
 * Header file includes
 ******************************************************************************/
#include "cyhal.h"
#include "cybsp.h"
#include "cy_lwip.h"

/* Wi-Fi Connection Manager includes */
#include "cy_wcm.h"

/* Task header files */
#include "wps_enrollee_task.h"


/*******************************************************************************
 * Global Variables
 ******************************************************************************/
TaskHandle_t wps_enrollee_task_handle;
bool is_network_connected = false;
bool is_retarget_io_initialized = false;
bool is_led_initialized = false;

/* Device's enrollee details. The details of WPS mode, WPS authentication, and
 * encryption methods supported are provided in this structure.
 */
const cy_wcm_wps_device_detail_t enrollee_details =
{
    .device_name               = "PSoC 6",
    .manufacturer              = "Cypress",
    .model_name                = "PSoC 6",
    .model_number              = "1.0",
    .serial_number             = "1234567",
    .device_category           = CY_WCM_WPS_DEVICE_COMPUTER,
    .sub_category              = 7,
    .config_methods            = CY_WCM_WPS_CONFIG_LABEL | CY_WCM_WPS_CONFIG_VIRTUAL_PUSH_BUTTON | CY_WCM_WPS_CONFIG_VIRTUAL_DISPLAY_PIN,
    .authentication_type_flags = CY_WCM_WPS_OPEN_AUTHENTICATION | CY_WCM_WPS_WPA_PSK_AUTHENTICATION | CY_WCM_WPS_WPA2_PSK_AUTHENTICATION | CY_WCM_WPS_WPA2_WPA_PSK_MIXED_AUTHENTICATION,
    .encryption_type_flags     = CY_WCM_WPS_NO_ENCRYPTION | CY_WCM_WPS_AES_ENCRYPTION | CY_WCM_WPS_TKIP_ENCRYPTION
};


/*******************************************************************************
 * Function Definitions
 ******************************************************************************/

static void network_event_callback(cy_wcm_event_t event, cy_wcm_event_data_t *event_data);
static cy_rslt_t wifi_connect(cy_wcm_connect_params_t *connect_param, cy_wcm_ip_address_t *ip_addr);
static void gpio_interrupt_handler(void *handler_arg, cyhal_gpio_event_t event);
static void print_wps_ap_credential(cy_wcm_wps_credential_t *result);


/*******************************************************************************
 * Function Definitions
 ******************************************************************************/

/*******************************************************************************
 * Function Name: wps_enrollee_task
 *******************************************************************************
 * Summary: Task waits for notification from the ISR associated with the user
 * button. After receiving notification, it starts to scan for APs. If the
 * AP is WPS enabled then a WPS transaction occurs between them on exchange of
 * PIN or on button press on the AP. Afterwards, it connects to the AP with the
 * credentials obtained. If the user presses the button again after connecting
 * to the AP, the device disconnects before starting the enrollee operation.
 *
 * Parameters:
 *  void* arg: Task parameter defined during task creation (unused).
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void wps_enrollee_task(void *arg)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_wcm_config_t wcm_config = { .interface = CY_WCM_INTERFACE_TYPE_STA };
    cy_wcm_wps_config_t wps_config = { .mode = WPS_MODE_CONFIG };
    cy_wcm_wps_credential_t credentials[MAX_WIFI_CREDENTIALS_COUNT];
    uint16_t credential_count = MAX_WIFI_CREDENTIALS_COUNT;
    cy_wcm_connect_params_t connect_param;
    cy_wcm_ip_address_t ip_addr;

    result = cy_wcm_init(&wcm_config);
    error_handler(result, "Failed to initialize Wi-Fi Connection Manager.\n");

    /* Register event callbacks for changes in Wi-Fi link status. These events
     * could be related to IP address changes, connection, and disconnection
     * events.
     */
    cy_wcm_register_event_callback(network_event_callback);

    /* Initialize the user button after the tasks are created to prevent sending
     * task notification to wps_enrollee_task before its creation.
     */
    result = cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT,
                             CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);
    error_handler(result, "Failed to initialize GPIO button.\n");

    /* Configure GPIO interrupt */
    cyhal_gpio_register_callback(CYBSP_USER_BTN, gpio_interrupt_handler, NULL);
    cyhal_gpio_enable_event(CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL, GPIO_INTERRUPT_PRIORITY, true);

    memset(credentials, 0, sizeof(credentials));
    memset(&connect_param, 0, sizeof(cy_wcm_connect_params_t));
    memset(&ip_addr, 0, sizeof(cy_wcm_ip_address_t));

    while(true)
    {
        /* The task waits until it receives task notification from the user
         * button ISR. If this value is not pdPASS, then it was something other
         * than a button press (such as a timeout) that caused the notification.
         */
        if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY))
        {
            credential_count = MAX_WIFI_CREDENTIALS_COUNT;

            if(is_network_connected)
            {
                APP_INFO(("Already connected to Wi-Fi. Disconnecting before starting WPS.\n"));
                if(CY_RSLT_SUCCESS == cy_wcm_disconnect_ap())
                {
                    APP_INFO(("Disconnected from Wi-Fi.\n"));
                    is_network_connected = false;
                }
            }

            /* Check for the WPS mode.*/
            if (CY_WCM_WPS_PIN_MODE == WPS_MODE_CONFIG)
            {
                APP_INFO(("Starting Enrollee in PIN mode.\n"));
                char pin_string[CY_WCM_WPS_PIN_LENGTH];

                /* Here, the WPS PIN is generated by the device. the user has to
                 * enter the pin in the AP to join the network through WPS.
                 */
                cy_wcm_wps_generate_pin(pin_string);
                wps_config.password = pin_string;
                APP_INFO(("Enter this PIN: \'%s\' in your AP.\n", pin_string));
            }
            else
            {
                APP_INFO(("Press the push button on your WPS AP.\n"));
            }

            result = cy_wcm_wps_enrollee(&wps_config, &enrollee_details, credentials, &credential_count);

            if (CY_RSLT_SUCCESS == result)
            {
                APP_INFO(("WPS Success.\n"));

                /* Print the WPS credentials obtained through WPS.*/
                for (uint32_t loop = 0; loop < credential_count; loop++)
                {
                    print_wps_ap_credential(&credentials[loop]);
                }

                /* Copy credentials of first AP credential obtained through WPS and
                 * connect to AP.
                 */
                memcpy(connect_param.ap_credentials.SSID, credentials->ssid, sizeof(credentials->ssid));
                memcpy(connect_param.ap_credentials.password, credentials->passphrase, sizeof(credentials->passphrase));
                connect_param.ap_credentials.security = credentials->security;
                result = wifi_connect(&connect_param, &ip_addr);

                if(CY_RSLT_SUCCESS != result)
                {
                    /* Failed after maximum retry attempts. */
                    ERR_INFO(("Exceeded maximum Wi-Fi connection attempts. Failed to connect to Wi-Fi\n"));
                }
                else
                {
                    is_network_connected = true;
                }
            }
            else
            {
                ERR_INFO(("WPS Enrollee failed.\n"));
            }
        }
    }
}


/*******************************************************************************
 * Function Name: network_event_callback
 *******************************************************************************
 * Summary: This callback function is called when there is a change in the link
 * status. Registering this callback function will enable the notifications from
 * WCM in the events of disconnection, reconnection, and change in IP address.
 *
 * Parameters:
 * cy_wcm_event_t event: Network event as listed in the enumeration 
 *                       cy_wcm_event_t.
 * cy_wcm_event_data_t *event_data: Contains data related to the network event.
 *
 * Return:
 * void
 *
 ******************************************************************************/
static void network_event_callback(cy_wcm_event_t event, cy_wcm_event_data_t *event_data)
{
    if (CY_WCM_EVENT_DISCONNECTED == event)
    {
        APP_INFO(("Disconnected from Wi-Fi\n"));
        is_network_connected = false;
    }
    else if (CY_WCM_EVENT_RECONNECTED == event)
    {
        APP_INFO(("Reconnected to Wi-Fi.\n"));
        is_network_connected = true;
    }
    /* This event corresponds to the event when the IP address of the device
     * changes.
     */
    else if (CY_WCM_EVENT_IP_CHANGED == event)
    {
        if (event_data->ip_addr.version == CY_WCM_IP_VER_V4)
        {
            APP_INFO(("Assigned IP address = %s\n", ip4addr_ntoa((const ip4_addr_t *)&event_data->ip_addr.ip.v4)));
        }
        else if(event_data->ip_addr.version == CY_WCM_IP_VER_V6)
        {
            APP_INFO(("Assigned IP address = %s\n", ip6addr_ntoa((const ip6_addr_t *)&event_data->ip_addr.ip.v6)));
        }
    }
}


/*******************************************************************************
 * Function Name: wifi_connect
 *******************************************************************************
 * Summary: This function executes a connect to the AP. The maximum number of
 * times it attempts to connect to the AP is specified by MAX_WIFI_RETRY_COUNT.
 *
 * Parameters:
 * cy_wcm_connect_params_t *connect_param: Pointer to connection parameters.
 * cy_wcm_ip_address_t *ip_address: Pointer to IP address.
 *
 * Return:
 * cy_rslt_t: The status of connecting to Wi-Fi network.
 *
 ******************************************************************************/
static cy_rslt_t wifi_connect(cy_wcm_connect_params_t *connect_param, cy_wcm_ip_address_t *ip_address )
{
    APP_INFO(("Connecting to AP \n"));
    cy_rslt_t result;

    /* Attempt to connect to WiFi until a connection is made or until
     * MAX_WIFI_RETRY_COUNT attempts have been made.
     */
    for(uint32_t conn_retries = 0; conn_retries < MAX_WIFI_RETRY_COUNT; conn_retries++ )
    {
        result = cy_wcm_connect_ap(connect_param, ip_address);

        if(result == CY_RSLT_SUCCESS)
        {
            APP_INFO(("Successfully connected to Wi-Fi network '%s'.\n", connect_param->ap_credentials.SSID));
            break;
        }

        ERR_INFO(("Connection to Wi-Fi network failed with error code %d."
               "Retrying in %d ms...\n", (int)result, WIFI_CONN_RETRY_INTERVAL_MSEC));
               
        vTaskDelay(pdMS_TO_TICKS(WIFI_CONN_RETRY_INTERVAL_MSEC));
    }

    return result;
}


/*******************************************************************************
 * Function Name: print_wps_ap_credential
 *******************************************************************************
 * Summary: This function prints the WPS AP credential. It converts the security
 * type to a corresponding string before printing.
 *
 * Parameters:
 *  cy_wcm_wps_credential_t *result: Pointer to the WPS AP credential.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void print_wps_ap_credential(cy_wcm_wps_credential_t *result)
{
    char* security_type_string;

    /* Convert the security type of the scan result to the corresponding
     * security string.
     */
    switch (result->security)
    {
    case CY_WCM_SECURITY_OPEN:
         security_type_string = SECURITY_OPEN;
        break;
    case CY_WCM_SECURITY_WEP_PSK:
         security_type_string = SECURITY_WEP_PSK;
        break;
    case CY_WCM_SECURITY_WEP_SHARED:
         security_type_string = SECURITY_WEP_SHARED;
        break;
    case CY_WCM_SECURITY_WPA_TKIP_PSK:
         security_type_string = SECURITY_WEP_TKIP_PSK;
        break;
    case CY_WCM_SECURITY_WPA_AES_PSK:
         security_type_string = SECURITY_WPA_AES_PSK;
        break;
    case CY_WCM_SECURITY_WPA_MIXED_PSK:
         security_type_string = SECURITY_WPA_MIXED_PSK;
        break;
    case CY_WCM_SECURITY_WPA2_AES_PSK:
         security_type_string = SECURITY_WPA2_AES_PSK;
        break;
    case CY_WCM_SECURITY_WPA2_TKIP_PSK:
         security_type_string = SECURITY_WPA2_TKIP_PSK;
        break;
    case CY_WCM_SECURITY_WPA2_MIXED_PSK:
         security_type_string = SECURITY_WPA2_MIXED_PSK;
        break;
    case CY_WCM_SECURITY_WPA2_FBT_PSK:
         security_type_string = SECURITY_WPA2_FBT_PSK;
        break;
    case CY_WCM_SECURITY_WPA3_SAE:
         security_type_string = SECURITY_WPA3_SAE;
        break;
    case CY_WCM_SECURITY_WPA3_WPA2_PSK:
         security_type_string = SECURITY_WPA3_WPA2_PSK;
        break;
    case CY_WCM_SECURITY_IBSS_OPEN:
         security_type_string = SECURITY_IBSS_OPEN;
        break;
    case CY_WCM_SECURITY_WPS_SECURE:
         security_type_string = SECURITY_WPS_SECURE;
        break;
    case CY_WCM_SECURITY_UNKNOWN:
         security_type_string = SECURITY_UNKNOWN;
        break;
    default:
         security_type_string = SECURITY_UNKNOWN;
        break;
    }

    APP_INFO(("SSID = %s, Password = %c%c******, Security = %s.\n", 
              result->ssid, result->passphrase[0], result->passphrase[1], security_type_string));
}


/*******************************************************************************
 * Function Name: gpio_interrupt_handler
 *******************************************************************************
 * Summary:
 *  GPIO interrupt service routine. This function detects button presses and
 *  sends task notifications to the WPS Enrollee task to scan for WPS AP.
 *
 * Parameters:
 *  void *callback_arg : pointer to variable passed to the ISR
 *  cyhal_gpio_event_t event : GPIO event type
 *
 * Return:
 *  None
 *
 ******************************************************************************/
static void gpio_interrupt_handler(void *handler_arg, cyhal_gpio_event_t event)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Notify wps_enrollee_task to start scanning for existing WPS AP to obtain
     * credentials through WPS.
     */
    xTaskNotifyFromISR(wps_enrollee_task_handle, 0x00, eSetValueWithoutOverwrite,
                       &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/*******************************************************************************
* Function Name: error_handler
********************************************************************************
*
* Summary:
* This function processes unrecoverable errors such as any component
* initialization errors etc. In case of such error, the system will halt the
* CPU.
*
* Parameters:
* cy_rslt_t result: contains the result of an operation.
* char* message: contains the error message that is printed to the serial 
* terminal.
*
* Note: If error occurs interrupts are disabled.
*
*******************************************************************************/
void error_handler(cy_rslt_t result, char* message)
{
    if(CY_RSLT_SUCCESS != result)
    {
        if(is_led_initialized)
        {
            cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);
        }

        if(is_retarget_io_initialized && (NULL != message))
        {
            ERR_INFO(("%s", message));
        }

        __disable_irq();
        CY_ASSERT(0);
    }
}


/* [] END OF FILE */
