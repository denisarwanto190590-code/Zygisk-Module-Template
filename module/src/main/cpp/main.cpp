#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cstring>
#include <string.h>
#include <stdlib.h>
#include <android/log.h>
#include "zygisk.hpp"
#include "dobby.h" // Wajib menyertakan Dobby Hook untuk kestabilan injector

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

// OFFSET ENGINE GRAFIS INTERNAL UNITY (Gunakan versi Unity FF Max saat ini)
// Seringkali ditemukan di libunity.so atau libil2cpp.so tergantung dump versi gamenya
#define OFFSET_UNITY_ONGUI              0x1234567 // GANTI: Isi dengan offset UnityEngine.GUI.OnGUI atau sejenisnya jika diperlukan, atau hook internal function

struct Vector3 {
    float x, y, z;
};

// Struktur warna untuk Unity GUI
struct Color {
    float r, g, b, a;
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

// Pointer Fungsi Gambar Unity Internal
void (*UnityEngine_GL_Color)(Color) = nullptr;
void (*UnityEngine_GL_Vertex3)(float, float, float) = nullptr;
void (*UnityEngine_GL_Begin)(int) = nullptr;
void (*UnityEngine_GL_End)() = nullptr;
void* (*UnityEngine_Material_SetPass)(void*, int) = nullptr;
void* draw_material = nullptr; // Material kosmetik dasar untuk menggambar garis

// Pointer original untuk menyimpan fungsi sebelum di-hook
void (*orig_Unity_RenderPipeline)(void* instance, void* context, void* cameras) = nullptr;

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

// Fungsi internal untuk menggambar garis 2D memanfaatkan material dasar Unity
void GambarGarisDirect(float x1, float y1, float x2, float y2, Color warna) {
    if (!UnityEngine_GL_Color || !UnityEngine_GL_Vertex3 || !UnityEngine_GL_Begin || !UnityEngine_GL_End) return;

    // Aktifkan pass material (jika ada) agar warna garis muncul utuh di layar
    if (UnityEngine_Material_SetPass && draw_material) {
        UnityEngine_Material_SetPass(draw_material, 0);
    }

    // Mode 1 = GL.LINES (Menggambar garis antar dua titik masukan)
    UnityEngine_GL_Begin(1); 
    UnityEngine_GL_Color(warna);
    
    // Titik Awal (Bawah Tengah Layar)
    UnityEngine_GL_Vertex3(x1 / screen_width, 1.0f - (y1 / screen_height), 0.0f);
    // Titik Akhir (Posisi Layar Musuh)
    UnityEngine_GL_Vertex3(x2 / screen_width, 1.0f - (y2 / screen_height), 0.0f);
    
    UnityEngine_GL_End();
}

// KHUSUS MAIN THREAD: Fungsi ini berjalan aman di dalam rantai rendering game
void LoopGrafisUtama() {
    if (!get_main || !WorldToScreenPoint || !get_Position || !GetPlayerCount || !get_LocalPlayerEntity || !GetPlayerByIndex) return;

    void* main_camera = get_main();
    if (main_camera == nullptr) return;

    void* local_player = get_LocalPlayerEntity();
    int total_pemain = GetPlayerCount();
    
    if (total_pemain <= 0 || total_pemain > 100) return;

    // Definisikan warna garis (Merah solid: R=1, G=0, B=0, Alpha=1)
    Color warna_musuh = {1.0f, 0.0f, 0.0f, 1.0f};

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

                // EKSEKUSI: Menggambar langsung di thread utama game, dijamin anti-FC!
                GambarGarisDirect(start_x, start_y, end_x, end_y, warna_musuh);
            }
        }
    }
}

// HOOK RENDER PIPELINE: Terpicu otomatis setiap kali game menggambar frame baru di layar
void hook_Unity_RenderPipeline(void* instance, void* context, void* cameras) {
    // 1. Biarkan game menggambar grafik aslinya terlebih dahulu agar tidak glitch
    if (orig_Unity_RenderPipeline) {
        orig_Unity_RenderPipeline(instance, context, cameras);
    }

    // 2. Tempelkan fungsi ESP kita tepat di atas gambar game yang sudah jadi
    LoopGrafisUtama();
}

void* LoopInjeksiMemori(void*) {
    do {
        il2cpp_base = dapatkan_base_memori();
        usleep(500000); 
    } while (!il2cpp_base);

    // Inisialisasi Alamat Fungsi Posisi Pemain
    get_main = (void* (*)()) (il2cpp_base + OFFSET_GET_MAIN);
    WorldToScreenPoint = (Vector3 (*) (void*, Vector3)) (il2cpp_base + OFFSET_WORLD_TO_SCREEN);
    get_Position = (Vector3 (*) (void*)) (il2cpp_base + OFFSET_GET_POSITION);
    GetPlayerCount = (int (*)()) (il2cpp_base + OFFSET_GET_PLAYER_COUNT);
    get_LocalPlayerEntity = (void* (*)()) (il2cpp_base + OFFSET_GET_LOCAL_PLAYER);
    GetPlayerByIndex = (void* (*)(int)) (il2cpp_base + OFFSET_GET_PLAYER_BY_INDEX);
    is_Dead = (bool (*)(void*)) (il2cpp_base + OFFSET_IS_DEAD);

    // Inisialisasi Alamat Grafis internal Unity (Dapatkan dari dump il2cpp kelas UnityEngine.GL)
    // Catatan: Anda perlu mengisi offset fungsi GL ini dari hasil dump il2cpp dumper Anda
    UnityEngine_GL_Color = (void (*)(Color)) (il2cpp_base + 0xIL2CPP_GL_COLOR_OFFSET);
    UnityEngine_GL_Vertex3 = (void (*)(float, float, float)) (il2cpp_base + 0xIL2CPP_GL_VERTEX3_OFFSET);
    UnityEngine_GL_Begin = (void (*)(int)) (il2cpp_base + 0xIL2CPP_GL_BEGIN_OFFSET);
    UnityEngine_GL_End = (void (*)()) (il2cpp_base + 0xIL2CPP_GL_END_OFFSET);

    // ANTI-FC SYSTEM: Mencegat fungsi render kamera utama menggunakan Dobby Hook [1]
    // Cari offset 'UnityEngine.Rendering.RenderPipelineManager:DoRenderLoop_Internal' di dump Anda
    uintptr_t render_loop_offset = il2cpp_base + 0xOFFSET_RENDER_LOOP_INTERNAL; 
    
    DobbyHook((void*)render_loop_offset, (void*)hook_Unity_RenderPipeline, (void**)&orig_Unity_RenderPipeline);

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
            pthread_create(&t, nullptr, LoopInjeksiMemori, nullptr);
        }
        env->ReleaseStringUTFChars(args->nice_name, process_name);
    }
private:
    zygisk::Api* api;
    JNIEnv* env;
};

REGISTER_ZYGISK_MODULE(MyZygiskModule);
