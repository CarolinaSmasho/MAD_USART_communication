# UART Chat ระหว่าง 2 บอร์ด Nucleo-F767ZI

โปรแกรมสนทนาผ่าน UART ระหว่างคอมพิวเตอร์ 2 เครื่อง ใช้ STM32 HAL Library กับ Interrupt-driven receive — โค้ดชุดเดียว flash ลงทั้ง 2 บอร์ด เปลี่ยนแค่ `#define IS_UART1`

---

## 1. ภาพรวม Hardware

```
┌──────────────────────┐                              ┌──────────────────────┐
│    Board A (UART1)   │                              │    Board B (UART2)   │
│    IS_UART1 = 1      │                              │    IS_UART1 = 0      │
│                      │                              │                      │
│  ┌────────────────┐  │                              │  ┌────────────────┐  │
│  │    USART3      │  │                              │  │    USART3      │  │
│  │  PD8 TX/PD9 RX │  │                              │  │  PD8 TX/PD9 RX │  │
│  └───────┬────────┘  │                              │  └───────┬────────┘  │
│          │           │                              │          │           │
│  ┌────────────────┐  │   TX ──────────────── RX     │  ┌────────────────┐  │
│  │    USART6      │──│──PC6 ──────────────── PC7 ──│──│    USART6      │  │
│  │  PC6 TX/PC7 RX │──│──PC7 ──────────────── PC6 ──│──│  PC6 TX/PC7 RX │  │
│  └────────────────┘  │   RX ──────────────── TX     │  └────────────────┘  │
│                      │         GND ร่วม              │                      │
└──────────┬───────────┘                              └──────────┬───────────┘
           │ ST-Link USB                                         │ ST-Link USB
     ┌─────┴─────┐                                         ┌─────┴─────┐
     │ PC / Term │                                         │ PC / Term │
     └───────────┘                                         └───────────┘
```

| UART | Pin | Baud | หน้าที่ |
|------|-----|------|---------|
| `USART3` | PD8 TX / PD9 RX | 115200 8N1 | Serial monitor + keyboard input (ผ่าน ST-Link USB) |
| `USART6` | PC6 TX / PC7 RX | 115200 8N1 | สื่อสารระหว่าง 2 บอร์ด (สายไขว้) |

---

## 2. โครงสร้างโปรเจกต์

```
Lab_Duo/
├── Core/
│   ├── Inc/                         ← Header files
│   │   ├── main.h
│   │   ├── usart.h                  ← ประกาศ huart3, huart6
│   │   ├── stm32f7xx_it.h
│   │   └── stm32f7xx_hal_conf.h
│   ├── Src/
│   │   ├── main.c                   ← ★ ไฟล์ที่เราเขียนโค้ด
│   │   ├── usart.c                  ← UART init (CubeMX)
│   │   ├── stm32f7xx_it.c          ← ISR handlers (CubeMX)
│   │   ├── stm32f7xx_hal_msp.c     ← Pin/Clock/NVIC config
│   │   └── system_stm32f7xx.c
│   └── Startup/
│       └── startup_stm32f767zitx.s  ← Vector table
├── Drivers/
│   ├── CMSIS/                       ← ARM Cortex-M7 defs
│   └── STM32F7xx_HAL_Driver/        ← HAL library source
├── Lab_Duo.ioc                      ← CubeMX config file
└── STM32F767ZITX_FLASH.ld           ← Linker script
```

> **กฎสำคัญ:** เขียนโค้ดเฉพาะใน `/* USER CODE BEGIN */` ถึง `/* USER CODE END */` เท่านั้น — ถ้า regenerate จาก CubeMX โค้ดนอก block จะถูกเขียนทับ

---

## 3. แผนผังโค้ดใน main.c

โค้ดทั้งหมดที่เราเขียนอยู่ใน 7 ตำแหน่ง:

