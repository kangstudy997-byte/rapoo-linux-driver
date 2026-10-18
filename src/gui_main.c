#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "rapoo_driver.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *header_bar;
    GtkWidget *bat_badge;
    GtkWidget *status_label;
    GtkWidget *apply_btn;

    /* DPI Widgets */
    GtkWidget *dpi_spin_active;
    GtkWidget *dpi_scale_active;
    GtkWidget *dpi_stage_spins[7];
    GtkWidget *dpi_stage_radios[7];
    GtkWidget *dpi_stage_count_combo;

    /* Sensor & Polling */
    GtkWidget *polling_combo;
    GtkWidget *lod_scale;
    GtkWidget *debounce_spin;
    GtkWidget *motionsync_switch;

    /* Buttons */
    GtkWidget *btn_combos[16];
    GtkWidget *btn_macro_slots[16];

    /* Macro section */
    GtkWidget *macro_slot_combo;
    GtkWidget *macro_key_entry;
    GtkWidget *macro_delay_spin;
    GtkWidget *macro_flash_btn;

    /* Info Bar / Notification */
    GtkWidget *info_bar;
    GtkWidget *info_label;

    /* State */
    rapoo_profile_t profile;
    bool is_busy;
} AppWidgets;

static AppWidgets g_app;

static const char *DARK_CSS =
    "window { background-color: #181825; color: #cdd6f4; font-family: 'Noto Sans CJK KR', 'Cantarell', sans-serif; }\n"
    "headerbar { background-color: #11111b; border-bottom: 1px solid #313244; color: #cdd6f4; }\n"
    "notebook tab { font-weight: bold; padding: 10px 18px; border-radius: 6px 6px 0 0; color: #a6adc8; }\n"
    "notebook tab:checked { background-color: #313244; color: #89b4fa; }\n"
    "frame { border: 1px solid #313244; border-radius: 8px; padding: 12px; margin: 6px; background-color: #1e1e2e; }\n"
    "button { border-radius: 6px; font-weight: bold; padding: 6px 14px; }\n"
    "button.suggested-action { background-color: #a6e3a1; color: #11111b; font-weight: bold; border: none; }\n"
    "button.suggested-action:hover { background-color: #94e2d5; }\n"
    "label.title { font-size: 15px; font-weight: bold; color: #89b4fa; margin-bottom: 6px; }\n"
    "label.subtitle { font-size: 12px; color: #a6adc8; }\n"
    ".badge-green { background-color: #1c362b; color: #a6e3a1; border-radius: 12px; padding: 4px 10px; font-weight: bold; }\n"
    ".badge-yellow { background-color: #3b331e; color: #f9e2af; border-radius: 12px; padding: 4px 10px; font-weight: bold; }\n"
    ".badge-blue { background-color: #1e293b; color: #89b4fa; border-radius: 12px; padding: 4px 10px; font-weight: bold; }\n"
    "entry, spinbutton, combobox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 6px; }\n";

static void show_notification(const char *msg, bool is_success) {
    if (!g_app.info_label || !g_app.info_bar) return;
    gtk_label_set_text(GTK_LABEL(g_app.info_label), msg);
    gtk_info_bar_set_message_type(GTK_INFO_BAR(g_app.info_bar),
                                  is_success ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR);
    gtk_widget_show(g_app.info_bar);
}

typedef struct {
    char message[512];
    bool success;
} AsyncResult;

static gboolean on_async_done(gpointer user_data) {
    AsyncResult *res = (AsyncResult *)user_data;
    g_app.is_busy = false;
    gtk_widget_set_sensitive(g_app.apply_btn, TRUE);
    gtk_button_set_label(GTK_BUTTON(g_app.apply_btn), "마우스에 즉시 적용 (Apply)");

    if (g_app.macro_flash_btn) {
        gtk_widget_set_sensitive(g_app.macro_flash_btn, TRUE);
        gtk_button_set_label(GTK_BUTTON(g_app.macro_flash_btn), "마우스 플래시 메모리에 업로드 (Flash)");
    }

    show_notification(res->message, res->success);
    free(res);
    return G_SOURCE_REMOVE;
}

static gpointer apply_thread_func(gpointer user_data) {
    (void)user_data;
    AsyncResult *res = (AsyncResult *)malloc(sizeof(AsyncResult));
    memset(res, 0, sizeof(*res));

    rapoo_device_t dev;
    if (rapoo_open(&dev, NULL) != 0) {
        snprintf(res->message, sizeof(res->message), "❌ 마우스 하드웨어 노드를 열 수 없습니다. 연결을 확인하세요.");
        res->success = false;
    } else {
        if (rapoo_apply_profile(&dev, &g_app.profile) == 0) {
            rapoo_save_profile(NULL, &g_app.profile);
            snprintf(res->message, sizeof(res->message), "✓ 모든 설정이 마우스 온보드 EEPROM에 성공적으로 저장되었습니다!");
            res->success = true;
        } else {
            snprintf(res->message, sizeof(res->message), "❌ 마우스 EEPROM 플래싱 중 오류가 발생했습니다.");
            res->success = false;
        }
        rapoo_close(&dev);
    }

    g_idle_add(on_async_done, res);
    return NULL;
}

