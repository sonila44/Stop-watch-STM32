/* ================= REGISTERS ================= */
#define RCC_AHB1ENR      (*(volatile unsigned int *)(0x40023800 + 0x30))
#define RCC_APB1ENR      (*(volatile unsigned int *)(0x40023800 + 0x40))
#define RCC_APB2ENR      (*(volatile unsigned int *)(0x40023800 + 0x44))

#define GPIOA_MODER      (*(volatile unsigned int *)(0x40020000 + 0x00))
#define GPIOA_IDR        (*(volatile unsigned int *)(0x40020000 + 0x10))
#define GPIOA_AFRH       (*(volatile unsigned int *)(0x40020000 + 0x24))

#define USART1_SR        (*(volatile unsigned int *)(0x40011000 + 0x00))
#define USART1_DR        (*(volatile unsigned int *)(0x40011000 + 0x04))
#define USART1_BRR       (*(volatile unsigned int *)(0x40011000 + 0x08))
#define USART1_CR1       (*(volatile unsigned int *)(0x40011000 + 0x0C))

#define TIM2_CR1         (*(volatile unsigned int *)(0x40000000 + 0x00))
#define TIM2_DIER        (*(volatile unsigned int *)(0x40000000 + 0x0C))
#define TIM2_SR          (*(volatile unsigned int *)(0x40000000 + 0x10))
#define TIM2_PSC         (*(volatile unsigned int *)(0x40000000 + 0x28))
#define TIM2_ARR         (*(volatile unsigned int *)(0x40000000 + 0x2C))

#define NVIC_ISER0       (*(volatile unsigned int *)(0xE000E100))
#define NVIC_ISER1       (*(volatile unsigned int *)(0xE000E104))

/* ================= VARIABLES ================= */
typedef struct { int h, m, s, ms; } Time_t;
	Time_t now = {0, 0, 0, 0};
	Time_t laps[10];
	int lap_count = 0;
	volatile int running = 0;

volatile int f_ms = 0, f_btn = 0, f_rx = 0;
char rx_buf[20];
int rx_ptr = 0;

/* ================= UART ================= */

// Gui 1 ky tu
void uart_putc(char c) {
    while (!(USART1_SR & (1 << 7)));
    USART1_DR = c;
}

// Gui chuoi
void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

// Gui so duoi dang 2 chu so
void uart_send_2d(int num) {
    uart_putc((num / 10) + '0'); // hang chuc
    uart_putc((num % 10) + '0'); // hang don vi
}

/* ================= FORMAT ================= */
// Format hh:mm:ss
void send_time_formatted(const char *prefix, Time_t t, int lap_idx) {
    uart_puts(prefix);
		if (lap_idx >= 0) { // In them so thu tu neu co nhieu lab
        uart_putc(lap_idx + '1'); 
        uart_puts(": ");
    }
    uart_send_2d(t.h);
    uart_putc(':');
    uart_send_2d(t.m);
    uart_putc(':');
    uart_send_2d(t.s);
    uart_puts("\r\n");
}

// So sanh chuoi
int str_cmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* ================= DEBOUNCE ================= */
void Handle_Button_Debounce(void) {
    static int cnt = 0;
    static int pressed = 0;
    if (GPIOA_IDR & (1 << 0)) { // PA0 High khi bam
        if (++cnt >= 20) {
            if (!pressed) { f_btn = 1; pressed = 1; }
            cnt = 20;
        }
    } else { cnt = 0; pressed = 0; }
}

/* ================= INIT HW ================= */
void Init_Hardware(void) {
    RCC_AHB1ENR |= (1 << 0); //GPIOA
    RCC_APB2ENR |= (1 << 4); //USART1
    RCC_APB1ENR |= (1 << 0); //TIM2

    GPIOA_MODER &= ~(3 << 0); // PA0 Input
    GPIOA_MODER |= (2 << 18) | (2 << 20); // PA9, PA10 AF
    GPIOA_AFRH |= (7 << 4) | (7 << 8);    // AF7 (USART1)

    TIM2_PSC = 16 - 1;	//Prescaler 16
    TIM2_ARR = 1000 - 1; //
    TIM2_DIER |= (1 << 0); //Update interrupt cho TIM2
    TIM2_CR1 |= (1 << 0); // Cho phep chay (control register)

    USART1_BRR = 0x8B; // mantisa 8.68, fraction 10.8
    USART1_CR1 |= (1 << 13) | (1 << 3) | (1 << 2) | (1 << 5); //UE, TE, RE, RXNEIE (rx not empty thi intterupt enable)

    NVIC_ISER0 |= (1 << 28); // ngat TIM2
    NVIC_ISER1 |= (1 << (37 - 32)); // ngat USART1
}

