# 🔥 FireGuard Pro – Hệ thống cảnh báo cháy thông minh sử dụng ESP32 và IoT

## 1. Giới thiệu dự án

**FireGuard Pro** là hệ thống cảnh báo cháy thông minh sử dụng ESP32, cảm biến khí gas MQ-2 và cảm biến nhiệt độ DS18B20.

Hệ thống có khả năng:
- Phát hiện khí gas, khói thông qua cảm biến MQ-2.
- Đo nhiệt độ môi trường bằng cảm biến DS18B20.
- Tự động điều khiển quạt và máy bơm khi phát hiện nguy cơ cháy.
- Gửi dữ liệu cảm biến lên nền tảng MQTT.
- Giám sát trạng thái hệ thống thông qua giao diện Web Dashboard.
- Cho phép người dùng điều khiển thiết bị từ xa bằng giao diện Web.
- Sử dụng FreeRTOS để tổ chức các tác vụ hoạt động song song.

## 2. Hình ảnh tổng quan hệ thống

![FireGuard Pro System Overview](images/system-overview.png)

## 3. Mô phỏng hệ thống trên Wokwi

Mạch điện và chương trình ESP32 được xây dựng, mô phỏng trên nền tảng Wokwi.

👉 **[Nhấn vào đây để xem mô phỏng Wokwi](https://wokwi.com/projects/477916793426036737)**

## 4. Giao diện Web Dashboard

Giao diện Web Dashboard cho phép theo dõi dữ liệu cảm biến và điều khiển các thiết bị trong hệ thống.

👉 **[Nhấn vào đây để truy cập Web Dashboard](https://cheery-starlight-430776.netlify.app)**


## 5. Các linh kiện sử dụng

| STT | Linh kiện | Chức năng |
|---|---|---|
| 1 | ESP32 | Vi điều khiển trung tâm |
| 2 | MQ-2 | Phát hiện khí gas và khói |
| 3 | DS18B20 | Đo nhiệt độ |
| 4 | Relay 1 | Điều khiển quạt |
| 5 | Relay 2 | Điều khiển máy bơm |
| 6 | Quạt | Thiết bị làm mát |
| 7 | Máy bơm | Thiết bị hỗ trợ chữa cháy |

## 6. Công nghệ sử dụng

- **Phần cứng:** ESP32, MQ-2, DS18B20, Relay.
- **Lập trình nhúng:** Arduino IDE, C/C++.
- **Hệ điều hành thời gian thực:** FreeRTOS.
- **Giao thức truyền thông:** MQTT.
- **MQTT Broker:** EMQX.
- **Giao diện Web:** HTML, CSS, JavaScript.
- **Thư viện MQTT Web:** MQTT.js.
- **Mô phỏng:** Wokwi.

## 7. Nguyên lý hoạt động

1. Cảm biến MQ-2 liên tục đo nồng độ khí gas và khói.
2. Cảm biến DS18B20 liên tục đo nhiệt độ môi trường.
3. ESP32 xử lý dữ liệu thông qua các tác vụ FreeRTOS.
4. Dữ liệu cảm biến được gửi lên MQTT Broker thông qua Wi-Fi.
5. Web Dashboard kết nối MQTT để nhận và hiển thị dữ liệu theo thời gian thực.
6. Khi phát hiện nguy cơ cháy, hệ thống tự động kích hoạt các thiết bị cảnh báo và xử lý theo chương trình.
7. Người dùng có thể chuyển đổi chế độ hoạt động và điều khiển quạt, máy bơm từ giao diện Web.


## 8. Thông tin kết nối MQTT

- **Broker:** `broker.emqx.io`
- **Port MQTT:** `1883`
- **Port MQTT WebSocket:** `8084`
- **WebSocket URL:** `wss://broker.emqx.io:8084/mqtt`
- **Topic gốc:** `home/fire`

### Các MQTT Topic chính

| Topic | Chức năng |
|---|---|
| `home/fire/gas` | Dữ liệu cảm biến khí gas |
| `home/fire/temperature` | Dữ liệu nhiệt độ |
| `home/fire/mode/state` | Trạng thái chế độ hoạt động |
| `home/fire/fan/state` | Trạng thái quạt |
| `home/fire/pump/state` | Trạng thái máy bơm |
| `home/fire/states` | Trạng thái tổng hợp |
| `home/fire/alarm` | Thông tin cảnh báo |
| `home/fire/mode/set` | Lệnh thay đổi chế độ |
| `home/fire/fan/set` | Lệnh điều khiển quạt |
| `home/fire/pump/set` | Lệnh điều khiển máy bơm |

## 9. Tác giả

**Bùi Đức Mạnh**

- Trường: Đại học Giao thông Vận tải
- Ngành: Điện tử – Viễn thông
- Đề tài: Hệ thống cảnh báo cháy thông minh sử dụng ESP32 và IoT

---

*FireGuard Pro – Smart Fire Detection and Monitoring System.*