static void on_apply_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    if (g_app.is_busy) return;
    g_app.is_busy = true;

    gtk_widget_set_sensitive(g_app.apply_btn, FALSE);
    gtk_button_set_label(GTK_BUTTON(g_app.apply_btn), "마우스에 플래싱 중...");

    /* Collect values from GUI into g_app.profile */
    int count = gtk_combo_box_get_active(GTK_COMBO_BOX(g_app.dpi_stage_count_combo)) + 1;
    if (count < 1) count = 1;
    if (count > 7) count = 7;
    g_app.profile.stage_count = (uint8_t)count;

    for (int i = 0; i < count; ++i) {
        g_app.profile.dpi_stages[i] = (uint16_t)gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(g_app.dpi_stage_spins[i]));
    }

    /* Polling rate */
    int poll_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(g_app.polling_combo));
    const uint16_t polls[] = {125, 250, 500, 1000, 2000, 4000, 8000};
    if (poll_idx >= 0 && poll_idx < 7) {
        g_app.profile.polling_rate_hz = polls[poll_idx];
    }

    /* Sensor */
    g_app.profile.lod_mm = (float)gtk_range_get_value(GTK_RANGE(g_app.lod_scale));
    g_app.profile.debounce_ms = (uint8_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_app.debounce_spin));

    /* Button Mappings */
    uint8_t btns[] = {0, 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    for (size_t b = 0; b < sizeof(btns); ++b) {
        uint8_t bid = btns[b];
        if (g_app.btn_combos[bid]) {
            int func = GPOINTER_TO_INT(g_object_get_data(
                G_OBJECT(g_app.btn_combos[bid]), "active_func_id"));
            g_app.profile.button_mappings[bid] = (uint8_t)func;

            if (g_app.btn_macro_slots[bid] && gtk_widget_is_visible(g_app.btn_macro_slots[bid])) {
                int slot = gtk_combo_box_get_active(GTK_COMBO_BOX(g_app.btn_macro_slots[bid]));
                g_app.profile.button_params[bid] = (uint8_t)(slot >= 0 ? slot : 0);
            } else {
                g_app.profile.button_params[bid] = 0;
            }
        }
    }

    g_thread_new("rapoo-apply", apply_thread_func, NULL);
}

typedef struct {
    uint8_t battery;
    bool charging;
    uint16_t dpi;
    bool online;
} TelemetryResult;

static gboolean on_telemetry_done(gpointer user_data) {
    TelemetryResult *res = (TelemetryResult *)user_data;
    if (res->online) {
        char bat_str[64];
        snprintf(bat_str, sizeof(bat_str), "%s %u%% (%s)",
                 res->charging ? "⚡ 충전 중" : "🔋 배터리",
                 res->battery,
                 res->charging ? "Charging" : "무선");
        gtk_label_set_text(GTK_LABEL(g_app.bat_badge), bat_str);
        GtkStyleContext *ctx = gtk_widget_get_style_context(g_app.bat_badge);
        gtk_style_context_remove_class(ctx, "badge-yellow");
        gtk_style_context_add_class(ctx, "badge-green");
    } else {
        gtk_label_set_text(GTK_LABEL(g_app.bat_badge), "💤 대기/절전 상태 (움직이면 활성화)");
        GtkStyleContext *ctx = gtk_widget_get_style_context(g_app.bat_badge);
        gtk_style_context_remove_class(ctx, "badge-green");
        gtk_style_context_add_class(ctx, "badge-yellow");
    }
    free(res);
    return G_SOURCE_REMOVE;
}

static gpointer telemetry_thread_func(gpointer user_data) {
    (void)user_data;
    TelemetryResult *res = (TelemetryResult *)malloc(sizeof(TelemetryResult));
    memset(res, 0, sizeof(*res));

    rapoo_device_t dev;
    if (rapoo_open(&dev, NULL) == 0) {
        rapoo_telemetry_t telem;
        if (rapoo_get_telemetry(&dev, &telem, 1.2) == 0) {
            res->battery = telem.battery_percent;
            res->charging = telem.is_charging;
            res->dpi = telem.current_dpi;
            res->online = true;
        }
        rapoo_close(&dev);
    }

    g_idle_add(on_telemetry_done, res);
    return NULL;
}

static void refresh_telemetry(void) {
    g_thread_new("rapoo-telemetry", telemetry_thread_func, NULL);
}

static void on_preset_clicked(GtkButton *btn, gpointer user_data) {
    int target_dpi = GPOINTER_TO_INT(user_data);
    (void)btn;
    uint8_t active = g_app.profile.active_dpi_stage;
    if (active < 7 && g_app.dpi_stage_spins[active]) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_app.dpi_stage_spins[active]), target_dpi);
    }
    if (g_app.dpi_spin_active) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_app.dpi_spin_active), target_dpi);
    }
}

static void on_stage_spin_changed(GtkSpinButton *spin, gpointer user_data) {
    int idx = GPOINTER_TO_INT(user_data);
    if ((uint8_t)idx == g_app.profile.active_dpi_stage && g_app.dpi_spin_active) {
        int val = gtk_spin_button_get_value_as_int(spin);
        if (gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_app.dpi_spin_active)) != val) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_app.dpi_spin_active), val);
        }
    }
}

static void on_active_radio_toggled(GtkToggleButton *toggle, gpointer user_data) {
    if (gtk_toggle_button_get_active(toggle)) {
        int idx = GPOINTER_TO_INT(user_data);
        g_app.profile.active_dpi_stage = (uint8_t)idx;
        if (idx < 7 && g_app.dpi_stage_spins[idx]) {
            int val = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_app.dpi_stage_spins[idx]));
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_app.dpi_spin_active), val);
        }
    }
}

