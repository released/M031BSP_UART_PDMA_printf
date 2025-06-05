/*_____ I N C L U D E S ____________________________________________________*/

#include "custom_printf.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

/*_____ D E F I N I T I O N S ______________________________________________*/

volatile uint8_t pdma_uart_tx_flag = 0U;

static uart_tx_fifo_t tx_fifo = {0};
static uint32_t last_sent_len = 0;
/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/


#if defined (USE_FIFO_CTRL)

static bool fifo_is_full(uart_tx_fifo_t *fifo)
{
    return (fifo->count >= UART_TX_FIFO_SIZE);
}

static bool fifo_is_empty(uart_tx_fifo_t *fifo)
{
    return (fifo->count == 0);
}

static uint32_t fifo_put_data(uart_tx_fifo_t *fifo, uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t written = 0;
    
    uint32_t available = UART_TX_FIFO_SIZE - fifo->count;
    if (len > available) 
    {
        len = available; 
    }
    
    for (i = 0; i < len; i++) 
    {
        fifo->buffer[fifo->head] = data[i];
        fifo->head = (fifo->head + 1) % UART_TX_FIFO_SIZE;
        fifo->count++;
        written++;
    }
    
    return written;
}

static uint32_t fifo_get_continuous_data(uart_tx_fifo_t *fifo, uint8_t **data_ptr)
{
    uint32_t continuous_len = 0;

    if (fifo_is_empty(fifo)) 
    {
        return 0;
    }
    
    *data_ptr = (uint8_t *)&fifo->buffer[fifo->tail];
    
    if (fifo->head > fifo->tail) 
    {
        continuous_len = fifo->head - fifo->tail;
    } else 
    {
        continuous_len = UART_TX_FIFO_SIZE - fifo->tail;
    }
    
    return continuous_len;
}

static void fifo_remove_data(uart_tx_fifo_t *fifo, uint32_t len)
{
    if (len > fifo->count) 
    {
        len = fifo->count;
    }
    
    fifo->tail = (fifo->tail + len) % UART_TX_FIFO_SIZE;
    fifo->count -= len;
}

static void start_uart_pdma_tx(uint8_t *data, uint32_t len)
{
    pdma_uart_tx_flag = 0;
    
    PDMA_Open(PDMA, (1 << UART_TX_DMA_CH));
    PDMA_SetTransferCnt(PDMA, UART_TX_DMA_CH, PDMA_WIDTH_8, len);
    PDMA_SetTransferAddr(PDMA, UART_TX_DMA_CH, (uint32_t)data, PDMA_SAR_INC, 
                        (uint32_t)&UART0->DAT, PDMA_DAR_FIX);
    PDMA_SetTransferMode(PDMA, UART_TX_DMA_CH, PDMA_UART0_TX, FALSE, 0);
    PDMA_SetBurstType(PDMA, UART_TX_DMA_CH, PDMA_REQ_SINGLE, 0);
    PDMA_DisableInt(PDMA, UART_TX_DMA_CH, PDMA_INT_TEMPTY);
    PDMA_EnableInt(PDMA, UART_TX_DMA_CH, PDMA_INT_TRANS_DONE);
    
    NVIC_EnableIRQ(PDMA_IRQn);
    UART_ENABLE_INT(UART0, UART_INTEN_TXPDMAEN_Msk);
    
    tx_fifo.is_sending = TRUE;
    last_sent_len = len;
}

void uart_tx_process(void)
{
    if (tx_fifo.is_sending) 
    {
        if (pdma_uart_tx_flag) 
        {
            uint8_t *data_ptr;
            uint32_t sent_len = fifo_get_continuous_data(&tx_fifo, &data_ptr);
            // fifo_remove_data(&tx_fifo, sent_len);
            fifo_remove_data(&tx_fifo, last_sent_len);
            
            UART_DISABLE_INT(UART0,UART_INTEN_TXPDMAEN_Msk );
            PDMA_DisableInt(PDMA, UART_TX_DMA_CH, PDMA_INT_TRANS_DONE);
            NVIC_DisableIRQ(PDMA_IRQn);
            tx_fifo.is_sending = FALSE;
        }
    }
    
    if (!tx_fifo.is_sending && !fifo_is_empty(&tx_fifo)) 
    {
        uint8_t *data_ptr;
        uint32_t len = fifo_get_continuous_data(&tx_fifo, &data_ptr);
        if (len > 0) 
        {
            start_uart_pdma_tx(data_ptr, len);
        }
    }
}