/* ================= INTERRUPT ================= */
void TIM2_IRQHandler(void) { // moi ms kiem tra 1 lan
    TIM2_SR &= ~(1 << 0);
    Handle_Button_Debounce();
    f_ms = 1;
}

void USART1_IRQHandler(void) {
    if (USART1_SR & (1 << 5)) { //RXNE
        char c = (char)USART1_DR;
        
        // Luu buffer va day co flag de main kiem tra ngay lap tuc
        if (rx_ptr < 19) {
            rx_buf[rx_ptr++] = c;
            rx_buf[rx_ptr] = '\0'; // ket thuc de compare string
            f_rx = 1; 
        } else {
            rx_ptr = 0; // reset neu tran
        }
    }
}

/* ================= MAIN ================= */
int main(void) {
    Init_Hardware();
    uart_puts("--- STOPWATCH hh:mm:ss ---\r\n");

    while (1) {
        if (f_ms) {
            if (running) {
                now.ms++;
                if (now.ms >= 1000) {
                    now.ms = 0; now.s++;
                    if (now.s >= 60) { now.s = 0; now.m++; }
                    if (now.m >= 60) { now.m = 0; now.h++; }
                    send_time_formatted("TIME: ", now, -1); // Cap nhat /s
                }
            }
            f_ms = 0;
        }

        if (f_btn) {
            running = !running;
            uart_puts(running ? "STATUS: START\r\n" : "STATUS: STOP\r\n");
            f_btn = 0;
        }

        if (f_rx) {
            f_rx = 0; // Clear flag 

            // 1. Kiem tra LABS trc
            if (str_cmp(rx_buf, "LABS?") == 0) {
                running = 0; // Stop de in ra
                uart_puts(">> STOPPED & SHOWING LAPS\r\n");
                uart_puts("--- LAP LIST ---\r\n");
                
                if (lap_count == 0) uart_puts("(Empty)\r\n");
                for (int i = 0; i < lap_count; i++) {
                    send_time_formatted("LAP ", laps[i], i);
                }
                
                rx_ptr = 0; rx_buf[0] = '\0'; // Reset buffer
            } 
            
            // 2. Kiem tra LAP 
            else if (str_cmp(rx_buf, "LAP") == 0) {
                if (lap_count < 10) {
                    laps[lap_count++] = now;
                    uart_puts(">> LAP SAVED\r\n");
                } else {
                    uart_puts(">> ERROR: LAP FULL\r\n");
                }
                rx_ptr = 0; rx_buf[0] = '\0';
            } 
            
            // 3. Kiem tra START
            else if (str_cmp(rx_buf, "START") == 0) {
                running = 1;
                uart_puts(">> STARTED\r\n");
                rx_ptr = 0; rx_buf[0] = '\0';
            }
            
            // 4. Kiem tra STOP 
            else if (str_cmp(rx_buf, "STOP") == 0) {
                running = 0;
                uart_puts(">> STOPPED\r\n");
                rx_ptr = 0; rx_buf[0] = '\0';
            }
            
            // 5. Kiem tra RESET
            else if (str_cmp(rx_buf, "RESET") == 0) {
                running = 0; 
                now.h = now.m = now.s = now.ms = 0; 
                lap_count = 0;
                uart_puts(">> SYSTEM RESET\r\n");
                rx_ptr = 0; rx_buf[0] = '\0';
            }

            // Xoa bufer neu khong khop 6 ky tu
            if (rx_ptr >= 6) { 
                rx_ptr = 0; 
                rx_buf[0] = '\0'; 
            }
        }
    }
}