static void on_active_dpi_spin_changed(GtkSpinButton *spin, gpointer user_data) {
    (void)user_data;
    int val = gtk_spin_button_get_value_as_int(spin);
    uint8_t active = g_app.profile.active_dpi_stage;
    if (active < 7 && g_app.dpi_stage_spins[active]) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_app.dpi_stage_spins[active]), val);
    }
}

static void on_stage_count_changed(GtkComboBox *combo, gpointer user_data) {
    (void)user_data;
    int count = gtk_combo_box_get_active(combo) + 1;
    for (int i = 0; i < 7; ++i) {
        bool visible = (i < count);
        gtk_widget_set_sensitive(g_app.dpi_stage_spins[i], visible);
        gtk_widget_set_sensitive(g_app.dpi_stage_radios[i], visible);
    }
    if (g_app.profile.active_dpi_stage >= count) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_app.dpi_stage_radios[0]), TRUE);
    }
}

typedef struct {
    uint8_t slot;
    char keystr[256];
    int delay_ms;
} MacroUploadParams;

static gpointer macro_flash_thread_func(gpointer user_data) {
    MacroUploadParams *params = (MacroUploadParams *)user_data;
    AsyncResult *res = (AsyncResult *)malloc(sizeof(AsyncResult));
    memset(res, 0, sizeof(*res));

    uint8_t bytecode[512];
    size_t out_len = 0;
    if (rapoo_encode_macro_string(params->keystr, params->delay_ms, 1, 1, bytecode, sizeof(bytecode), &out_len) != 0) {
        snprintf(res->message, sizeof(res->message), "❌ 매크로 문자열 인코딩 실패 ('%s')", params->keystr);
        res->success = false;
    } else {
        rapoo_device_t dev;
        if (rapoo_open(&dev, NULL) != 0) {
            snprintf(res->message, sizeof(res->message), "❌ 마우스 노드를 열 수 없습니다.");
            res->success = false;
        } else {
            if (rapoo_upload_macro(&dev, params->slot, bytecode, out_len) == 0) {
                snprintf(res->message, sizeof(res->message),
                         "✓ 하드웨어 매크로 슬롯 %u에 성공적으로 플래싱되었습니다! (%zu 바이트)",
                         params->slot, out_len);
                res->success = true;
            } else {
                snprintf(res->message, sizeof(res->message), "❌ 매크로 업로드 실패");
                res->success = false;
            }
            rapoo_close(&dev);
        }
    }

    free(params);
    g_idle_add(on_async_done, res);
    return NULL;
}

static void on_macro_flash_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    if (g_app.is_busy) return;
    g_app.is_busy = true;

    gtk_widget_set_sensitive(g_app.macro_flash_btn, FALSE);
    gtk_button_set_label(GTK_BUTTON(g_app.macro_flash_btn), "플래시 메모리에 쓰는 중...");

    MacroUploadParams *params = (MacroUploadParams *)malloc(sizeof(MacroUploadParams));
    params->slot = (uint8_t)gtk_combo_box_get_active(GTK_COMBO_BOX(g_app.macro_slot_combo));
    const char *text = gtk_entry_get_text(GTK_ENTRY(g_app.macro_key_entry));
    snprintf(params->keystr, sizeof(params->keystr), "%s", text ? text : "p + enter");
    params->delay_ms = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_app.macro_delay_spin));

    g_thread_new("rapoo-macro-flash", macro_flash_thread_func, params);
}

static void on_btn_func_changed(GtkComboBox *combo, gpointer user_data) {
    int bid = GPOINTER_TO_INT(user_data);
    int active_idx = gtk_combo_box_get_active(combo);

    /* Function IDs corresponding to combo items */
    static const int funcs[] = {
        RAPOO_FUNC_LEFT, RAPOO_FUNC_RIGHT, RAPOO_FUNC_MIDDLE,
        RAPOO_FUNC_FORWARD, RAPOO_FUNC_BACK,
        RAPOO_FUNC_DPI_CYCLE, RAPOO_FUNC_DPI_PLUS, RAPOO_FUNC_DPI_MINUS,
        RAPOO_FUNC_SNIPER, RAPOO_FUNC_FIRE,
        RAPOO_FUNC_WHEEL_UP, RAPOO_FUNC_WHEEL_DOWN,
        RAPOO_FUNC_MACRO
    };

    if (active_idx >= 0 && (size_t)active_idx < sizeof(funcs)/sizeof(funcs[0])) {
        int fid = funcs[active_idx];
        g_object_set_data(G_OBJECT(combo), "active_func_id", GINT_TO_POINTER(fid));
        if (g_app.btn_macro_slots[bid]) {
            gtk_widget_set_visible(g_app.btn_macro_slots[bid], (fid == RAPOO_FUNC_MACRO));
        }
    }
}

/* GUI Construction Helpers */
static GtkWidget* wrap_in_scrolled_window(GtkWidget *child) {
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), child);
    return sw;
}