bool uart_tx_send_nb(uint8_t *ptr, uint32_t len)
{
    uint32_t written = 0;

    __disable_irq();
    written = fifo_put_data(&tx_fifo, ptr, len);
    __enable_irq();
    
    uart_tx_process();
    
    return (written == len);
}

void uart_tx_send(uint8_t *ptr, uint32_t len)
{
    uint32_t remaining = len;
    uint8_t *current_ptr = ptr;
    uint32_t written = 0;
    
    while (remaining > 0) 
    {
        __disable_irq();
        written = fifo_put_data(&tx_fifo, current_ptr, remaining);
        __enable_irq();
        
        uart_tx_process();
        
        if (written > 0) 
        {
            current_ptr += written;
            remaining -= written;
        }
                
        if (written == 0) 
        {
            while (fifo_is_full(&tx_fifo)) 
            {
                uart_tx_process();
            }
        }
    }
}

bool pdma_printf_nb(const char *fmt, ...)   //non-blocking
{
    va_list ap;
    char buf[UART_BUFFER_SIZE] = {0};
    uint32_t size = 0;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    size = strlen(buf);

    return uart_tx_send_nb((uint8_t *)buf, size);
}

bool pdma_printf(const char *fmt, ...)  // blocking
{
    va_list ap;
    char buf[UART_BUFFER_SIZE] = {0};
    uint32_t size = 0;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    size = strlen(buf);

    uart_tx_send((uint8_t *)buf, size);

    return TRUE;
}

bool custom_printf(const char *fmt, ...)
{
    #if defined (USE_NON_BLOCKING)
    return pdma_printf_nb(fmt);
    #else
    return pdma_printf(fmt);
    #endif
}


uint32_t uart_tx_fifo_free_space(void)
{
    return (UART_TX_FIFO_SIZE - tx_fifo.count);
}

uint32_t uart_tx_fifo_used_space(void)
{
    return tx_fifo.count;
}

bool uart_tx_is_busy(void)
{
    return tx_fifo.is_sending || !fifo_is_empty(&tx_fifo);
}

void uart_tx_fifo_init(void)
{
    memset(&tx_fifo, 0, sizeof(tx_fifo));
}

void uart_tx_flush(void)
{
    while (uart_tx_is_busy()) 
    {
        uart_tx_process();
    }
}

#else

void uart_tx_send(uint8_t *ptr, uint32_t len)
{
    // uint32_t i = 0;

    #if 1   //PDMA

    pdma_uart_tx_flag = 0;

    PDMA_Open(PDMA, (1 << UART_TX_DMA_CH));

    /* UART Tx PDMA channel configuration */
    /* Set transfer width (8 bits) and transfer count */
    PDMA_SetTransferCnt(PDMA, UART_TX_DMA_CH, PDMA_WIDTH_8, len);

    /* Set source/destination address and attributes */
    PDMA_SetTransferAddr(PDMA, UART_TX_DMA_CH, (uint32_t)ptr, PDMA_SAR_INC, (uint32_t)&UART0->DAT, PDMA_DAR_FIX);

    /* Set request source; set basic mode. */
    PDMA_SetTransferMode(PDMA, UART_TX_DMA_CH, PDMA_UART0_TX, FALSE, 0);

    /* Single request type */
    PDMA_SetBurstType(PDMA, UART_TX_DMA_CH, PDMA_REQ_SINGLE, 0);

    /* Disable table interrupt */
    PDMA_DisableInt(PDMA,UART_TX_DMA_CH, PDMA_INT_TEMPTY );

    PDMA_EnableInt(PDMA, UART_TX_DMA_CH, PDMA_INT_TRANS_DONE);

    NVIC_EnableIRQ(PDMA_IRQn);

    UART_ENABLE_INT(UART0,UART_INTEN_TXPDMAEN_Msk );

    while(!pdma_uart_tx_flag);
	// UART_WAIT_TX_EMPTY(UART0);

    UART_DISABLE_INT(UART0,UART_INTEN_TXPDMAEN_Msk );
    PDMA_DisableInt(PDMA, UART_TX_DMA_CH, PDMA_INT_TRANS_DONE);
    NVIC_DisableIRQ(PDMA_IRQn);

    #else
    for (i = 0; i < len; i++)
    {
        UART_WRITE(UART0, *ptr++);
        UART_WAIT_TX_EMPTY(UART0);
    }
    #endif
}

bool pdma_printf(const char * fmt, ...)
{
    va_list ap;
    char buf[UART_BUFFER_SIZE] = {0};
    uint32_t size = 0;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    size = strlen(buf);

    uart_tx_send((uint8_t *)buf, size);
    
    return TRUE;
}

#endif