| USER CODE Block | บรรทัด | เนื้อหา |
|-----------------|--------|---------|
| `Includes` | 26–27 | `string.h`, `stdio.h` |
| `PD` | 37–40 | `#define IS_UART1`, `BUF_SIZE`, `NAME_SIZE` |
| `PV` | 51–66 | ตัวแปร global: buffers, flags, ชื่อผู้สนทนา |
| `BEGIN 0` | 78–109 | Helper functions: `send_str`, `send_line`, `read_line_from_keyboard`, `wait_remote_line` |
| `BEGIN 2` | 147–196 | Init: เปิด interrupt, แสดง banner, handshake ชื่อ |
| `BEGIN 3` | 206–251 | Main chat loop: สลับส่ง/รับข้อความ + quit |
| `BEGIN 4` | 310–343 | `HAL_UART_RxCpltCallback` — หัวใจของระบบ interrupt |

---

## 4. ตัวแปรสำคัญ

### Keyboard (USART3)

| ตัวแปร | หน้าที่ |
|--------|---------|
| `rx3_byte` | เก็บ 1 byte ที่รับจาก keyboard |
| `kb_buf[]` | สะสม byte ที่พิมพ์ จนกด Enter |
| `kb_idx` | ตำแหน่งปัจจุบันใน buffer |
| `kb_ready` | flag = 1 เมื่อกด Enter |

### Remote (USART6)

| ตัวแปร | หน้าที่ |
|--------|---------|
| `rx6_byte` | เก็บ 1 byte ที่รับจากอีกบอร์ด |
| `remote_buf[]` | สะสม byte จากอีกฝั่ง |
| `remote_idx` | ตำแหน่งปัจจุบันใน buffer |
| `remote_ready` | flag = 1 เมื่อรับ `\r` |

### State

| ตัวแปร | หน้าที่ |
|--------|---------|
| `my_name[]` | ชื่อที่ตัวเองพิมพ์ |
| `other_name[]` | ชื่อที่รับมาจากอีกฝั่ง |
| `my_turn` | `1` = ฝั่งนี้พิมพ์ / `0` = รอรับ |
| `chat_ended` | `1` = จบการสนทนาแล้ว |

> ตัวแปรที่ถูกแก้ไขใน interrupt callback ต้องประกาศเป็น `volatile` เสมอ — บอก compiler ว่าค่าอาจเปลี่ยนได้ตลอดเวลาจาก ISR อย่า optimize ออก

---

## 5. Interrupt Flow — เกิดอะไรเมื่อรับ byte

เมื่อ UART รับ byte สำเร็จ จะเกิด chain การเรียกฟังก์ชัน 4 ขั้น ข้ามไฟล์:

```
 ┌─────────────────────────────────────────────────────────────────┐
 │  ① HARDWARE                                                    │
 │  UART รับ byte → trigger IRQ                                   │
 └──────────────────────┬──────────────────────────────────────────┘
                        ▼
 ┌─────────────────────────────────────────────────────────────────┐
 │  ② startup_stm32f767zitx.s                                     │
 │  Vector Table → กระโดดไปที่ ISR ตาม interrupt number            │
 └──────────────────────┬──────────────────────────────────────────┘
                        ▼
 ┌─────────────────────────────────────────────────────────────────┐
 │  ③ stm32f7xx_it.c                                              │
 │  USART3_IRQHandler()  →  HAL_UART_IRQHandler(&huart3)          │
 │  USART6_IRQHandler()  →  HAL_UART_IRQHandler(&huart6)          │
 └──────────────────────┬──────────────────────────────────────────┘
                        ▼
 ┌─────────────────────────────────────────────────────────────────┐
 │  ④ HAL Driver (Drivers/)                                       │
 │  HAL_UART_IRQHandler()                                         │
 │  ตรวจว่ารับ byte ครบตาม size → เรียก callback                   │
 └──────────────────────┬──────────────────────────────────────────┘
                        ▼  __weak override
 ┌─────────────────────────────────────────────────────────────────┐
 │  ⑤ main.c — OUR CODE                                           │
 │  HAL_UART_RxCpltCallback()                                     │
 │                                                                 │
 │    ❶ ตรวจ: USART3 หรือ USART6?                                 │
 │    ❷ ถ้า \r → set flag ready = 1                                │
 │    ❸ ถ้าไม่ → เก็บ byte ลง buffer                               │
 │    ❹ เรียก HAL_UART_Receive_IT() ซ้ำ                            │
 │       ↑ ต้องเรียกทุกครั้ง ไม่งั้นรับได้แค่ byte เดียว              │
 └──────────────────────┬──────────────────────────────────────────┘
                        ▼  set flag
 ┌─────────────────────────────────────────────────────────────────┐
 │  ⑥ main.c — while(1) loop                                      │
 │  เช็ค flag → ประมวลผลข้อความ                                    │
 └─────────────────────────────────────────────────────────────────┘
```

