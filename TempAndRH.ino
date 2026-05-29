#define F_CPU 16000000UL  // ความถี่นาฬิกาของ MPU

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#define DHT_DDR DDRC
#define DHT_PORT PORTC
#define DHT_PIN PINC
#define DHT_INPUTPIN 0

#define TIMEOUT 200

#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1

int8_t dht_getdata(uint8_t *temperature, uint8_t *humidity);
void UART_Init(unsigned int ubrr);
void UART_Transmit(char data);
void UART_SendString(char *str);

int main(void)
{
    uint8_t Temperature, Humidity;
    int8_t Return_Code;
    char buffer[50];

    UART_Init(MYUBRR);  // เริ่มต้นการสื่อสาร UART

    while(1)
    {
        Return_Code = dht_getdata(&Temperature, &Humidity);

        if(Return_Code == 0)
        {
            // อ่านข้อมูลสำเร็จ
            sprintf(buffer, "Temperature: %d°C, Humidity: %d%%\r\n", Temperature, Humidity);
            UART_SendString(buffer);
        }
        else if(Return_Code == -1)
        {
            // ข้อผิดพลาด Timeout
            UART_SendString("Error: Timeout while reading from DHT11\r\n");
        }
        else if(Return_Code == -2)
        {
            // ข้อผิดพลาด Checksum
            UART_SendString("Error: Checksum mismatch\r\n");
        }

        _delay_ms(1000);  // รออย่างน้อย 1 วินาทีก่อนอ่านค่าครั้งถัดไป
    }
}

int8_t dht_getdata(uint8_t *temperature, uint8_t *humidity)
{
    uint8_t blocks[5];     // สำหรับข้อมูล 40 บิต (5 ไบต์)
    uint8_t i, j = 0;
    uint16_t loopCnt;
    uint8_t checksum;

    // เริ่มต้นค่าในอาร์เรย์ blocks ให้เป็นศูนย์
    for(i = 0; i < 5; i++) blocks[i] = 0;

    // ส่งสัญญาณเริ่มต้นไปยัง DHT11
    DHT_DDR |= (1 << DHT_INPUTPIN);    // ตั้งค่า DHT_PIN เป็น Output
    DHT_PORT &= ~(1 << DHT_INPUTPIN);  // ส่งสัญญาณ LOW เพื่อเริ่มต้น
    _delay_ms(18);                     // รออย่างน้อย 18ms
    DHT_PORT |= (1 << DHT_INPUTPIN);   // ส่งสัญญาณ HIGH
    _delay_us(20);                     // รอ 20us
    DHT_DDR &= ~(1 << DHT_INPUTPIN);   // สลับ DHT_PIN เป็น Input
    DHT_PORT &= ~(1 << DHT_INPUTPIN);  // ปิดใช้งาน Pull-up Resistor ภายใน

    // รอการตอบสนองจาก DHT11 (LOW ประมาณ 80us)
    loopCnt = TIMEOUT;
    while(DHT_PIN & (1 << DHT_INPUTPIN))
    {
        _delay_us(1);
        if (--loopCnt == 0) return -1;  // ข้อผิดพลาด Timeout
    }

    // รอการตอบสนองจาก DHT11 (HIGH ประมาณ 80us)
    loopCnt = TIMEOUT;
    while(!(DHT_PIN & (1 << DHT_INPUTPIN)))
    {
        _delay_us(1);
        if (--loopCnt == 0) return -1;  // ข้อผิดพลาด Timeout
    }

    // รอให้ DHT11 ดึงสายลง LOW อีกครั้ง
    loopCnt = TIMEOUT;
    while(DHT_PIN & (1 << DHT_INPUTPIN))
    {
        _delay_us(1);
        if (--loopCnt == 0) return -1;  // ข้อผิดพลาด Timeout
    }

    // อ่านข้อมูล 40 บิต (5 ไบต์)
    for (j = 0; j < 5; j++)
    {
        for (i = 0; i < 8; i++)
        {
            // รอให้สายข้อมูลเป็น HIGH
            loopCnt = TIMEOUT;
            while(!(DHT_PIN & (1 << DHT_INPUTPIN)))
            {
                _delay_us(1);
                if (--loopCnt == 0) return -1;  // ข้อผิดพลาด Timeout
            }

            // วัดระยะเวลาที่สัญญาณเป็น HIGH
            _delay_us(30);  // รอ 30us
            if (DHT_PIN & (1 << DHT_INPUTPIN))
            {
                blocks[j] |= (1 << (7 - i));  // ถ้ายังเป็น HIGH ถือว่าเป็น '1'

                // รอให้สายข้อมูลเป็น LOW
                loopCnt = TIMEOUT;
                while(DHT_PIN & (1 << DHT_INPUTPIN))
                {
                    _delay_us(1);
                    if (--loopCnt == 0) return -1;  // ข้อผิดพลาด Timeout
                }
            }
            else
            {
                // ถ้าเป็น LOW หลังจาก 30us ถือว่าเป็น '0'
                blocks[j] &= ~(1 << (7 - i));
            }
        }
    }

    // ตรวจสอบ Checksum
    checksum = blocks[0] + blocks[1] + blocks[2] + blocks[3];
    if (checksum != blocks[4])
    {
        return -2;  // ข้อผิดพลาด Checksum
    }

    // แปลงข้อมูลเป็นค่าอุณหภูมิและความชื้น
    *humidity = blocks[0];
    *temperature = blocks[2];

    return 0;  // อ่านข้อมูลสำเร็จ
}

// ฟังก์ชันสำหรับการเริ่มต้น UART
void UART_Init(unsigned int ubrr)
{
    // ตั้งค่า Baud rate
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;

    // เปิดใช้งานตัวส่งและตัวรับ
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);

    // ตั้งค่า Frame Format: 8 data bits, 1 stop bit
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}

// ฟังก์ชันสำหรับส่งข้อมูล 1 ไบต์ผ่าน UART
void UART_Transmit(char data)
{
    // รอจนกว่า Buffer พร้อมสำหรับส่งข้อมูล
    while (!(UCSR0A & (1<<UDRE0)));

    // ใส่ข้อมูลลงใน Buffer เพื่อส่ง
    UDR0 = data;
}

// ฟังก์ชันสำหรับส่งสตริงผ่าน UART
void UART_SendString(char *str)
{
    while(*str)
    {
        UART_Transmit(*str++);
    }
}
