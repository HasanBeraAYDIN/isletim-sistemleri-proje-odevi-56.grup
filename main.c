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

    // Komutları alt süreçlerde çalıştır
    for (int i = 0; i < num_commands; i++) {
        pid_t pid = fork(); // Yeni bir süreç oluştur
        if (pid == -1) { // Fork başarısız olduysa
            printf("Fork basarisiz oldu"); // Hata mesajı
            exit(1); // Programı sonlandır
        } else if (pid == 0) { // Çocuk süreci
            // Giriş bağlantısını ayarla
            if (i > 0) { // İlk komut dışındaki komutlar için
                if (dup2(pipe_fds[(i - 1) * 2], STDIN_FILENO) == -1) { // Giriş borusunu bağla
                    printf("dup2 giris basarisiz oldu"); // Hata mesajı
                    exit(1); // Programı sonlandır
                }
            }
            // Çıkış bağlantısını ayarla
            if (i < num_commands - 1) { // Son komut dışındaki komutlar için
                if (dup2(pipe_fds[i * 2 + 1], STDOUT_FILENO) == -1) { // Çıkış borusunu bağla
                    printf("dup2 cikis basarisiz oldu"); // Hata mesajı
                    exit(1); // Programı sonlandır
                }
            }

            // Tüm boru dosya tanımlayıcılarını kapat
            for (int j = 0; j < 2 * (num_commands - 1); j++) {
                close(pipe_fds[j]);
            }

            // Komutun argümanlarını ayrıştır
            char *args[MAX_ARGS];
            char *token = strtok(commands[i], " "); // Komutu boşluklara göre böl
            int arg_count = 0;

            while (token != NULL) { // Tüm argümanları al
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_count] = NULL; // Argümanların sonuna NULL ekle

            // Eğer komut "increment" ise özel işlem
            if (strncmp(args[0], "increment", 9) == 0) {
                handle_increment(); // increment işlemini gerçekleştir
                exit(0); // Çocuk süreci sonlandır
            }

            // Komutu çalıştır
            if (execvp(args[0], args) == -1) { // Komut çalıştırma başarısız olduysa
                printf("Exec basarisiz oldu"); // Hata mesajı
                exit(1); // Programı sonlandır
            }
        }
    }

    // Ana süreçte tüm boru dosya tanımlayıcılarını kapat
    for (int i = 0; i < 2 * (num_commands - 1); i++) {
        close(pipe_fds[i]);
    }

    // Tüm çocuk süreçlerini bekle
    for (int i = 0; i < num_commands; i++) {
        wait(NULL); // Çocuk süreçlerin tamamlanmasını bekle
    }

    // stdout'u eski haline döndür
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    // stdout'u temizle
    fflush(stdout); // Çıkışı hemen aktar
}

// Noktalı virgül ile ayrılmış komutları çalıştırma fonksiyonu
// Bu fonksiyon, birden fazla komutun noktalı virgül (;) ile ayrıldığı bir dizgeyi işler ve her bir komutu sırasıyla çalıştırır.
void execute_sequential_commands(char *cmd) {
    char *command = strtok(cmd, ";"); // İlk komutu al

    // Tüm komutları sırayla işle
    while (command != NULL) {
        execute_command(command); // Her bir komutu çalıştır
        command = strtok(NULL, ";"); // Bir sonraki komutu al
    }
}


// Tek bir komut veya karmaşık bir komut dizisini işleyen ana fonksiyon
void execute_command(char *cmd) {
    char *input_file = NULL; // Giriş dosyası için değişken
    char *output_file = NULL; // Çıkış dosyası için değişken

    // Giriş ve çıkış yönlendirmelerini kontrol et
    handle_redirection(cmd, &input_file, &output_file);

    // "increment" komutunu kontrol et ve çalıştır
    if (strncmp(cmd, "increment", 9) == 0) {
        int saved_stdin = dup(STDIN_FILENO); // Standart girişin yedeğini al
        if (input_file != NULL) {
            int fd = open(input_file, O_RDONLY); // Giriş dosyasını aç
            if (fd == -1) {
                printf("%s giris dosyasi bulunamadi", input_file);
                return;
            }
            if (dup2(fd, STDIN_FILENO) == -1) { // Standart girişi dosya ile değiştir
                printf("dup2 basarisiz oldu");
                close(fd);
                return;
            }
            handle_increment(); // "increment" işlemini gerçekleştir
            close(fd);
        } else {
            handle_increment(); // Giriş dosyası yoksa doğrudan çalıştır
        }
        dup2(saved_stdin, STDIN_FILENO); // Standart girişi eski haline döndür
        close(saved_stdin);
        return;
    }

    // Boru ile komutları kontrol et ve çalıştır
    if (strchr(cmd, '|') != NULL) {
        char *commands[MAX_PIPES];
        int num_commands = 0;

        char *command = strtok(cmd, "|"); // Komutları boruya göre ayır
        while (command != NULL) {
            commands[num_commands++] = command;
            command = strtok(NULL, "|");
        }
        execute_pipe_commands(commands, num_commands); // Boru komutlarını çalıştır
    }
    // Noktalı virgül ile ayrılmış komutları kontrol et ve çalıştır
    else if (strchr(cmd, ';') != NULL) {
        execute_sequential_commands(cmd); // Sıralı komutları çalıştır
    }
    // Tek komut veya arka plan komutlarını kontrol et ve çalıştır
    else {
        int is_background = 0; // Arka plan işlemi olup olmadığını kontrol et
        char *ampersand = strstr(cmd, "&");
        if (ampersand != NULL) {
            is_background = 1;
            *ampersand = '\0'; // "&" karakterini kaldır
        }

        pid_t pid = fork(); // Yeni süreç oluştur
        if (pid == -1) {
            printf("Fork basarisiz oldu");
            exit(1);
        } 
        // Çocuk süreç işlemleri
        else if (pid == 0) {
            if (input_file != NULL) {
                int fd = open(input_file, O_RDONLY); // Giriş dosyasını aç
                if (fd == -1) {
                    printf("%s giris dosyasi bulunamadi", input_file);
                    exit(1);
                }
                dup2(fd, STDIN_FILENO); // Standart girişi dosya ile değiştir
                close(fd);
            }

            if (output_file != NULL) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Çıkış dosyasını aç
                if (fd == -1) {
                    printf("Cikis dosyasi hatali");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO); // Standart çıkışı dosya ile değiştir
                close(fd);
            }

            // Komutun argümanlarını ayrıştır ve çalıştır
            char *args[MAX_CMD_LEN / 2 + 1];
            int i = 0;
            char *token = strtok(cmd, " ");
            while (token != NULL) {
                args[i] = token;
                i++;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;

            if (execvp(args[0], args) == -1) { // Komutu çalıştır
                printf("Exec basarisiz oldu");
                exit(1);
            }
        } 
        // Ana süreç işlemleri
        else {
            if (is_background) {
                // Arka plan işlemi ise listeye ekle
                background_process_count++;
                if (background_pids_count < MAX_BG_PROCESSES) {
                    background_pids[background_pids_count++] = pid;
                } else {
                    printf("Hata: Cok fazla arka plan islemi.\n");
                }
            } else {
                waitpid(pid, NULL, 0); // Ön plan işlemini bekle
                if (strncmp(cmd, "cat", 3) == 0 && output_file == NULL) {
                    printf("\n");
                    fflush(stdout);
                }
            }
        }
    }
}

