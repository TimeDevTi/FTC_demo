# Mecanum Teleop — UNO Q + UNO R4 Minima

โครงงาน PlatformIO สำหรับ VS Code ที่จัดไฟล์ตามแนว `WRO_Tools_UnoR3` แต่แยกงานควบคุมหุ่นยนต์ mecanum สี่ล้อออกจาก mission เดิม

## โครงระบบ

- **Arduino UNO Q:** Linux Debian เปิดเว็บ UI/HTTP บนพอร์ต 8080; sketch บน MCU รับคำสั่ง RPC จาก Linux แล้วส่งเฟรม UART ไป R4 Minima
- **Arduino UNO R4 Minima:** รับ UART, คำนวณ mecanum mixing, ขับ MDD3A สองช่องแบบ PWM/PWM และ MD10C R3 สองตัวแบบ PWM/DIR; หยุดเมื่อไม่มีคำสั่ง/deadman
- **Web UI:** จอย touch/mouse; บนคอมพิวเตอร์ใช้ `W/A/S/D` ขับและกด `J/L` ค้างเพื่อหมุน มีปุ่ม `STOP` และหยุดเมื่อปล่อยจอย/เสีย focus
- **Digital twin:** มุมมองบนแบบ 2D ตามสไตล์การจำลองใน `web_v8`; แสดงล้อและ PWM ที่ตอบกลับจาก R4 เท่านั้น ไม่ใช่ odometry/ตำแหน่งจริง เพราะยังไม่มี encoder/IMU รวมในเฟิร์มแวร์นี้
- **ลิงก์บอร์ด:** UART `Serial1` 115200 baud, framed packet พร้อม CRC-8

## โครงสร้างไฟล์

```text
include/link/DriveProtocol.h      UART frames และ CRC
include/robot/Config.h            pin mapping และ timeout ของ R4
include/robot/Mecanum.h           mecanum mixing
src/uno_q/main.cpp                RPC bridge และ UART TX/RX ของ UNO Q MCU
src/r4_minima/main.cpp            มอเตอร์, mixing และ failsafe ของ R4
uno_q/web_server.py               HTTP server ฝั่ง Linux ของ UNO Q
uno_q/web/index.html              touch/keyboard UI + twin 2D
uno_q/start_hotspot.sh            เปิด Wi-Fi hotspot แบบสั่งเอง
platformio.ini                    PlatformIO environments
test/native/test_mecanum.cpp       native tests ของ mixing/protocol
tools/test_native.py               runner ของ native tests
docs/HARDWARE.md                  pinout, wiring และข้อจำกัด
```

## Build / upload ด้วย PlatformIO

### UNO R4 Minima — PlatformIO ทางการ

เลือก environment `r4_minima` ในแถบ PlatformIO ของ VS Code:

```sh
pio run -e r4_minima
pio run -e r4_minima -t upload
```

PlatformIO ใช้ board ID `uno_r4_minima` และ protocol `dfu`; เลือก serial/upload port ให้ตรงกับบอร์ดและวิธีที่เครื่องใช้ก่อน upload หากตรวจไม่พบอัตโนมัติ

### UNO Q — ใช้ community platform

UNO Q ใช้ PlatformIO platform `lee-lab-skku/arduinoq@1.0.0-rc.4` (ยังเป็น unofficial/release candidate; ไม่ใช่ support ทางการของ PlatformIO) Build host ต้องเป็น Linux x86_64/aarch64; upload/debug ผ่าน MPU ของ UNO Q และ OpenOCD บนบอร์ด ไม่ใช่ upload ผ่าน USB แบบ R4

- ถ้าเปิดโปรเจกต์ PlatformIO บน UNO Q/Linux ให้รัน `pio run -e uno_q` และ `pio run -e uno_q -t upload` จาก MPU ตาม README ของ community platform
- ถ้าใช้ VS Code บนคอมพิวเตอร์ Linux สามารถ build ได้ และต้องตั้งค่า PlatformIO Remote/remote build ให้ upload ไปทำบน UNO Q; บน Windows/macOS ต้องใช้ `pio remote run -r`/`--force-remote` ตาม platform guide
- ต้องเปิด `arduino-router.service` บน UNO Q สำหรับ upload และ RPC
- Build ทั้งสอง environment ผ่านบน Linux x86_64 ในเครื่องนี้; ยังไม่ได้ upload หรือทดสอบการขับจริง เพราะไม่มีบอร์ดเชื่อมต่อ

เอกสาร platform/วิธี remote ล่าสุด: [PlatformIO UNO Q community platform](https://github.com/lee-lab-skku/platform-arduinoq) และ [กระทู้สถานะ/ข้อจำกัด](https://community.platformio.org/t/a-custom-development-platform-for-arduino-uno-q/54299)

### Native tests

```sh
python3 tools/test_native.py
```

ใช้ `g++` และ Python standard library เท่านั้น

## เปิดเว็บบน UNO Q

1. ตั้งชื่อ Wi-Fi และรหัสผ่าน hotspot ใน environment (ห้ามใส่ password จริงลง git) แล้วเปิด hotspot ด้วย `uno_q/start_hotspot.sh` ดูเงื่อนไข/ข้อควรระวังใน [docs/HARDWARE.md](docs/HARDWARE.md)
2. ติดตั้ง Python bridge บน UNO Q:
   ```sh
   python3 -m pip install -r uno_q/requirements.txt
   ```
3. Build/upload sketch `uno_q` ให้ MCU และเชื่อม Serial1 ระหว่างบอร์ดก่อน
4. เริ่ม HTTP server บน Linux:
   ```sh
   python3 uno_q/web_server.py
   ```
5. ต่อมือถือ/คอมเข้ากับ hotspot ของ UNO Q แล้วเปิด `http://<UNO-Q-IP>:8080/` (localhost ใช้ได้เฉพาะ browser ที่รันบน UNO Q เอง; เครื่องอื่นต้องใช้ IP ของบอร์ด)

เว็บ server bind `0.0.0.0` เพื่อให้ client ใน LAN เข้าถึงได้ ให้ใช้เฉพาะ hotspot/เครือข่ายทดสอบที่มีรหัสผ่านและไม่เปิดให้บุคคลอื่นเข้าถึง หากจะใช้บนเครือข่ายร่วม/อินเทอร์เน็ต ต้องเพิ่ม authentication และ HTTPS ก่อน

## สถานะฮาร์ดแวร์ที่ยังต้องยืนยัน

- จากภาพระบุ driver ด้านบนเป็น MDD3A โดยประมาณ และสองตัวล่างเป็น MD10C R3; ยืนยันข้อความรุ่นบนบอร์ดด้านบนก่อนต่อสาย
- mapping ปัจจุบันกำหนด MDD3A คุมล้อหน้า FL/FR และ MD10C สองตัวคุมล้อหลัง RL/RR; ยืนยันตำแหน่งมอเตอร์/ช่อง A-B และทิศล้อขณะยกล้อพ้นพื้น
- ตรวจแรงดันแบตเตอรี่ให้เข้ากับทั้ง MDD3A (4–16 V) และ MD10C (5–30 V) พร้อมตรวจ stall current ของมอเตอร์แต่ละล้อเทียบกับขีดจำกัดของ channel ก่อนจ่ายไฟ
- โปรดทดสอบยกล้อพ้นพื้นและตั้ง current limit/ใช้ battery disconnect ก่อนทดสอบจริง; ไม่มี software e-stop แทน hardware cutoff
- R4 input/output UART และ UNO Q เป็นคนละ logic-voltage domain: ใช้ level shifter สำหรับ TX/RX ทั้งสองทิศและต่อ GND ร่วม
