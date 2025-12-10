# Raspberry Pi sensor, lcd driver for Monitoring System

## Yocto기반 통합 BSP

https://github.com/Jminu/Yocto-rasp-BSP


## Overview
+ 디바이스 드라이버, Device Tree 작성
+ SHT20과 HD44780(I2C LCD) 제어
+ 버튼 입력으로 모드 전환, 버튼IRQ, WaitQueue
+ 멀티 프로세세스 및 공유 메모리 기법을 활용하여 실시간 데이터 모니터링
+ Yocto 통합

## Tech Stack
+ Hardware: Raspberry Pi 4B, SHT20, HD44780, Tactile Button
+ Kernel Space: 문자 디바이스 드라이버, i2c, 인터럽트 핸들링, wait queue
+ User Space: Multi-process, IPC(shared memory), read/write
+ Tools: GCC, Makefile, Datasheet, Yocto

## Feature

### 1. I2C Device Drivers (SHT20 & HD44780)
* **Direct Control:** 라이브러리 없이 데이터시트 분석하여 I2C 통신 및 제어 로직 구현.
* **4-bit Mode LCD:** I/O 핀 부족 문제를 해결하기 위해 상위/하위 니블(Nibble) 분할 전송 및 제어 신호 패키징 로직 구현.
* **Optimization:** 커널 내부 부동소수점 연산 회피를 위한 **고정 소수점 연산(Fixed-Point Arithmetic)** 적용.

### 2. 버튼 IRQ
* **Problem:** 기존 폴링(Polling) 방식의 `read()`는 무의미한 루프 반복으로 CPU 자원을 낭비함.
* **Solution:** **GPIO Interrupt**와 **Wait Queue**를 결합하여 구현.
    * 입력 대기 시 프로세스를 **Sleep 상태**로 전환 (CPU 점유율 0%).
    * 인터럽트 발생 시에만 프로세스를 **Wake-up** 하여 즉각 반응.

### 3. 멀티 프로세스 & IPC
* **Concurrency:** 센서 데이터 처리와 사용자 입력 대기를 병렬로 수행하기 위해 `fork()`를 사용하여 프로세스 분리.
* **Synchronization:** **System V Shared Memory**를 활용하여 프로세스 간 측정 모드(온도 ↔ 습도) 상태를 실시간 동기화하여 데이터 무결성 확보.
