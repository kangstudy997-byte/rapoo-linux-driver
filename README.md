# Rapoo Gaming Mouse Native Linux Driver (C / C++ Native Engine)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C99 / C++](https://img.shields.io/badge/Language-C99%20%2F%20C%2B%2B-blue.svg)](https://en.cppreference.com/)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-orange.svg)](https://www.kernel.org)
[![GUI: GTK3](https://img.shields.io/badge/GUI-GTK3-brightgreen.svg)](https://www.gtk.org/)

리눅스(Fedora, Ubuntu, Arch 등) 환경에서 **Rapoo 게이밍 마우스**(VT3PRO MAX, VT1PRO, VT9PRO 시리즈 등)를 파이썬이나 외부 데몬 없이 **100% 순수 C/C++ 온보드 하드웨어 직접 제어**로 구동하는 초경량 네이티브 드라이버 및 GTK3 GUI 제어 센터입니다.

---

## 🎯 지원 대상 기기 (Tested Devices)

- **Rapoo VT3PRO MAX** (Wireless 2.4GHz / Wired, Realtek B-Cut MCU, VID:PID `24ae:1244`)
- **Rapoo VT1PRO MAX** (Realtek B-Cut MCU, VID:PID `24ae:1244`)
- **Rapoo VT9PRO / VT9PRO 4K / VT9Air** (Realtek B-Cut & Nordic MCU 계열)

---

## ✨ 핵심 아키텍처 및 특징

1. **100% Pure C / C++ 네이티브 아키텍처 (Zero Python Dependencies)**:
   - 파이썬 런타임 오버헤드 없이 `gcc` 단일 컴파일러로 즉시 빌드됩니다.
   - CLI 실행 파일 용량 **40 KB**, 실행 시간 **6ms**, 메모리 사용량 **1.5 MB** 미만의 극한의 최적화.
2. **C++ 프로젝트용 모던 RAII 헤더 지원 (`include/rapoo_mouse.hpp`)**:
   - `rapoo::Mouse dev; dev.setDpiStages({2000}); dev.commit();` 등 C++17/20 STL 기반 객체 지향 제어 제공.
3. **100% 온보드 하드웨어 제어 (Zero Software Injector)**:
   - 가상 입력 주입(`uinput`, `xdotool` 등) 없이 마우스 내부 MCU EEPROM/플래시 메모리에 직접 설정값을 기록하므로 OS 백그라운드 프로세스 오버헤드가 전혀 없습니다.
4. **온보드 하드웨어 매크로 플래셔**:
   - `P + Enter` (50ms 딜레이 등) 동시/연속 입력 매크로를 물리 버튼에 할당.
   - 마우스 자체 펌웨어가 물리 키보드 스캔코드를 USB 파이프로 직접 발송.
5. **DPI 다이렉트 플래싱 (X/Y 독립 제어)**:
   - 50 ~ 26000 DPI 범위 내 16바이트 직렬 uint16 LE 다이렉트 규격 지원.
6. **광학 센서 정밀 튜닝 (LOD & Debounce & Motion Sync)**:
   - Lift-Off Distance (0.7mm ~ 1.7mm, 기본 권장 1.1mm) 최적화.
   - 클릭 디바운스 딜레이 (1ms ~ 10ms) 더블클릭 방지 및 초저지연 세팅.
7. **GTK3 네이티브 GUI 제어 센터 (`rapoo-mouse-gui`)**:
   - 실시간 배터리 잔량 표시, 프리셋 전환, 시각적 버튼 리매핑 및 온보드 매크로 플래셔 탑재.
8. **부팅 자동 영속화 (systemd user oneshot)**:
   - 부팅 시 0.006초 만에 하드웨어 상태를 동기화하고 즉시 종료되어 PC CPU/RAM 점유율 0.00%.

> [!TIP]
> 📖 GUI 화면 구성, 온보드 하드웨어 매크로 작성법, 자주 묻는 질문(FAQ) 등 상세한 안내는 **[상세 매뉴얼 가이드 (docs/MANUAL.md)](docs/MANUAL.md)**를 확인하세요.

---

## 🛠️ 빌드 및 설치 (Build & Install)

### 1. 요구 사양 (Build Requirements)
- `gcc`, `make`, `pkg-config`
- `gtk+-3.0` (GUI 빌드용)

### 2. 빌드
```bash
make all      # CLI, GUI, 정적 라이브러리(librapoo.a) 빌드
```

### 3. 설치
```bash
./install.sh  # 컴파일 후 ~/.local/bin 및 udev, systemd 서비스 자동 등록
```

---

## 🖥️ 사용 방법

### 1. GUI 제어 센터 실행
```bash
rapoo-mouse-gui
```

### 2. CLI 커맨드라인 도구 (`rapoo-mouse`)
```bash
# 마우스 연결 상태 및 실시간 텔레메트리/배터리 확인
rapoo-mouse status

# DPI 단계 설정 (예: 단일 2000 DPI 또는 다중 단계)
rapoo-mouse dpi 2000
rapoo-mouse dpi 800 1200 1600 2000 2400

# 폴링레이트 설정 (Hz: 125, 250, 500, 1000, 2000, 4000, 8000)
rapoo-mouse polling 1000

# 센서 LOD(1.1mm) 및 디바운스(2ms) 조절
rapoo-mouse sensor 1.1 2

# 하드웨어 매크로 업로드 (슬롯 0에 P+Enter 업로드)
rapoo-mouse macro 0 "p + enter"

# 버튼 기능 리매핑 (Key 5 상단 버튼에 매크로 슬롯 0 할당)
rapoo-mouse remap 5 41 0

# 버튼 및 기능 번호 목록 확인
rapoo-mouse list-buttons
rapoo-mouse list-functions

# 저장된 프로필 마우스 EEPROM에 일괄 플래싱
rapoo-mouse apply
```

---

## 📂 프로젝트 구조

```
rapoo-linux-driver/
├── include/
│   ├── rapoo_driver.h       # C 언어 코어 드라이버 API 헤더
│   └── rapoo_mouse.hpp      # C++ RAII 래퍼 클래스 헤더
├── src/
│   ├── rapoo_driver.c       # HID IOCTL 통신 및 레지스터 인코딩 엔진 (C99)
│   ├── cli_main.c           # CLI 제어 유틸리티 (C99)
│   └── gui_main.c           # GTK3 네이티브 GUI 제어 센터 (C99)
├── Makefile                 # 표준 빌드 시스템
├── udev/
│   └── 70-rapoo-mouse.rules # /dev/hidraw 비루트 사용자 접근 규칙
├── systemd/
│   └── rapoo-mouse.service  # 부팅 시 자동 프로파일 동기화 서비스
├── profiles/
│   └── default.json         # 기본 권장 프로파일
├── macros/
│   └── macros.json          # 기본 사전 정의 매크로 세트
├── install.sh               # 원클릭 컴파일 & 설치 스크립트
├── uninstall.sh             # 언인스톨 스크립트
├── LICENSE                  # MIT 라이선스
└── README.md                # 설명서
```

---

## 📜 C++ 라이브러리 사용 예시

다른 C++ 프로젝트에서 이 드라이버를 직접 포함하여 사용할 수 있습니다:

```cpp
#include "rapoo_mouse.hpp"
#include <iostream>

int main() {
    rapoo::Mouse mouse;
    if (!mouse.open()) {
        std::cerr << "마우스를 찾을 수 없습니다.\n";
        return 1;
    }

    // 2000 DPI 설정
    mouse.setDpiStages({2000});

    // 1000 Hz 폴링레이트 설정
    mouse.setPollingRate(1000);

    // 센서 설정 (LOD 1.1mm, 디바운스 2ms)
    mouse.setSensor(1.1f, 2);

    // 하드웨어 EEPROM 커밋
    mouse.commit();

    std::cout << "설정이 마우스에 적용되었습니다!\n";
    return 0;
}
```

---

## 🛡️ 라이선스 및 고지 (License & Disclaimer)

- **License**: 본 프로젝트는 [MIT License](LICENSE)에 따라 자유롭게 수정, 배포 및 사용할 수 있습니다.
- **Disclaimer**: This is an independent, community-driven open-source project created strictly for Linux hardware interoperability. It is not affiliated with, sponsored by, or endorsed by Shenzhen Rapoo Technology Co., Ltd. "Rapoo" and related product names are trademarks of Shenzhen Rapoo Technology Co., Ltd., used solely for compatibility identification.
