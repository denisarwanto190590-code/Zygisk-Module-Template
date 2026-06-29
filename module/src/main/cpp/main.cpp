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
#define OFFSET_GET_PLAYER_LIST_TEAM     0x645d00c 

#define OFFSET_GUI_DRAW_TEXTURE         0x1234567 

struct Vector3 {
    float x, y, z;
};

struct Rect {
    float x, y, width, height;
    Rect(float x, float y, float w, float h) : x(x), y(y), width(w), height(h) {}
};

struct MonoArray {
    void* klass;
    void* monitor;
    void* bounds;
    int max_length;
    void* vector; 
};

uintptr_t il2cpp_base = 0;
float screen_width = 2400.0f;  
float screen_height = 1080.0f; 

void* (*get_main)() = nullptr;
Vector3 (*WorldToScreenPoint)(void*, Vector3) = nullptr;
Vector3 (*get_Position)(void*) = nullptr;
int (*GetPlayerCount)() = nullptr;
void* (*get_LocalPlayerEntity)() = nullptr;
void* (*GetPlayerListFromTeamId)(uint8_t team) = nullptr;
void (*DrawTexture)(Rect, void*, int, bool) = nullptr;

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

void EksekusiESPLine() {
    if (!get_main || !GetPlayerCount || !get_LocalPlayerEntity || !GetPlayerListFromTeamId) return;

    void* main_camera = get_main();
    if (main_camera == nullptr) return;

    void* local_player = get_LocalPlayerEntity();
    int total_pemain = GetPlayerCount();
    if (total_pemain <= 0) return;

    for (uint8_t team_id = 0; team_id <= 1; team_id++) {
        MonoArray* player_list = (MonoArray*)GetPlayerListFromTeamId(team_id);
        
        if (player_list != nullptr && player_list->max_length > 0) {
            for (int i = 0; i < player_list->max_length; i++) {
                void* current_player = ((void**)player_list->vector)[i]; 

                if (current_player != nullptr) {
                    if (current_player == local_player) {
                        continue; 
                    }

                    Vector3 musuh_3d = get_Position(current_player);
                    Vector3 layar_2d = WorldToScreenPoint(main_camera, musuh_3d);

                    if (layar_2d.z > 0.0f) {
                        float start_x = screen_width / 2.0f;
                        float start_y = screen_height;
                        float end_x = layar_2d.x;
                        float end_y = screen_height - layar_2d.y;

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
