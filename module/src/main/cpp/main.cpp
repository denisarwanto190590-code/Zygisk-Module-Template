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
// DAFTAR OFFSET MATANG ESP LINE (FREE FIRE MAX) - UPDATE TERBARU
// ====================================================================

// STEP 1: KAMERA & PROYEKSI LAYAR (MENGUBAH 3D KE 2D LAYAR)
#define OFFSET_GET_MAIN                 0xa7ed6c0
#define OFFSET_WORLD_TO_SCREEN          0xa7ed344

// STEP 2: MANAJEMEN PEMAIN & BATAS PERULANGAN
#define OFFSET_GET_PLAYER_COUNT         0x645d5c4
#define OFFSET_GET_LOCAL_PLAYER         0x64cbde8
#define OFFSET_GET_PLAYER_BY_INDEX      0x7d3fb8c

// STEP 3: DATA INTERNAL UNTUK POSISI DAN VALIDASI NYAWA MUSUH
#define OFFSET_GET_POSITION             0x8857b00
#define OFFSET_IS_DEAD                  0x76611dc

struct Vector3 {
    float x, y, z;
};

uintptr_t il2cpp_base = 0;
float screen_width = 2400.0f;  
float screen_height = 1080.0f; 

// Deklarasi fungsi penunjuk sesuai offset baru
void* (*get_main)() = nullptr;
Vector3 (*WorldToScreenPoint)(void*, Vector3) = nullptr;
Vector3 (*get_Position)(void*) = nullptr;
int (*GetPlayerCount)() = nullptr;
void* (*get_LocalPlayerEntity)() = nullptr;
void* (*GetPlayerByIndex)(int index) = nullptr; // Fungsi baru menggantikan fungsi Team
bool (*is_Dead)(void*) = nullptr;               // Fungsi baru untuk cek status hidup/mati

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

void GambarGarisESP(float x1, float y1, float x2, float y2) {
    // Jalur data koordinat untuk ditarik oleh Canvas Overlay / ImGui Anda
    __android_log_print(ANDROID_LOG_INFO, "ZygiskESP", "Garis ke musuh: Dari (%.1f, %.1f) menuju (%.1f, %.1f)", x1, y1, x2, y2);
}

void EksekusiESPLine() {
    // Validasi fungsi utama agar tidak crash jika libil2cpp belum siap sepenuhnya
    if (!get_main || !WorldToScreenPoint || !get_Position || !GetPlayerCount || !get_LocalPlayerEntity || !GetPlayerByIndex) return;

    void* main_camera = get_main();
    if (main_camera == nullptr) return;

    void* local_player = get_LocalPlayerEntity();
    int total_pemain = GetPlayerCount();
    
    // Batasi perulangan maksimal agar aman dari freeze/lag jika data memory bermasalah
    if (total_pemain <= 0 || total_pemain > 100) return;

    // Perulangan langsung menggunakan Index Pemain (Jauh lebih aman dari FC)
    for (int i = 0; i < total_pemain; i++) {
        void* current_player = GetPlayerByIndex(i);

        // Validasi dasar pointer pemain
        if (current_player != nullptr && current_player != local_player && !((uintptr_t)current_player & 1)) {
            
            // OPTIONAL: Validasi apakah musuh masih hidup (Fitur dari offset 0x76611dc Anda)
            if (is_Dead != nullptr && is_Dead(current_player)) {
                continue; // Jika mati, lewati pemain ini dan lanjut ke indeks berikutnya
            }

            Vector3 musuh_3d = get_Position(current_player);
            Vector3 layar_2d = WorldToScreenPoint(main_camera, musuh_3d);

            if (layar_2d.z > 0.0f) {
                float start_x = screen_width / 2.0f;
                float start_y = screen_height;
                float end_x = layar_2d.x;
                float end_y = screen_height - layar_2d.y;

                // Salurkan koordinat matang ke fungsi gambar
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

    // Inisialisasi alamat memori berdasarkan daftar offset baru Anda
    get_main = (void* (*)()) (il2cpp_base + OFFSET_GET_MAIN);
    WorldToScreenPoint = (Vector3 (*) (void*, Vector3)) (il2cpp_base + OFFSET_WORLD_TO_SCREEN);
    get_Position = (Vector3 (*) (void*)) (il2cpp_base + OFFSET_GET_POSITION);
    GetPlayerCount = (int (*)()) (il2cpp_base + OFFSET_GET_PLAYER_COUNT);
    get_LocalPlayerEntity = (void* (*)()) (il2cpp_base + OFFSET_GET_LOCAL_PLAYER);
    GetPlayerByIndex = (void* (*)(int)) (il2cpp_base + OFFSET_GET_PLAYER_BY_INDEX);
    is_Dead = (bool (*)(void*)) (il2cpp_base + OFFSET_IS_DEAD);

    while (true) {
        EksekusiESPLine();
        usleep(33000); // 30 FPS
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
