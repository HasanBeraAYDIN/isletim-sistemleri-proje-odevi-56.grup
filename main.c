/*
    HAZIRLAYANLAR
    ----------------
    Can Örge    G221210028  2-A
    Ömer Keleş  B221210022  1-A
    Hasan Bera Aydın B221210010 1-A
    Yusuf Kaçmaz    G221210007  2-A    
    Alper Bora      B221210307  1-A  
*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_CMD_LEN 1024 // Maksimum komut uzunluğu
#define MAX_ARGS 100     // Maksimum argüman sayısı
#define MAX_PIPES 10     // Maksimum pipe sayısı

#define MAX_BG_PROCESSES 100 // Maksimum arka plan süreci sayısı
pid_t background_pids[MAX_BG_PROCESSES]; // Arka plan işlemlerinin PID'lerini tutan dizi
int background_pids_count = 0; // Arka plan işlemlerinin sayısı
int promptSayi = 0; // Prompt'un yeniden basılması için sayaç

// Arka plan işlemleri için kullanılan global değişken
volatile sig_atomic_t background_process_count = 0; // Sinyal güvenli arka plan işlem sayacı

// Fonksiyon prototipleri
void execute_command(char *cmd); // Komutları çalıştıran fonksiyon
void print_prompt(); // Prompt'u yazdıran fonksiyon

// Arka plan işlemi tamamlandığında çağrılan sinyal işleyici
void handle_sigchld(int sig) {
    int status;
    pid_t pid;
    promptSayi = 0; // Prompt'un yazdırılma durumunu sıfırla

    // Tamamlanan tüm çocuk süreçleri kontrol et
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Sadece arka plan işlemlerini kontrol et
        for (int i = 0; i < background_pids_count; i++) {
            if (background_pids[i] == pid) { // PID eşleşmesi bulunduğunda
                if (WIFEXITED(status)) { // Süreç normal şekilde tamamlandıysa
                    printf("[%d] retval: %d\n", pid, WEXITSTATUS(status)); // Sürecin dönüş değerini yazdır
                }
                // PID'i listeden çıkar
                for (int j = i; j < background_pids_count - 1; j++) {
                    background_pids[j] = background_pids[j + 1];
                }
                background_pids_count--; // Arka plan süreç sayısını azalt
                background_process_count--; // Genel arka plan süreç sayısını azalt
                break;
            }
        }
        // Eğer arka plan işlemi kalmadıysa prompt'u yazdır
        if (background_pids_count == 0) {
            print_prompt();
        }
    }
}


// quit komutu ile kabuğu sonlandıracak fonksiyon
// Bu fonksiyon, önce tüm arka plan işlemlerinin tamamlanmasını bekler
// Ardından kabuğu güvenli bir şekilde sonlandırır
void quit_shell() {
    while (background_process_count > 0) { // Arka plan işlemleri devam ettiği sürece bekle
        pause(); // Sinyal gelene kadar duraklat
    }
    exit(0); // Kabuk programını sonlandır
}