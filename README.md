
## Introduction

This repository includes the Arduino project files and documentation for the final project for  **ECE 528** class (Robotics and Embedded Systems) from **Fall 2025** at **California State University, Northridge (CSUN)**.


* Click on **[Final Project Report](docs/Final_Project_Report.docx)** to view the report that contains additional details on system architecture and testing.
* **[Click here to watch a short video demonstration]** *([Link to your YouTube video](https://www.youtube.com/watch?v=tOG5r1Idgxo))*.
* **[Click here to view the presentation slides](docs/FPV RC Car Presentation.pptx)**.

---

## Description

Implemented in **C++** using the **Seeed Studio XIAO ESP32S3 Sense** microcontroller, this project involves retrofitting a toy-grade RC car chassis with a modern wireless control system.

The firmware utilizes the ESP32's **LEDC peripheral** (hardware timer) to generate **1000 Hz PWM signals** for precise motor speed control. A **Hybrid Drive System** was designed using a high-current **BTS7960** driver for the main traction motors and a **DRV8833** driver for the steering mechanism. The steering logic was reverse-engineered to control a 5-wire servo using H-Bridge logic.

The system hosts an **asynchronous web server** directly on the microcontroller, creating a local Wi-Fi Access Point (`FPV_RC_CAR`). The web interface features a custom **dual-joystick controller** (HTML5/JavaScript) that allows the user to drive the car while viewing a real-time, low-latency video stream from the onboard OV2640 camera.

The overall power system is stabilized using an **LM2596 Buck Converter** to isolate sensitive logic circuits from high-current motor loads, preventing system brownouts during operation.

---

## Block Diagram

### **System Architecture:**

![System Block Diagram](hardware/fpv_rc_car2.drawio.png)

*(Note: Ensure your block diagram image is uploaded to the `hardware/` folder)*

---

## Building the FPV RC Car

| Component | Description |
| :--- | :--- |
| **Microcontroller** | Seeed Studio XIAO ESP32S3 Sense (Dual Core, 240MHz) |
| **Drive Driver** | BTS7960 High-Current (43A) H-Bridge |
| **Steering Driver** | DRV8833 Dual H-Bridge Driver |
| **Motors** | 2x 390 Brushed DC Motors (Wired in Parallel) |
| **Steering** | 5-Wire Servo (Modified for DC Operation) |
| **Power** | 7.4V Li-Ion Battery (1500mAh) + LM2596 Regulator |

### **Wiring Implementation:**

![Wiring Diagram](hardware/Wiring_Diagram.png)

*(Note: Ensure your wiring diagram image is uploaded to the `hardware/` folder)*

---

## Video Demonstration Links

### **Short Video Demonstration:**
[Link to Video Demo 1]

### **Web Interface Walkthrough:**
[Link to Video Demo 2]
