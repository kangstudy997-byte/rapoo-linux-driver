#define _GNU_SOURCE
#include "rapoo_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_banner(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║   Rapoo Gaming Mouse Linux CLI Controller (C/C++ Native) ║\n");
    printf("║   Target: VT9PRO / VT3PRO MAX / VT1PRO MAX / B-Cut       ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
}

static void print_usage(const char *prog) {
    print_banner();
    printf("사용법 (Usage):\n");
    printf("  %s status                    - 마우스 배터리, DPI, 연결 상태 확인\n", prog);
    printf("  %s dpi <dpi1> [dpi2..]       - DPI 단계 설정 (예: %s dpi 2000)\n", prog, prog);
    printf("  %s polling <hz>              - 폴링레이트 설정 (125, 250, 500, 1000, 2000, 4000, 8000)\n", prog);
    printf("  %s sensor <lod_mm> <deb_ms>  - 센서 LOD (0.7~1.7mm) 및 디바운스 (1~10ms) 설정\n", prog);
    printf("  %s remap <btn_id> <func_id> [param] - 마우스 버튼 기능 매핑\n", prog);
    printf("  %s macro <slot> \"<keys>\"     - 하드웨어 매크로 업로드 (예: %s macro 0 \"p + enter\")\n", prog, prog);
    printf("  %s apply                     - 저장된 설정 파일(~/.config)을 마우스에 일괄 적용\n", prog);
    printf("  %s list-buttons              - 지원하는 마우스 버튼 번호 목록 출력\n", prog);
    printf("  %s list-functions            - 매핑 가능한 기능 번호 목록 출력\n", prog);
    printf("\n예시 (Examples):\n");
    printf("  %s status\n", prog);
    printf("  %s dpi 2000\n", prog);
    printf("  %s macro 0 \"p + enter\"\n", prog);
    printf("  %s remap 5 41 0              - 상단 버튼(Key 5)에 매크로 슬롯 0 할당\n", prog);
    printf("  %s polling 1000\n", prog);
    printf("  %s sensor 1.1 2\n\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "list-buttons") == 0) {
        print_banner();
        printf("지원하는 마우스 버튼 목록:\n");
        uint8_t btns[] = {0, 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13};
        for (size_t i = 0; i < sizeof(btns); ++i) {
            printf("  ID %2u : %-35s (레지스터 0x%04x)\n",
                   btns[i], rapoo_get_button_name(btns[i]), rapoo_get_button_register(btns[i]));
        }
        return 0;
    }

    if (strcmp(cmd, "list-functions") == 0) {
        print_banner();
        printf("매핑 가능한 기능 목록:\n");
        uint8_t funcs[] = {9, 10, 11, 12, 13, 26, 27, 36, 37, 38, 39, 40, 41};
        for (size_t i = 0; i < sizeof(funcs); ++i) {
            printf("  ID %2u : %s\n", funcs[i], rapoo_get_function_name(funcs[i]));
        }
        return 0;
    }

    rapoo_device_t dev;
    if (rapoo_open(&dev, NULL) != 0) {
        fprintf(stderr, "오류: 라푸 마우스 하드웨어 노드를 열 수 없습니다.\n");
        fprintf(stderr, "  1) 마우스 동글 또는 USB 케이블이 연결되어 있는지 확인하세요.\n");
        fprintf(stderr, "  2) udev 권한 규칙이 설치되어 있는지 확인하세요 (/etc/udev/rules.d/70-rapoo-mouse.rules).\n");
        return 1;
    }

    int exit_code = 0;

    if (strcmp(cmd, "status") == 0) {
        print_banner();
        printf("디바이스 노드 : %s (연결됨)\n", dev.path);

        rapoo_telemetry_t telem;
        printf("텔레메트리 수신 중 (1.5초 대기)... ");
        fflush(stdout);

        if (rapoo_get_telemetry(&dev, &telem, 1.5) == 0) {
            printf("완료!\n\n");
            printf("  배터리 잔량 : %u%%\n", telem.battery_percent);
            printf("  충전 상태   : %s\n", telem.is_charging ? "⚡ 유선 충전 중 (Charging)" : "🔋 무선 배터리 모드 (Battery)");
            printf("  현재 DPI    : %u DPI\n", telem.current_dpi);
        } else {
            printf("응답 없음 (마우스 절전/대기 모드)\n");
            printf("  마우스를 살짝 움직인 후 다시 실행해 보세요.\n");
        }

        rapoo_profile_t prof;
        if (rapoo_load_profile(NULL, &prof) == 0) {
            printf("\n저장된 프로필 정보:\n");
            printf("  폴링레이트  : %u Hz\n", prof.polling_rate_hz);
            printf("  LOD 높이    : %.1f mm\n", prof.lod_mm);
            printf("  디바운스    : %u ms\n", prof.debounce_ms);
            printf("  DPI 단계    : ");
            for (size_t i = 0; i < prof.stage_count; ++i) {
                if (i == prof.active_dpi_stage) {
                    printf("[%u] ", prof.dpi_stages[i]);
                } else {
                    printf("%u ", prof.dpi_stages[i]);
                }
            }
            printf("\n");
        }
    }
    else if (strcmp(cmd, "dpi") == 0) {
        if (argc < 3) {
            fprintf(stderr, "사용법: %s dpi <단계1> [단계2 ... 단계7]\n", argv[0]);
            exit_code = 1;
        } else {
            uint16_t stages[7];
            size_t count = 0;
            for (int i = 2; i < argc && count < 7; ++i) {
                stages[count++] = (uint16_t)atoi(argv[i]);
            }

            printf("마우스에 DPI 단계 전송 중... (단계 수: %zu)\n", count);
            if (rapoo_set_dpi_stages(&dev, stages, count, 0) == 0) {
                printf("✓ DPI 설정이 성공적으로 마우스 EEPROM에 플래싱되었습니다.\n");
                rapoo_profile_t prof;
                rapoo_load_profile(NULL, &prof);
                prof.stage_count = (uint8_t)count;
                for (size_t i = 0; i < count; ++i) prof.dpi_stages[i] = stages[i];
                prof.active_dpi_stage = 0;
                rapoo_save_profile(NULL, &prof);
            } else {
                fprintf(stderr, "오류: DPI 설정 전송에 실패했습니다.\n");
                exit_code = 1;
            }
        }
    }
    else if (strcmp(cmd, "polling") == 0) {
        if (argc < 3) {
            fprintf(stderr, "사용법: %s polling <125|250|500|1000|2000|4000|8000>\n", argv[0]);
            exit_code = 1;
        } else {
            uint16_t rate = (uint16_t)atoi(argv[2]);
            if (rapoo_set_polling_rate(&dev, rate) == 0) {
                printf("✓ 폴링레이트 %u Hz 설정이 성공적으로 적용되었습니다.\n", rate);
                rapoo_profile_t prof;
                rapoo_load_profile(NULL, &prof);
                prof.polling_rate_hz = rate;
                rapoo_save_profile(NULL, &prof);
            } else {
                fprintf(stderr, "오류: 유효하지 않은 폴링레이트이거나 전송에 실패했습니다.\n");
                exit_code = 1;
            }
        }
    }
    else if (strcmp(cmd, "sensor") == 0) {
        if (argc < 4) {
            fprintf(stderr, "사용법: %s sensor <lod_mm (0.7~1.7)> <debounce_ms (1~10)>\n", argv[0]);
            exit_code = 1;
        } else {
            float lod = (float)atof(argv[2]);
            uint8_t deb = (uint8_t)atoi(argv[3]);
            if (rapoo_set_sensor(&dev, lod, deb) == 0) {
                printf("✓ 센서 LOD %.1f mm, 디바운스 %u ms 설정이 성공적으로 적용되었습니다.\n", lod, deb);
                rapoo_profile_t prof;
                rapoo_load_profile(NULL, &prof);
                prof.lod_mm = lod;
                prof.debounce_ms = deb;
                rapoo_save_profile(NULL, &prof);
            } else {
                fprintf(stderr, "오류: 센서 설정 전송 실패.\n");
                exit_code = 1;
            }
        }
    }
    else if (strcmp(cmd, "remap") == 0) {
        if (argc < 4) {
            fprintf(stderr, "사용법: %s remap <버튼ID> <기능ID> [매크로슬롯파라미터]\n", argv[0]);
            exit_code = 1;
        } else {
            uint8_t btn_id = (uint8_t)atoi(argv[2]);
            uint8_t func_id = (uint8_t)atoi(argv[3]);
            uint8_t param = (argc >= 5) ? (uint8_t)atoi(argv[4]) : 0;

            if (rapoo_remap_button(&dev, btn_id, func_id, param) == 0) {
                printf("✓ 버튼 %u (%s) -> 기능 %u (%s) 매핑이 적용되었습니다.\n",
                       btn_id, rapoo_get_button_name(btn_id),
                       func_id, rapoo_get_function_name(func_id));
                rapoo_profile_t prof;
                rapoo_load_profile(NULL, &prof);
                if (btn_id < 16) {
                    prof.button_mappings[btn_id] = func_id;
                    prof.button_params[btn_id] = param;
                    rapoo_save_profile(NULL, &prof);
                }
            } else {
                fprintf(stderr, "오류: 버튼 매핑 전송 실패.\n");
                exit_code = 1;
            }
        }
    }
    else if (strcmp(cmd, "macro") == 0) {
        if (argc < 4) {
            fprintf(stderr, "사용법: %s macro <슬롯 0~5> \"<키 시퀀스>\" (예: %s macro 0 \"p + enter\")\n", argv[0], argv[0]);
            exit_code = 1;
        } else {
            uint8_t slot = (uint8_t)atoi(argv[2]);
            const char *keystr = argv[3];

            uint8_t bytecode[512];
            size_t out_len = 0;
            if (rapoo_encode_macro_string(keystr, 50, 1, 1, bytecode, sizeof(bytecode), &out_len) != 0) {
                fprintf(stderr, "오류: 매크로 문자열을 인코딩할 수 없습니다: '%s'\n", keystr);
                exit_code = 1;
            } else {
                printf("하드웨어 매크로 슬롯 %u에 바이트코드(%zu 바이트) 업로드 중...\n", slot, out_len);
                if (rapoo_upload_macro(&dev, slot, bytecode, out_len) == 0) {
                    printf("✓ 하드웨어 매크로가 마우스 플래시 메모리(0x%04x)에 성공적으로 플래싱되었습니다!\n",
                           0x1000 + (slot * 0x1000));
                } else {
                    fprintf(stderr, "오류: 매크로 업로드 실패.\n");
                    exit_code = 1;
                }
            }
        }
    }
    else if (strcmp(cmd, "apply") == 0) {
        rapoo_profile_t prof;
        if (rapoo_load_profile(NULL, &prof) != 0) {
            fprintf(stderr, "오류: 설정 파일을 불러올 수 없습니다.\n");
            exit_code = 1;
        } else {
            printf("저장된 프로필 설정을 마우스에 일괄 적용 중...\n");
            if (rapoo_apply_profile(&dev, &prof) == 0) {
                printf("✓ 모든 설정이 마우스 하드웨어에 성공적으로 일괄 적용되었습니다.\n");
            } else {
                fprintf(stderr, "오류: 설정 적용 실패.\n");
                exit_code = 1;
            }
        }
    }
    else {
        fprintf(stderr, "알 수 없는 명령: %s\n", cmd);
        print_usage(argv[0]);
        exit_code = 1;
    }

    rapoo_close(&dev);
    return exit_code;
}
