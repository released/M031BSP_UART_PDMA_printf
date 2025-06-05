# M031BSP_UART_PDMA_printf
M031BSP_UART_PDMA_printf

update @ 2025/06/04

1. use M032SE EVB , to test UART printf , by PDMA CH4 

	- enable define
	
```c
#define USE_FIFO_CTRL
#define USE_NON_BLOCKING
```

2. below is log message , when use pdma_printf

![image](https://github.com/released/M031BSP_UART_PDMA_printf/blob/main/log.jpg)