static GtkWidget* create_dpi_page(void) {
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 16);

    /* Left Column: Active DPI & Presets */
    GtkWidget *card1 = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(card1, TRUE);
    GtkWidget *card1_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(card1), card1_box);

    GtkWidget *lbl1 = gtk_label_new("🎯 현재 활성 DPI 단계 (Active Stage)");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl1), "title");
    gtk_box_pack_start(GTK_BOX(card1_box), lbl1, FALSE, FALSE, 0);

    GtkWidget *spin_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    g_app.dpi_spin_active = gtk_spin_button_new_with_range(50, 26000, 50);
    g_app.dpi_scale_active = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 50, 26000, 50);
    gtk_widget_set_hexpand(g_app.dpi_scale_active, TRUE);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_app.dpi_spin_active),
                              g_app.profile.dpi_stages[g_app.profile.active_dpi_stage]);
    gtk_range_set_value(GTK_RANGE(g_app.dpi_scale_active),
                        g_app.profile.dpi_stages[g_app.profile.active_dpi_stage]);

    /* Sync spin and scale */
    g_signal_connect(g_app.dpi_spin_active, "value-changed", G_CALLBACK(on_active_dpi_spin_changed), NULL);
    g_object_bind_property(g_app.dpi_spin_active, "value", g_app.dpi_scale_active, "value",
                           G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

    gtk_box_pack_start(GTK_BOX(spin_box), g_app.dpi_scale_active, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(spin_box), g_app.dpi_spin_active, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card1_box), spin_box, FALSE, FALSE, 0);

    /* Quick Preset Grid (2x4) */
    GtkWidget *p_lbl = gtk_label_new("원클릭 DPI 프리셋 (Presets):");
    gtk_widget_set_halign(p_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(card1_box), p_lbl, FALSE, FALSE, 4);

    GtkWidget *preset_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(preset_grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(preset_grid), 8);
    int presets[] = {400, 800, 1200, 1600, 2000, 2400, 3200, 6400};
    for (size_t i = 0; i < sizeof(presets)/sizeof(presets[0]); ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d DPI", presets[i]);
        GtkWidget *pbtn = gtk_button_new_with_label(buf);
        gtk_widget_set_hexpand(pbtn, TRUE);
        g_signal_connect(pbtn, "clicked", G_CALLBACK(on_preset_clicked), GINT_TO_POINTER(presets[i]));
        gtk_grid_attach(GTK_GRID(preset_grid), pbtn, (int)(i % 4), (int)(i / 4), 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(card1_box), preset_grid, FALSE, FALSE, 4);

    GtkWidget *info_dpi = gtk_label_new(
        "💡 팁: '사용할 단계 수'를 1단계로 설정하면 상단 DPI 버튼을 눌러도\n"
        "속도가 바뀌지 않고 원하는 감도로 고정됩니다.");
    gtk_widget_set_halign(info_dpi, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(info_dpi), "subtitle");
    gtk_box_pack_start(GTK_BOX(card1_box), info_dpi, FALSE, FALSE, 8);

    gtk_box_pack_start(GTK_BOX(hbox), card1, TRUE, TRUE, 0);

    /* Right Column: Multi-stage Card */
    GtkWidget *card2 = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(card2, TRUE);
    GtkWidget *card2_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(card2), card2_box);

    GtkWidget *lbl2_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *lbl2 = gtk_label_new("📊 DPI 단계별 상세 설정 (1 ~ 7 Stages)");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl2), "title");
    gtk_box_pack_start(GTK_BOX(lbl2_box), lbl2, FALSE, FALSE, 0);

    GtkWidget *lbl_count = gtk_label_new("사용할 단계 수:");
    g_app.dpi_stage_count_combo = gtk_combo_box_text_new();
    for (int i = 1; i <= 7; ++i) {
        char s[16]; snprintf(s, sizeof(s), "%d단계", i);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_app.dpi_stage_count_combo), s);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_app.dpi_stage_count_combo), g_app.profile.stage_count - 1);
    g_signal_connect(g_app.dpi_stage_count_combo, "changed", G_CALLBACK(on_stage_count_changed), NULL);

    gtk_box_pack_end(GTK_BOX(lbl2_box), g_app.dpi_stage_count_combo, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(lbl2_box), lbl_count, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card2_box), lbl2_box, FALSE, FALSE, 0);

    GSList *group = NULL;
    GtkWidget *stages_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(stages_grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(stages_grid), 6);

    for (int i = 0; i < 7; ++i) {
        char rlabel[32];
        snprintf(rlabel, sizeof(rlabel), "%d단계 (Stage %d)", i + 1, i + 1);
        g_app.dpi_stage_radios[i] = gtk_radio_button_new_with_label(group, rlabel);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(g_app.dpi_stage_radios[i]));

        if (i == g_app.profile.active_dpi_stage) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_app.dpi_stage_radios[i]), TRUE);
        }
        g_signal_connect(g_app.dpi_stage_radios[i], "toggled", G_CALLBACK(on_active_radio_toggled), GINT_TO_POINTER(i));

        g_app.dpi_stage_spins[i] = gtk_spin_button_new_with_range(50, 26000, 50);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_app.dpi_stage_spins[i]), g_app.profile.dpi_stages[i]);
        g_signal_connect(g_app.dpi_stage_spins[i], "value-changed", G_CALLBACK(on_stage_spin_changed), GINT_TO_POINTER(i));

        gtk_grid_attach(GTK_GRID(stages_grid), g_app.dpi_stage_radios[i], 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(stages_grid), g_app.dpi_stage_spins[i], 1, i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(card2_box), stages_grid, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), card2, TRUE, TRUE, 0);

    on_stage_count_changed(GTK_COMBO_BOX(g_app.dpi_stage_count_combo), NULL);
    return wrap_in_scrolled_window(hbox);
}

