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
// Prompt yazdırma fonksiyonu
// Bu fonksiyon, kabuk arayüzünde kullanıcıya yeni bir komut girmesi için prompt yazdırır
void print_prompt() {
    if (background_process_count == 0) { // Eğer arka plan işlemi yoksa prompt yazdır
        printf("> "); // Prompt karakterini yaz
        fflush(stdout); // Çıktıyı hemen ekrana aktar
        ++promptSayi; // Prompt sayaç değerini artır
    }
}


// Giriş ve çıkış yönlendirmesini analiz eden yardımcı fonksiyon
// Bu fonksiyon, bir komutun giriş ("<") ve çıkış (">") yönlendirmelerini işler
// Eğer yönlendirme işaretleri bulunursa, ilgili dosya adlarını input_file ve output_file işaretçilerine kaydeder
void handle_redirection(char *cmd, char **input_file, char **output_file) {
    char *input_redirect = strstr(cmd, "<"); // Komutta giriş yönlendirmesini ara
    char *output_redirect = strstr(cmd, ">"); // Komutta çıkış yönlendirmesini ara

    if (input_redirect != NULL) { // Eğer giriş yönlendirmesi varsa
        *input_redirect = '\0'; // '<' karakterini komuttan ayır
        input_redirect++; // '<' işaretinden sonrasını al
        *input_file = strtok(input_redirect, " "); // İlk boşlukta keserek dosya adını al
    }

    if (output_redirect != NULL) { // Eğer çıkış yönlendirmesi varsa
        *output_redirect = '\0'; // '>' karakterini komuttan ayır
        output_redirect++; // '>' işaretinden sonrasını al
        *output_file = strtok(output_redirect, " "); // İlk boşlukta keserek dosya adını al
    }
}

// "increment" komutunu işleyen fonksiyon
// Borudan gelen bir tam sayıyı okur, değeri 1 artırır ve sonucu standart çıktıya yazdırır
void handle_increment() {
    char buffer[16]; // Okuma için bir tampon oluştur
    int bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1); // Standart girişten veriyi oku

    if (bytes_read <= 0) { // Okuma başarısız olduysa
        printf("Borudan okuma basarisiz oldu veya giris bos."); // Hata mesajı yazdır
        exit(1); // Programı hata koduyla sonlandır
    }

    buffer[bytes_read] = '\0'; // Tamponun sonuna null karakter ekle
    int value = atoi(buffer); // Tampondaki değeri tam sayıya dönüştür
    printf("%d\n", value + 1); // Değeri 1 artır ve sonucu yazdır
    fflush(stdout); // Çıktıyı hemen ekrana aktar
}


// Komutları borularla çalıştırma fonksiyonu
// Bu fonksiyon, verilen komutları bir dizi boru (pipe) kullanarak çalıştırır.
// Her komut bir alt süreçte çalıştırılır ve borular ile diğer komutlara veri aktarılır.
void execute_pipe_commands(char *commands[], int num_commands) {
    int pipe_fds[2 * (num_commands - 1)]; // Gerekli boru dosya tanımlayıcılarını saklamak için bir dizi
    int saved_stdout = dup(STDOUT_FILENO); // stdout'u kaydet, böylece fonksiyon sonunda eski haline döndürülebilir

    // Borular oluştur
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipe_fds + i * 2) == -1) { // Boruları oluştur
            printf("Boru olusturma basarisiz oldu"); // Hata mesajı
            exit(1); // Programı sonlandır
        }
    }
