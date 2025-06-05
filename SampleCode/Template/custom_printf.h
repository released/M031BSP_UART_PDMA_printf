#ifndef __CUSTOM_PRINTF_H__
#define __CUSTOM_PRINTF_H__

/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "NuMicro.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/


#ifndef bool
typedef unsigned char      	bool;
#endif


#ifndef bool
typedef unsigned char      	bool;
#endif


/*_____ D E F I N I T I O N S ______________________________________________*/

/*  
	template
	typedef struct _peripheral_manager_t
	{
		uint8_t u8Cmd;
		uint8_t au8Buf[33];
		uint8_t u8RecCnt;
		uint8_t bByPass;
		uint16_t* pu16Far;
	}PERIPHERAL_MANAGER_T;

	volatile PERIPHERAL_MANAGER_T g_PeripheralManager = 
	{
		.u8Cmd = 0,
		.au8Buf = {0},		//.au8Buf = {100U, 200U},
		.u8RecCnt = 0,
		.bByPass = FALSE,
		.pu16Far = NULL,	//.pu16Far = 0	
	};
	extern volatile PERIPHERAL_MANAGER_T g_PeripheralManager;
*/

#define USE_FIFO_CTRL
#define USE_NON_BLOCKING

#define UART_BUFFER_SIZE								(2048)
#define UART_TX_FIFO_SIZE 								(256)

#define UART_TX_DMA_CH                                  (4)

typedef struct 
{
    volatile uint8_t buffer[UART_TX_FIFO_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail; 
    volatile uint32_t count;
    volatile bool is_sending;
} uart_tx_fifo_t;

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/


uint32_t uart_tx_fifo_free_space(void);
uint32_t uart_tx_fifo_used_space(void);
bool uart_tx_is_busy(void);
void uart_tx_fifo_init(void);
void uart_tx_flush(void);

bool pdma_printf_nb(const char *fmt, ...);
bool custom_printf(const char *fmt, ...);
bool pdma_printf(const char * fmt, ...);

#endif //__CUSTOM_PRINTF_H__