static GtkWidget* create_sensor_page(void) {
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 16);

    /* Left Column: Polling Rate Card */
    GtkWidget *card1 = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(card1, TRUE);
    GtkWidget *card1_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(card1), card1_box);

    GtkWidget *lbl1 = gtk_label_new("⚡ USB / 2.4G 무선 폴링레이트");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl1), "title");
    gtk_box_pack_start(GTK_BOX(card1_box), lbl1, FALSE, FALSE, 0);

    g_app.polling_combo = gtk_combo_box_text_new();
    const char *rates[] = {
        "125 Hz (8ms - 절전 모드)",
        "250 Hz (4ms)",
        "500 Hz (2ms)",
        "1000 Hz (1ms - 표준 게이밍 권장)",
        "2000 Hz (0.5ms - 고속 리스폰스)",
        "4000 Hz (0.25ms - e스포츠 4K 수신기)",
        "8000 Hz (0.125ms - 초고속 플래그십)"
    };
    int active_idx = 3; /* default 1000 */
    for (int i = 0; i < 7; ++i) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_app.polling_combo), rates[i]);
    }
    if (g_app.profile.polling_rate_hz == 125) active_idx = 0;
    else if (g_app.profile.polling_rate_hz == 250) active_idx = 1;
    else if (g_app.profile.polling_rate_hz == 500) active_idx = 2;
    else if (g_app.profile.polling_rate_hz == 1000) active_idx = 3;
    else if (g_app.profile.polling_rate_hz == 2000) active_idx = 4;
    else if (g_app.profile.polling_rate_hz == 4000) active_idx = 5;
    else if (g_app.profile.polling_rate_hz == 8000) active_idx = 6;
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_app.polling_combo), active_idx);

    gtk_box_pack_start(GTK_BOX(card1_box), g_app.polling_combo, FALSE, FALSE, 0);

    /* Motion Sync inside Left Card */
    GtkWidget *ms_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *ms_lbl = gtk_label_new("모션 싱크 (Motion Sync):");
    g_app.motionsync_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(g_app.motionsync_switch), TRUE);
    gtk_box_pack_start(GTK_BOX(ms_box), ms_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ms_box), g_app.motionsync_switch, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card1_box), ms_box, FALSE, FALSE, 6);

    GtkWidget *rate_desc = gtk_label_new(
        "• 1000 Hz: 가장 안정적이며 모든 게임 권장 사양\n"
        "• 4K/8K: 고성능 CPU 및 전용 수신기 필요\n"
        "• 모션 싱크: PC 폴링과 센서 캡처를 1:1 동기화");
    gtk_widget_set_halign(rate_desc, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(rate_desc), "subtitle");
    gtk_box_pack_start(GTK_BOX(card1_box), rate_desc, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(hbox), card1, TRUE, TRUE, 0);

    /* Right Column: Sensor Parameters Card */
    GtkWidget *card2 = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(card2, TRUE);
    GtkWidget *card2_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(card2), card2_box);

    GtkWidget *lbl2 = gtk_label_new("🔍 광학 센서 및 클릭 파라미터");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl2), "title");
    gtk_box_pack_start(GTK_BOX(card2_box), lbl2, FALSE, FALSE, 0);

    /* LOD */
    GtkWidget *lod_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lod_lbl = gtk_label_new("센서 리프트오프 거리 (LOD, 기본 1.1mm):");
    gtk_widget_set_halign(lod_lbl, GTK_ALIGN_START);
    g_app.lod_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.7, 1.7, 0.1);
    gtk_scale_set_digits(GTK_SCALE(g_app.lod_scale), 1);
    gtk_widget_set_hexpand(g_app.lod_scale, TRUE);
    gtk_range_set_value(GTK_RANGE(g_app.lod_scale), g_app.profile.lod_mm > 0 ? g_app.profile.lod_mm : 1.1);

    gtk_box_pack_start(GTK_BOX(lod_box), lod_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(lod_box), g_app.lod_scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card2_box), lod_box, FALSE, FALSE, 0);

    /* Debounce */
    GtkWidget *deb_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *deb_lbl = gtk_label_new("스위치 디바운스 지연 (Debounce, 기본 2ms):");
    gtk_widget_set_halign(deb_lbl, GTK_ALIGN_START);
    GtkWidget *deb_inner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    g_app.debounce_spin = gtk_spin_button_new_with_range(1, 10, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_app.debounce_spin),
                              g_app.profile.debounce_ms > 0 ? g_app.profile.debounce_ms : 2);
    GtkWidget *deb_unit = gtk_label_new("ms (1~2ms 초저지연 클릭, 3~5ms 더블클릭 방지)");
    gtk_box_pack_start(GTK_BOX(deb_inner), g_app.debounce_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deb_inner), deb_unit, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deb_box), deb_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deb_box), deb_inner, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card2_box), deb_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), card2, TRUE, TRUE, 0);
    return wrap_in_scrolled_window(hbox);
}

static void on_macro_preset_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    const char *text = (const char *)user_data;
    if (g_app.macro_key_entry && text) {
        gtk_entry_set_text(GTK_ENTRY(g_app.macro_key_entry), text);
    }
}

