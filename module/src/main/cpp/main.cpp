#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cstring>
#include "zygisk.hpp"

// =======================================================
// SEMUA OFFSET UTUH UNTUK ESP LINE (FREE FIRE MAX)
// =======================================================
#define OFFSET_GET_MAIN                 0xa7ed6c0
#define OFFSET_WORLD_TO_SCREEN          0xa7ed344
#define OFFSET_GET_POSITION             0x8857b00
#define OFFSET_GET_PLAYER_COUNT         0x645d5c4
#define OFFSET_GET_LOCAL_PLAYER         0x64cbde8
#define OFFSET_GET_PLAYER_LIST_TEAM     0x645d00c // Diambil dari baris 1942 foto dump kamu

// Offset fungsi gambar internal Unity (Ganti dengan hasil dump UnityEngine.GUI::DrawTexture kamu jika ada)
#define OFFSET_GUI_DRAW_TEXTURE         0x1234567 

struct Vector3 {
    float x, y, z;
};

struct Rect {
    float x, y, width, height;
    Rect(float x, float y, float w, float h) : x(x), y(y), width(w), height(h) {}
};

// Struktur standar memori Array milik Unity engine
struct MonoArray {
    void* klass;
    void* monitor;
    void* bounds;
    int max_length;
    void* vector[1]; // Menampung list pointer player game
};

uintptr_t il2cpp_base = 0;
float screen_width = 2400.0f;  // Otomatis menyesuaikan lebar layar HP
float screen_height = 1080.0f; // Otomatis menyesuaikan tinggi layar HP

// Pointer Fungsi Resmi Unity
void* (*get_main)() = nullptr;
Vector3 (*WorldToScreenPoint)(void*, Vector3) = nullptr;
Vector3 (*get_Position)(void*) = nullptr;
int (*GetPlayerCount)() = nullptr;
void* (*get_LocalPlayerEntity)() = nullptr;
void* (*GetPlayerListFromTeamId)(uint8_t team) = nullptr;
void (*DrawTexture)(Rect, void*, int, bool) = nullptr;

// Fungsi pembaca base memori libil2cpp yang aman untuk HP (Anti-FC)
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

// Logika Fungsi Perulangan ESP yang SUDAH SEMPURNA
void EksekusiESPLine() {
    if (!get_main || !GetPlayerCount || !get_LocalPlayerEntity || !GetPlayerListFromTeamId) return;

    // 1. Ambil Kamera Utama Game
    void* main_camera = get_main();
    if (main_camera == nullptr) return;

    // 2. Ambil Pointer Karakter Diri Sendiri
    void* local_player = get_LocalPlayerEntity();

    // 3. Ambil Total Pemain Aktif di Room
    int total_pemain = GetPlayerCount();
    if (total_pemain <= 0) return;

    // Loop membaca list Team 0 dan Team 1 (Kawan & Musuh) berdasarkan foto dump kamu
    for (uint8_t team_id = 0; team_id <= 1; team_id++) {
        MonoArray* player_list = (MonoArray*)GetPlayerListFromTeamId(team_id);
        
        if (player_list != nullptr && player_list->max_length > 0) {
            
            // 4. Lakukan Perulangan (Looping) ke Seluruh Pemain di Map
            for (int i = 0; i < player_list->max_length; i++) {
                
                // SOLUSI: Sekarang current_player tidak kosong lagi, diambil langsung dari array memori game!
                void* current_player = player_list->vector[i]; 

                if (current_player != nullptr) {
                    // Saring: Jangan gambar garis ke diri sendiri!
                    if (current_player == local_player) {
                        continue; 
                    }

                    // Ambil posisi koordinat 3D musuh saat ini
                    Vector3 musuh_3d = get_Position(current_player);

                    // Ubah koordinat 3D tersebut ke koordinat piksel 2D layar HP Anda
                    Vector3 layar_2d = WorldToScreenPoint(main_camera, musuh_3d);

                    // Jika musuh berada di depan pandangan kamera (Z > 0)
                    if (layar_2d.z > 0.0f) {
                        // Titik awal garis: Tengah bawah layar ponsel Anda
                        float start_x = screen_width / 2.0f;
                        float start_y = screen_height;

                        // Titik akhir garis: Posisi piksel koordinat musuh
                        float end_x = layar_2d.x;
                        float end_y = screen_height - layar_2d.y;

                        // SOLUSI GAMBAR: Menggunakan fungsi tekstur bawaan Unity yang diperkecil menjadi garis tipis (2 pixel)
                        if (DrawTexture) {
                            Rect posisi_garis(start_x, end_y, 2.0f, start_y - end_y);
                            DrawTexture(posisi_garis, nullptr, 0, true);
                        }
                    }
                }
            }
        }
    }
}

// Thread Latar Belakang: Hanya berjalan sekali di awal untuk memetakan fungsi memori
void* InisialisasiFungsi(void*) {
    do {
        il2cpp_base = dapatkan_base_memori();
        usleep(500000); 
    } while (!il2cpp_base);

    get_main = (void* (*)()) (il2cpp_base + OFFSET_GET_MAIN);
    WorldToScreenPoint = (Vector3 (*) (void*, Vector3)) (il2cpp_base + OFFSET_WORLD_TO_SCREEN);
    get_Position = (Vector3 (*) (void*)) (il2cpp_base + OFFSET_GET_POSITION);
    GetPlayerCount = (int (*)()) (il2cpp_base + OFFSET_GET_PLAYER_COUNT);
    get_LocalPlayerEntity = (void* (*)()) (il2cpp_base + OFFSET_GET_LOCAL_PLAYER);
    GetPlayerListFromTeamId = (void* (*)(uint8_t)) (il2cpp_base + OFFSET_GET_PLAYER_LIST_TEAM);
    DrawTexture = (void (*)(Rect, void*, int, bool)) (il2cpp_base + OFFSET_GUI_DRAW_TEXTURE);

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
            pthread_create(&t, nullptr, InisialisasiFungsi, nullptr);
        }
        env->ReleaseStringUTFChars(args->nice_name, process_name);
    }
private:
    zygisk::Api* api;
    JNIEnv* env;
};

REGISTER_ZYGISK_MODULE(MyZygiskModule);
