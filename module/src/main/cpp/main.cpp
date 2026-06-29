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

// Pointer Fungsi Game sesuai offset baru Anda
void* (*get_main)() = nullptr;
Vector3 (*WorldToScreenPoint)(void*, Vector3) = nullptr;
Vector3 (*get_Position)(void*) = nullptr;
int (*GetPlayerCount)() = nullptr;
void* (*get_LocalPlayerEntity)() = nullptr;
void* (*GetPlayerByIndex)(int index) = nullptr; 
bool (*is_Dead)(void*) = nullptr;               

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

// Fungsi utama penyalur data koordinat
void GambarGarisESP(float x1, float y1, float x2, float y2) {
    // Jalur data koordinat untuk dibaca oleh Canvas Overlay eksternal Anda
    __android_log_print(ANDROID_LOG_INFO, "ZygiskESP", "Garis ke musuh: Dari (%.1f, %.1f) menuju (%.1f, %.1f)", x1, y1, x2, y2);
}

void EksekusiESPLine() {
    // Proteksi utama: pastikan semua offset penting sudah termuat sempurna
    if (!get_main || !WorldToScreenPoint || !get_Position || !GetPlayerCount || !get_LocalPlayerEntity || !GetPlayerByIndex) return;

    void* main_camera = get_main();
    if (main_camera == nullptr) return;

    void* local_player = get_LocalPlayerEntity();
    int total_pemain = GetPlayerCount();
    
    // Pembatasan total loop agar memori tidak overload/freeze yang memicu FC
    if (total_pemain <= 0 || total_pemain > 100) return;

    for (int i = 0; i < total_pemain; i++) {
        void* current_player = GetPlayerByIndex(i);

        // Validasi pointer memori pemain (Anti-FC)
        if (current_player != nullptr && current_player != local_player && !((uintptr_t)current_player & 1)) {
            
            // Periksa status kematian musuh berdasarkan offset 0x76611dc Anda
            if (is_Dead != nullptr && is_Dead(current_player)) {
                continue; 
            }

            Vector3 musuh_3d = get_Position(current_player);
            Vector3 layar_2d = WorldToScreenPoint(main_camera, musuh_3d);

            // Z > 0 mengonfirmasi musuh berada di depan sudut pandang kamera pemain
            if (layar_2d.z > 0.0f) {
                float start_x = screen_width / 2.0f;
                float start_y = screen_height;
                float end_x = layar_2d.x;
                float end_y = screen_height - layar_2d.y;

                // Mengirimkan data koordinat matang ke Logcat/Overlay
                GambarGarisESP(start_x, start_y, end_x, end_y);
            }
        }
    }
}

void* LoopLatarBelakang(void*) {
    do {
        il2cpp_base = dapatkan_base_memori();
        usleep(500000); 
    } while (!il2cpp_base);

    // Pemetaan fungsi memori game
    get_main = (void* (*)()) (il2cpp_base + OFFSET_GET_MAIN);
    WorldToScreenPoint = (Vector3 (*) (void*, Vector3)) (il2cpp_base + OFFSET_WORLD_TO_SCREEN);
    get_Position = (Vector3 (*) (void*)) (il2cpp_base + OFFSET_GET_POSITION);
    GetPlayerCount = (int (*)()) (il2cpp_base + OFFSET_GET_PLAYER_COUNT);
    get_LocalPlayerEntity = (void* (*)()) (il2cpp_base + OFFSET_GET_LOCAL_PLAYER);
    GetPlayerByIndex = (void* (*)(int)) (il2cpp_base + OFFSET_GET_PLAYER_BY_INDEX);
    is_Dead = (bool (*)(void*)) (il2cpp_base + OFFSET_IS_DEAD);

    while (true) {
        EksekusiESPLine();
        usleep(33000); // Berjalan stabil di kisaran 30 FPS untuk menghindari bentrokan thread game (Anti-FC)
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
            pthread_t t;
            pthread_create(&t, nullptr, LoopLatarBelakang, nullptr);
        }
        env->ReleaseStringUTFChars(args->nice_name, process_name);
    }
private:
    zygisk::Api* api;
    JNIEnv* env;
};

REGISTER_ZYGISK_MODULE(MyZygiskModule);