static GtkWidget* create_buttons_page(void) {
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 16);

    /* Left Column: Buttons Remapping Card */
    GtkWidget *card1 = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(card1, TRUE);
    GtkWidget *card1_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(card1), card1_box);

    GtkWidget *lbl1 = gtk_label_new("🖱️ 물리 버튼 리매핑 (Hardware Buttons)");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl1), "title");
    gtk_box_pack_start(GTK_BOX(card1_box), lbl1, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);

    uint8_t btns[] = {0, 1, 2, 5, 6, 7, 8, 11, 12, 13};
    const char *func_labels[] = {
        "좌클릭 (Left Click)",
        "우클릭 (Right Click)",
        "휠 클릭 (Middle Click)",
        "앞으로 가기 (Forward)",
        "뒤로 가기 (Back)",
        "DPI 순환 (DPI Cycle)",
        "DPI+ (DPI Increase)",
        "DPI- (DPI Decrease)",
        "스나이퍼 모드 (Sniper Mode)",
        "연사 모드 (Fire / Rapid)",
        "휠 위로 (Wheel Up)",
        "휠 아래로 (Wheel Down)",
        "온보드 매크로 (Hardware Macro)"
    };
    int func_ids[] = {
        RAPOO_FUNC_LEFT, RAPOO_FUNC_RIGHT, RAPOO_FUNC_MIDDLE,
        RAPOO_FUNC_FORWARD, RAPOO_FUNC_BACK,
        RAPOO_FUNC_DPI_CYCLE, RAPOO_FUNC_DPI_PLUS, RAPOO_FUNC_DPI_MINUS,
        RAPOO_FUNC_SNIPER, RAPOO_FUNC_FIRE,
        RAPOO_FUNC_WHEEL_UP, RAPOO_FUNC_WHEEL_DOWN,
        RAPOO_FUNC_MACRO
    };

    for (size_t b = 0; b < sizeof(btns); ++b) {
        uint8_t bid = btns[b];
        GtkWidget *blbl = gtk_label_new(rapoo_get_button_name(bid));
        gtk_widget_set_halign(blbl, GTK_ALIGN_START);

        GtkWidget *combo = gtk_combo_box_text_new();
        g_app.btn_combos[bid] = combo;

        int active_fidx = 0;
        uint8_t current_fid = g_app.profile.button_mappings[bid];
        for (size_t f = 0; f < sizeof(func_ids)/sizeof(func_ids[0]); ++f) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), func_labels[f]);
            if (func_ids[f] == current_fid) active_fidx = (int)f;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active_fidx);
        g_object_set_data(G_OBJECT(combo), "active_func_id", GINT_TO_POINTER(current_fid));

        /* Macro Slot Selector (Visible only when Macro is chosen) */
        GtkWidget *slot_combo = gtk_combo_box_text_new();
        g_app.btn_macro_slots[bid] = slot_combo;
        for (int s = 0; s <= 5; ++s) {
            char sl[32]; snprintf(sl, sizeof(sl), "슬롯 %d (Slot %d)", s, s);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(slot_combo), sl);
        }
        int cur_param = g_app.profile.button_params[bid];
        gtk_combo_box_set_active(GTK_COMBO_BOX(slot_combo), (cur_param <= 5) ? cur_param : 0);
        gtk_widget_set_visible(slot_combo, (current_fid == RAPOO_FUNC_MACRO || current_fid == 29));

        g_signal_connect(combo, "changed", G_CALLBACK(on_btn_func_changed), GINT_TO_POINTER(bid));

        gtk_grid_attach(GTK_GRID(grid), blbl, 0, (int)b, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), combo, 1, (int)b, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), slot_combo, 2, (int)b, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(card1_box), grid, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), card1, TRUE, TRUE, 0);

    /* Right Column: Macro Studio Card */
    GtkWidget *card2 = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(card2, TRUE);
    GtkWidget *card2_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(card2), card2_box);

    GtkWidget *lbl2 = gtk_label_new("⚡ 온보드 하드웨어 매크로 플래셔");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl2), "title");
    gtk_box_pack_start(GTK_BOX(card2_box), lbl2, FALSE, FALSE, 0);

    GtkWidget *macro_desc = gtk_label_new(
        "소프트웨어 없이 마우스 내부 플래시 메모리(0x1000)에서\n"
        "즉각 실행되는 순수 온보드 하드웨어 매크로를 작성합니다.\n"
        "버튼을 '온보드 매크로'로 지정하고 해당 슬롯을 선택하세요.");
    gtk_widget_set_halign(macro_desc, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(macro_desc), "subtitle");
    gtk_box_pack_start(GTK_BOX(card2_box), macro_desc, FALSE, FALSE, 0);

    GtkWidget *mgrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(mgrid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(mgrid), 8);

    GtkWidget *sl_lbl = gtk_label_new("업로드할 슬롯:");
    gtk_widget_set_halign(sl_lbl, GTK_ALIGN_START);
    g_app.macro_slot_combo = gtk_combo_box_text_new();
    for (int s = 0; s <= 5; ++s) {
        char sl[32]; snprintf(sl, sizeof(sl), "슬롯 %d (0x%04x)", s, 0x1000 + (s * 0x1000));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_app.macro_slot_combo), sl);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_app.macro_slot_combo), 0);
    gtk_widget_set_hexpand(g_app.macro_slot_combo, TRUE);

    GtkWidget *ke_lbl = gtk_label_new("키 입력 시퀀스:");
    gtk_widget_set_halign(ke_lbl, GTK_ALIGN_START);
    g_app.macro_key_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(g_app.macro_key_entry), "p + enter");
    gtk_widget_set_hexpand(g_app.macro_key_entry, TRUE);

    GtkWidget *de_lbl = gtk_label_new("키 입력 딜레이:");
    gtk_widget_set_halign(de_lbl, GTK_ALIGN_START);
    GtkWidget *de_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    g_app.macro_delay_spin = gtk_spin_button_new_with_range(5, 500, 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_app.macro_delay_spin), 50);
    GtkWidget *ms_lbl = gtk_label_new("ms");
    gtk_box_pack_start(GTK_BOX(de_box), g_app.macro_delay_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(de_box), ms_lbl, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(mgrid), sl_lbl, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(mgrid), g_app.macro_slot_combo, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(mgrid), ke_lbl, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(mgrid), g_app.macro_key_entry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(mgrid), de_lbl, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(mgrid), de_box, 1, 2, 1, 1);

    gtk_box_pack_start(GTK_BOX(card2_box), mgrid, FALSE, FALSE, 4);

    /* Quick Preset Macros */
    GtkWidget *pre_lbl = gtk_label_new("간편 매크로 템플릿 (Quick Templates):");
    gtk_widget_set_halign(pre_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(card2_box), pre_lbl, FALSE, FALSE, 2);

    GtkWidget *p_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(p_grid), 6);
    gtk_grid_set_row_spacing(GTK_GRID(p_grid), 6);

    const char *tpls[] = {
        "p + enter", "ctrl + c", "ctrl + v",
        "space + c", "shift + w", "1 + 2"
    };
    const char *tpl_names[] = {
        "채팅 (P+Enter)", "복사 (Ctrl+C)", "붙여넣기 (Ctrl+V)",
        "점프샷 (Space+C)", "달리기 (Shift+W)", "무기교체 (1+2)"
    };
    for (size_t t = 0; t < sizeof(tpls)/sizeof(tpls[0]); ++t) {
        GtkWidget *tbtn = gtk_button_new_with_label(tpl_names[t]);
        gtk_widget_set_hexpand(tbtn, TRUE);
        g_signal_connect(tbtn, "clicked", G_CALLBACK(on_macro_preset_clicked), (gpointer)tpls[t]);
        gtk_grid_attach(GTK_GRID(p_grid), tbtn, (int)(t % 3), (int)(t / 3), 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(card2_box), p_grid, FALSE, FALSE, 4);

    g_app.macro_flash_btn = gtk_button_new_with_label("마우스 플래시 메모리에 업로드 (Flash)");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.macro_flash_btn), "suggested-action");
    gtk_widget_set_margin_top(g_app.macro_flash_btn, 8);
    g_signal_connect(g_app.macro_flash_btn, "clicked", G_CALLBACK(on_macro_flash_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(card2_box), g_app.macro_flash_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), card2, TRUE, TRUE, 0);

    return wrap_in_scrolled_window(hbox);
}

