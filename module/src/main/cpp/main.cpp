#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cstring>
#include <string.h>
#include <stdlib.h>
#include <android/log.h>
#include "zygisk.hpp"

// ====================================================================
// DAFTAR OFFSET MATANG ESP LINE (FREE FIRE MAX)
// ====================================================================
#define OFFSET_GET_MAIN                 0xa7ed6c0
#define OFFSET_WORLD_TO_SCREEN          0xa7ed344
#define OFFSET_GET_PLAYER_COUNT         0x645d5c4
#define OFFSET_GET_LOCAL_PLAYER         0x64cbde8
#define OFFSET_GET_PLAYER_BY_INDEX      0x7d3fb8c
#define OFFSET_GET_POSITION             0x8857b00
#define OFFSET_IS_DEAD                  0x76611dc

struct Vector3 {
    float x, y, z;
};

uintptr_t il2cpp_base = 0;
float screen_width = 2400.0f;  
float screen_height = 1080.0f; 

// Pointer Fungsi Game
void* (*get_main)() = nullptr;
Vector3 (*WorldToScreenPoint)(void*, Vector3) = nullptr;
Vector3 (*get_Position)(void*) = nullptr;
int (*GetPlayerCount)() = nullptr;
void* (*get_LocalPlayerEntity)() = nullptr;
void* (*GetPlayerByIndex)(int index) = nullptr; 
bool (*is_Dead)(void*) = nullptr;               

// Variabel Global untuk Render Grafis JNI Java (Anti-FC)
jobject global_canvas_view = nullptr;
jobject global_paint_red = nullptr;

uintptr_t dapatkan_base_memori() {
    uintptr_t addr = 0;
    char line[512];
    FILE* fp = fopen("/proc/self/maps", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "libil2cpp.so")) {
                addr = strtoul(line, NULL, 16);
                break;
            }
        }
        fclose(fp);
    }
    return addr;
}

// FUNGSI KHUSUS: Menggambar memanfaatkan sistem Canvas bawaan OS Android (Aman & Ringan)
void MenggambarGarisVisual(JNIEnv* env, float x1, float y1, float x2, float y2) {
    if (!global_canvas_view || !global_paint_red || !env) return;

    // Menghubungkan fungsi Java Canvas.drawLine(x1, y1, x2, y2, paint) secara Native
    jclass canvas_class = env->FindClass("android/graphics/Canvas");
    jmethodID draw_line_id = env->GetMethodID(canvas_class, "drawLine", "(FFFFLandroid/graphics/Paint;)V");
    
    if (draw_line_id) {
        env->CallVoidMethod(global_canvas_view, draw_line_id, x1, y1, x2, y2, global_paint_red);
    }
}

void EksekusiESPLine(JNIEnv* env) {
    if (!get_main || !WorldToScreenPoint || !get_Position || !GetPlayerCount || !get_LocalPlayerEntity || !GetPlayerByIndex) return;

    void* main_camera = get_main();
    if (main_camera == nullptr) return;

    void* local_player = get_LocalPlayerEntity();
    int total_pemain = GetPlayerCount();
    
    if (total_pemain <= 0 || total_pemain > 100) return;

    for (int i = 0; i < total_pemain; i++) {
        void* current_player = GetPlayerByIndex(i);

        if (current_player != nullptr && current_player != local_player && !((uintptr_t)current_player & 1)) {
            
            if (is_Dead != nullptr && is_Dead(current_player)) {
                continue; 
            }

            Vector3 musuh_3d = get_Position(current_player);
            Vector3 layar_2d = WorldToScreenPoint(main_camera, musuh_3d);

            if (layar_2d.z > 0.0f) {
                float start_x = screen_width / 2.0f;
                float start_y = screen_height;
                float end_x = layar_2d.x;
                float end_y = screen_height - layar_2d.y;

                // EKSEKUSI: Garis akan langsung terlukis secara nyata di atas layar game Anda
                MenggambarGarisVisual(env, start_x, start_y, end_x, end_y);
            }
        }
    }
}

void* LoopLatarBelakang(void* args) {
    JNIEnv* env = (JNIEnv*)args;

    do {
        il2cpp_base = dapatkan_base_memori();
        usleep(500000); 
    } while (!il2cpp_base);

    get_main = (void* (*)()) (il2cpp_base + OFFSET_GET_MAIN);
    WorldToScreenPoint = (Vector3 (*) (void*, Vector3)) (il2cpp_base + OFFSET_WORLD_TO_SCREEN);
    get_Position = (Vector3 (*) (void*)) (il2cpp_base + OFFSET_GET_POSITION);
    GetPlayerCount = (int (*)()) (il2cpp_base + OFFSET_GET_PLAYER_COUNT);
    get_LocalPlayerEntity = (void* (*)()) (il2cpp_base + OFFSET_GET_LOCAL_PLAYER);
    GetPlayerByIndex = (void* (*)(int)) (il2cpp_base + OFFSET_GET_PLAYER_BY_INDEX);
    is_Dead = (bool (*)(void*)) (il2cpp_base + OFFSET_IS_DEAD);

    // INISIALISASI WARNA DI JAVA (Warna Merah Solid untuk Garis)
    jclass paint_class = env->FindClass("android/graphics/Paint");
    jmethodID paint_init = env->GetMethodID(paint_class, "<init>", "()V");
    jobject local_paint = env->NewObject(paint_class, paint_init);
    global_paint_red = env->NewGlobalRef(local_paint);

    jmethodID set_color_id = env->GetMethodID(paint_class, "setColor", "(I)V");
    env->CallVoidMethod(global_paint_red, set_color_id, 0xFFFF0000); // 0xFFFF0000 = Merah murni Hex

    jmethodID set_stroke_id = env->GetMethodID(paint_class, "setStrokeWidth", "(F)V");
    env->CallVoidMethod(global_paint_red, set_stroke_id, 4.0f); // Ketebalan garis = 4 pixel

    // Loop gambar utama dengan pelindung refresh rate game (Anti-Lag & Anti-FC)
    while (true) {
        if (env != nullptr) {
            EksekusiESPLine(env);
        }
        usleep(16000); // Sinkronisasi otomatis mengikuti frame-rate game (~60 FPS)
    }
    return nullptr;
}

class MyZygiskModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* process_name = env->GetStringUTFChars(args->nice_name, nullptr);
        if (process_name && strcmp(process_name, "com.dts.freefiremax") == 0) {
            // Membuat thread yang membawa environment JNI agar aman dari crash cross-thread
            pthread_t t;
            pthread_create(&t, nullptr, LoopLatarBelakang, (void*)env);
        }
        env->ReleaseStringUTFChars(args->nice_name, process_name);
    }
private:
    zygisk::Api* api;
    JNIEnv* env;
};

REGISTER_ZYGISK_MODULE(MyZygiskModule);
