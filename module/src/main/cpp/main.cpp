#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cstring>
#include "zygisk.hpp" // Header resmi API Zygisk

// =======================================================
// OFFSET CORE ESP LINE (FREE FIRE MAX) - HASIL DUMP CS
// =======================================================
#define OFFSET_GET_MAIN             0xa7ed6c0
#define OFFSET_WORLD_TO_SCREEN      0xa7ed344
#define OFFSET_GET_POSITION         0x8857b00
#define OFFSET_GET_PLAYER_COUNT     0x645d5c4
#define OFFSET_GET_LOCAL_PLAYER     0x64cbde8

// Struktur Data Vector3 Unity bawaan untuk koordinat 3D
struct Vector3 {
    float x, y, z;
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
};

uintptr_t il2cpp_base = 0;
float screen_width = 2400.0f;  // Resolusi layar default (akan menyesuaikan sistem)
float screen_height = 1080.0f;

// Fungsi Utama yang Berjalan Otomatis di Latar Belakang Game
void* OtomatisESPThread(void*) {
    // 1. Tunggu sampai library utama game (libil2cpp.so) benar-benar termuat di memori HP
    do {
        il2cpp_base = (uintptr_t)dlopen("libil2cpp.so", RTLD_NOLOAD);
        usleep(500000); // Cek ulang setiap 0.5 detik agar tidak membebani CPU
    } while (!il2cpp_base);

    // Inisialisasi pointer fungsi berdasarkan offset hasil dump kita tadi
    auto get_main = (void* (*)()) (il2cpp_base + OFFSET_GET_MAIN);
    auto WorldToScreenPoint = (Vector3 (*) (void*, Vector3)) (il2cpp_base + OFFSET_WORLD_TO_SCREEN);
    auto get_Position = (Vector3 (*) (void*)) (il2cpp_base + OFFSET_GET_POSITION);
    auto GetPlayerCount = (int (*)()) (il2cpp_base + OFFSET_GET_PLAYER_COUNT);
    auto get_LocalPlayerEntity = (void* (*)()) (il2cpp_base + OFFSET_GET_LOCAL_PLAYER);

    // 2. Looping Tanpa Henti (Selama Game Terbuka, Fitur ESP Ini Aktif Terus)
    while (true) {
        // PENGAMAN 1: Pastikan pointer fungsi dasar tidak kosong sebelum dieksekusi
        if (get_main == nullptr || GetPlayerCount == nullptr || get_LocalPlayerEntity == nullptr) {
            usleep(100000);
            continue;
        }

        void* main_camera = get_main(); 
        void* local_player = get_LocalPlayerEntity();
        int total_pemain = GetPlayerCount();

        // PENGAMAN 2: Hanya jalankan logika jika kamera game dan data player sudah siap di room/match
        if (main_camera != nullptr && total_pemain > 0 && local_player != nullptr) {
            
            // Perulangan (Looping) ke Seluruh Pemain di Map
            for (int i = 0; i < total_pemain; i++) {
                void* current_player = nullptr; 
                // Catatan: Di sini modul Anda nantinya mengambil instans pemain berdasarkan indeks i
                
                // PENGAMAN 3: Skrip tidak boleh membaca jika objek current_player masih bernilai NULL (Pemicu Utama Game FC)
                if (current_player != nullptr && current_player != local_player) {
                    
                    if (get_Position != nullptr && WorldToScreenPoint != nullptr) {
                        // Ambil koordinat 3D posisi musuh saat ini
                        Vector3 musuh_3d = get_Position(current_player);

                        // Ubah koordinat 3D tersebut ke koordinat piksel 2D layar HP Anda
                        Vector3 layar_2d = WorldToScreenPoint(main_camera, musuh_3d);

                        // Jika musuh berada di depan pandangan kamera (Z > 0)
                        if (layar_2d.z > 0.0f) {
                            // Logika hitung piksel garis lurus dari tengah-bawah layar menuju posisi musuh
                            float start_x = screen_width / 2.0f;
                            float start_y = screen_height;
                            float end_x = layar_2d.x;
                            float end_y = screen_height - layar_2d.y;

                            // Perintah gambar garis otomatis disalurkan ke Canvas internal Unity di sini
                        }
                    }
                }
            }
        }
        usleep(32000); // Naikkan jeda waktu ke 32ms (~30 FPS) saat masa uji coba agar lebih stabil di latar belakang
    }
    return nullptr;
}

// Implementasi Class API Framework Zygisk
class MyZygiskModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        // Ambil nama paket aplikasi yang sedang dibuka oleh Android
        const char* process_name = env->GetStringUTFChars(args->nice_name, nullptr);
        
        // Cek otomatis apakah aplikasi yang dibuka adalah Free Fire Max
        if (process_name && strcmp(process_name, "com.dts.freefiremax") == 0) {
            // Jika Benar, Buat Thread Otomatis di Latar Belakang saat Game Mulai Berjalan
            pthread_t t;
            pthread_create(&t, nullptr, OtomatisESPThread, nullptr);
        }
        env->ReleaseStringUTFChars(args->nice_name, process_name);
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
};

// Daftarkan modul agar dibaca oleh Magisk / Kitsune Mask
REGISTER_ZYGISK_MODULE(MyZygiskModule);