static GtkWidget* create_info_page(void) {
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 16);

    /* Left Column: Hardware Specifications */
    GtkWidget *card1 = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(card1, TRUE);
    GtkWidget *card1_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(card1), card1_box);

    GtkWidget *lbl1 = gtk_label_new("ℹ️ 디바이스 하드웨어 사양 (Hardware Specs)");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl1), "title");
    gtk_box_pack_start(GTK_BOX(card1_box), lbl1, FALSE, FALSE, 0);

    GtkWidget *grid1 = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid1), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid1), 10);

    const char *hw_keys[] = {
        "디바이스 모델",
        "USB 식별자 (VID:PID)",
        "커널 인터페이스 노드",
        "광학 센서 엔진",
        "최대 추적 스펙",
        "온보드 메모리"
    };
    const char *hw_vals[] = {
        "Rapoo VT9PRO / VT3PRO MAX / VT1PRO MAX",
        "VID 0x24ae : PID 0x1244",
        "/dev/hidraw* (Vendor Interface 0xba)",
        "PixArt PAW3398 / PAW3395",
        "최대 26,000 DPI / 650 IPS / 50G",
        "EEPROM & Flash Memory (0x1000)"
    };

    for (size_t i = 0; i < sizeof(hw_keys)/sizeof(hw_keys[0]); ++i) {
        GtkWidget *k = gtk_label_new(hw_keys[i]);
        gtk_widget_set_halign(k, GTK_ALIGN_START);
        GtkWidget *v = gtk_label_new(hw_vals[i]);
        gtk_widget_set_halign(v, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(v), "badge-blue");

        gtk_grid_attach(GTK_GRID(grid1), k, 0, (int)i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid1), v, 1, (int)i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(card1_box), grid1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), card1, TRUE, TRUE, 0);

    /* Right Column: Driver & System Environment */
    GtkWidget *card2 = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(card2, TRUE);
    GtkWidget *card2_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(card2), card2_box);

    GtkWidget *lbl2 = gtk_label_new("⚙️ 드라이버 & 시스템 환경 (Environment)");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl2), "title");
    gtk_box_pack_start(GTK_BOX(card2_box), lbl2, FALSE, FALSE, 0);

    GtkWidget *grid2 = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid2), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid2), 10);

    const char *sw_keys[] = {
        "드라이버 아키텍처",
        "통신 프로토콜",
        "백그라운드 데몬",
        "프로필 설정 파일",
        "배터리 텔레메트리",
        "운영체제 환경"
    };
    const char *sw_vals[] = {
        "100% Native C99 / GTK3 (Zero Python)",
        "0xba Report ID / 0xa1 Opcode Flash",
        "systemd (rapoo-mouse.service)",
        "~/.config/rapoo-mouse/active_profile.json",
        "30초 주기 자동 폴링 및 실시간 알림",
        "Linux Generic (Kernel 5.x / 6.x+)"
    };

    for (size_t i = 0; i < sizeof(sw_keys)/sizeof(sw_keys[0]); ++i) {
        GtkWidget *k = gtk_label_new(sw_keys[i]);
        gtk_widget_set_halign(k, GTK_ALIGN_START);
        GtkWidget *v = gtk_label_new(sw_vals[i]);
        gtk_widget_set_halign(v, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(v), "badge-blue");

        gtk_grid_attach(GTK_GRID(grid2), k, 0, (int)i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid2), v, 1, (int)i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(card2_box), grid2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), card2, TRUE, TRUE, 0);

    return wrap_in_scrolled_window(hbox);
}