### ทำไม HAL เรียก callback ของเราได้?

HAL ประกาศ callback เป็น `__weak` (ตัวเปล่าไม่ทำอะไร):

```c
// ใน HAL Driver — stm32f7xx_hal_uart.c
__weak void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  UNUSED(huart);  // ไม่ทำอะไรเลย
}
```

พอเราเขียนฟังก์ชันชื่อเดียวกันใน `main.c` — **Linker เลือกตัวที่ไม่ใช่ weak → ใช้ของเราอัตโนมัติ** ไม่ต้อง register ไม่ต้อง pointer แค่ชื่อตรงกัน

---

## 6. ลำดับการทำงาน (Chat Protocol)

### Phase 1: Name Handshake

```
    Board A (UART1)                          Board B (UART2)
         │                                        │
    ① แสดง banner                             แสดง banner
    ② prompt "Name:"                          รอ...
    ③ User พิมพ์ "Mr.One"                          │
         │                                        │
         │────── "Mr.One\r" (USART6) ──────►      │
         │                                   รับชื่อ → แสดง
         │                                   "Mr.One is ready"
         │                                        │
         │                                   ④ prompt "Name:"
         │                                   User พิมพ์ "Mr.Two"
         │                                        │
         │      ◄────── "Mr.Two\r" (USART6) ──────│
    รับชื่อ → แสดง                                 │
    "Mr.Two is ready"                              │
         │                                        │
    ┌────┴─────────┐                     ┌────────┴────────┐
    │ my_turn = 1  │                     │  my_turn = 0    │
    │ (ส่งก่อน)     │                     │  (รอรับ)        │
    └──────────────┘                     └─────────────────┘
```

### Phase 2: Chat Loop

```
    Board A                                  Board B
         │                                        │
    Mr.One => พิมพ์ข้อความ                          │
         │────── "Hi there!\r" ──────────►         │
         │                               แสดง Mr.One : Hi there!
         │                                        │
         │                               Mr.Two => พิมพ์ตอบ
         │         ◄────── "Hi!!\r" ──────────     │
    แสดง Mr.Two : Hi!!                             │
         │                                        │
         ╰──────── ↻ วนซ้ำ สลับ turn ───────────╯
         │                                        │
    พิมพ์ q + Enter                                 │
         │────── "q\r" ──────────────────►         │
    "Chat ended."                        "Chat ended by other side."
```

- `=>` = ฝั่งที่กำลังพิมพ์ (ตัวเอง)
- `:` = ข้อความที่รับมาจากอีกฝั่ง

---

## 7. ฟังก์ชันทั้งหมด

| ฟังก์ชัน | หน้าที่ |
|----------|---------|
| `send_str(huart, s)` | ส่ง string ไปยัง UART ที่ระบุ (blocking transmit) |
| `send_line(huart, s)` | ส่ง string + `\r\n` (ขึ้นบรรทัดใหม่) |
| `read_line_from_keyboard()` | รอ user พิมพ์จน Enter — reset buffer แบบ interrupt-safe แล้ว busy-wait จนกว่า `kb_ready = 1` |
| `wait_remote_line()` | รอข้อความจากอีกบอร์ดจน `\r` — reset buffer แบบ interrupt-safe แล้ว busy-wait จนกว่า `remote_ready = 1` |
| `HAL_UART_RxCpltCallback()` | ถูกเรียกอัตโนมัติเมื่อรับ 1 byte สำเร็จ — เก็บ byte, set flag, เรียก `Receive_IT` ซ้ำ |

---

## 8. Callback ทำงานอย่างไร (ทีละ byte)

ตัวอย่าง: user พิมพ์ `Hi` แล้วกด Enter บน keyboard

1. **USART3 รับ `'H'` (0x48)** → trigger interrupt
   - Callback: echo `'H'` กลับจอ + เก็บ `kb_buf[0] = 'H'` + เรียก `Receive_IT` ซ้ำ

2. **USART3 รับ `'i'` (0x69)** → trigger interrupt
   - Callback: echo `'i'` กลับจอ + เก็บ `kb_buf[1] = 'i'` + เรียก `Receive_IT` ซ้ำ

3. **USART3 รับ `'\r'` (0x0D)** → trigger interrupt
   - Callback: ไม่เก็บ ไม่ echo → set `kb_ready = 1` + เรียก `Receive_IT` ซ้ำ

4. **Main loop** ตรวจเห็น `kb_ready == 1` → ออกจาก busy-wait
   - อ่าน `kb_buf` ได้ `"Hi"` (2 bytes, `kb_idx = 2`) → ส่งไปอีกบอร์ดผ่าน USART6

> **สำคัญ:** `HAL_UART_Receive_IT()` สั่งรับแค่ครั้งเดียวต่อ 1 call — ต้องเรียกซ้ำทุกครั้งในตอนท้ายของ callback ไม่งั้นรับได้แค่ byte แรก byte เดียว แล้วหยุดตลอดไป

---

## 9. ตัวอย่างผลลัพธ์

**Board A — Terminal:**
```
Man from U.A.R.T.1!
Quit PRESS q
    Name: Mr.One
    Mr.Two is ready
    Mr.One => Hi there!
    Mr.Two : Hi!!
    Mr.One => _
```

**Board B — Terminal:**
```
Man from U.A.R.T.2!
Quit PRESS q
    Mr.One is ready
    Name: Mr.Two
    Mr.One : Hi there!
    Mr.Two => Hi!!
    Mr.One : _
```

---

## 10. วิธีใช้งาน

1. **Flash Board A** — ตั้ง `#define IS_UART1 1` แล้ว build + flash
2. **Flash Board B** — เปลี่ยนเป็น `#define IS_UART1 0` แล้ว build + flash
3. **ต่อสาย USART6** — Board A PC6 (TX) → Board B PC7 (RX), Board A PC7 (RX) ← Board B PC6 (TX), GND ร่วม
4. **เปิด Terminal** ทั้ง 2 เครื่อง — Tera Term / CoolTerm, 115200 baud, local echo OFF
5. **Board A ตั้งชื่อก่อน** → Board B รับชื่อแล้วตั้งชื่อตัวเอง → สนทนา!

---

## 11. ปัญหาที่พบ & แก้ไข

### Race Condition ใน buffer reset

ปัญหาเดิม: `wait_remote_line()` reset `remote_idx = 0` แล้วค่อย reset `remote_ready = 0` — ถ้า interrupt แทรกตรงกลาง byte ขยะอาจทำให้ `remote_ready = 1` ทั้งที่ buffer ว่าง

```c
// แก้: ห่อด้วย __disable_irq() ให้ reset เป็น atomic
void wait_remote_line(void)
{
  __disable_irq();      // ปิด interrupt ชั่วคราว
  remote_idx = 0;       // reset ทั้ง 2 ตัว
  remote_ready = 0;     // โดยไม่มี interrupt แทรก
  __enable_irq();       // เปิด interrupt กลับ
  while (!remote_ready && !chat_ended) {}
}
```

### USART6 Receive เริ่มเร็วเกินไป

ปัญหาเดิม: เรียก `HAL_UART_Receive_IT(&huart6, ...)` ตั้งแต่ต้น ก่อนที่จะพร้อมรับ — noise บนสายตอน power-up อาจ trigger callback ก่อนเวลา

```c
// แก้: เรียก Receive_IT ตอนพร้อมรับจริงๆ
#if IS_UART1
  // ... ตั้งชื่อ ส่งชื่อไปก่อน ...
  HAL_UART_Receive_IT(&huart6, &rx6_byte, 1);  // เริ่มรับตรงนี้
  wait_remote_line();                            // แล้วค่อยรอ
#else
  HAL_UART_Receive_IT(&huart6, &rx6_byte, 1);  // พร้อมรับ
  wait_remote_line();                            // รอชื่อจาก Board A
#endif
```