static gboolean on_periodic_telemetry_tick(gpointer user_data) {
    (void)user_data;
    refresh_telemetry();
    return G_SOURCE_CONTINUE;
}

static void get_window_state_path(char *out, size_t maxlen) {
    snprintf(out, maxlen, "%s/rapoo-mouse/window_state.json", g_get_user_config_dir());
}

static void load_window_geometry(int *w, int *h) {
    *w = 1365;
    *h = 792;
    char path[512];
    get_window_state_path(path, sizeof(path));
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    char buf[256];
    int parsed_w = 0, parsed_h = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        if (strstr(buf, "\"width\"")) {
            char *colon = strchr(buf, ':');
            if (colon) parsed_w = atoi(colon + 1);
        } else if (strstr(buf, "\"height\"")) {
            char *colon = strchr(buf, ':');
            if (colon) parsed_h = atoi(colon + 1);
        }
    }
    fclose(fp);
    if (parsed_w >= 400 && parsed_h >= 300) {
        *w = parsed_w;
        *h = parsed_h;
    }
}

static void save_window_geometry(int w, int h) {
    char path[512];
    get_window_state_path(path, sizeof(path));
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    fprintf(fp, "{\n  \"width\": %d,\n  \"height\": %d\n}\n", w, h);
    fclose(fp);
}

static gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)event; (void)user_data;
    int w = 0, h = 0;
    gtk_window_get_size(GTK_WINDOW(widget), &w, &h);
    if (w >= 400 && h >= 300) {
        save_window_geometry(w, h);
    }
    return FALSE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    /* Load initial profile */
    rapoo_load_profile(NULL, &g_app.profile);

    int win_w = 1365;
    int win_h = 792;
    load_window_geometry(&win_w, &win_h);

    g_app.window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(g_app.window), "라푸 마우스 제어 센터 (Rapoo Mouse Control)");
    gtk_window_set_default_size(GTK_WINDOW(g_app.window), win_w, win_h);
    g_signal_connect(g_app.window, "delete-event", G_CALLBACK(on_window_delete_event), NULL);

    /* CSS Styling */
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, DARK_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    /* HeaderBar */
    g_app.header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(g_app.header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(g_app.header_bar), "라푸 마우스 제어 센터");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(g_app.header_bar), "VT9PRO / VT3PRO MAX (C/C++ Native)");

    /* Battery & Status Badge */
    g_app.bat_badge = gtk_label_new("🔋 배터리 확인 중...");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.bat_badge), "badge-yellow");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(g_app.header_bar), g_app.bat_badge);

    GtkWidget *refresh_btn = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(refresh_btn, "배터리 상태 새로고침");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(refresh_telemetry), NULL);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(g_app.header_bar), refresh_btn);

    /* Apply Button */
    g_app.apply_btn = gtk_button_new_with_label("마우스에 즉시 적용 (Apply)");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.apply_btn), "suggested-action");
    g_signal_connect(g_app.apply_btn, "clicked", G_CALLBACK(on_apply_clicked), NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(g_app.header_bar), g_app.apply_btn);

    gtk_window_set_titlebar(GTK_WINDOW(g_app.window), g_app.header_bar);

    /* Main Box */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(g_app.window), main_box);

    /* Notification InfoBar */
    g_app.info_bar = gtk_info_bar_new();
    g_app.info_label = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(gtk_info_bar_get_content_area(GTK_INFO_BAR(g_app.info_bar))), g_app.info_label);
    gtk_info_bar_set_show_close_button(GTK_INFO_BAR(g_app.info_bar), TRUE);
    g_signal_connect(g_app.info_bar, "response", G_CALLBACK(gtk_widget_hide), NULL);
    gtk_box_pack_start(GTK_BOX(main_box), g_app.info_bar, FALSE, FALSE, 0);

    /* Notebook Tabs */
    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_dpi_page(), gtk_label_new("🎯 DPI 설정"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_sensor_page(), gtk_label_new("⚡ 센서 & 폴링"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_buttons_page(), gtk_label_new("🖱️ 버튼 & 매크로"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_info_page(), gtk_label_new("ℹ️ 디바이스 정보"));

    gtk_box_pack_start(GTK_BOX(main_box), notebook, TRUE, TRUE, 0);

    gtk_widget_show_all(g_app.window);
    gtk_widget_hide(g_app.info_bar);

    /* Trigger battery telemetry */
    refresh_telemetry();

    /* Auto-refresh battery telemetry every 30 seconds */
    g_timeout_add_seconds(30, on_periodic_telemetry_tick, NULL);
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("io.github.rapoo_linux.control", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
