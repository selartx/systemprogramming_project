#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define MAX_FILES 32
#define MAX_TOTAL_SIZE (200 * 1024 * 1024) // 200 MB

//  yalnızca ascii karakter 
int is_ascii_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c > 127) { // 1 byte ascii aralığı 
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return 1;
}

// -b: dosya arşivleme 
void archive_files(int file_count, char **files, const char *archive_name) {
    long total_size = 0;
    struct stat file_stats[MAX_FILES];
    
//limit format
        if (file_count > MAX_FILES) {
        printf("Hata: En fazla %d dosya girilebilir!\n", MAX_FILES);
        exit(1);
    }

    for (int i = 0; i < file_count; i++) {
        if (!is_ascii_file(files[i])) {
            printf("%s giriş dosyasının formatı uyumsuzdur!\n", files[i]);
            exit(1);
        }
        if (stat(files[i], &file_stats[i]) < 0) {
            printf("Hata: %s dosyası okunamıyor!\n", files[i]);
            exit(1);
        }
        total_size += file_stats[i].st_size;
    }

    if (total_size > MAX_TOTAL_SIZE) {
        printf("Hata: Giriş dosyalarının toplam boyutu 200 MB'ı geçemez!\n");
        exit(1);
    }

    // header 
    char header_content[4096] = {0};
    for (int i = 0; i < file_count; i++) {
        char record[256];
        sprintf(record, "|%s,%o,%ld|", files[i], file_stats[i].st_mode & 0777, file_stats[i].st_size);
        strcat(header_content, record);
    }

    int header_length = strlen(header_content);
    
    // arşive yaz
    FILE *out = fopen(archive_name, "wb");
    if (!out) {
        printf("Arşiv dosyası oluşturulamadı!\n");
        exit(1);
    }

    fprintf(out, "%010d", header_length);
    fwrite(header_content, 1, header_length, out);

    for (int i = 0; i < file_count; i++) {
        FILE *in = fopen(files[i], "rb");
        if (in) {
            char buffer[4096];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
                fwrite(buffer, 1, bytes_read, out);
            }
            fclose(in);
        }
    }
    
    fclose(out);
    printf("%s arşivi başarıyla oluşturuldu.\n", archive_name);
}

// -a: arşivden çıkarma 
void extract_archive(const char *archive_name, const char *target_dir) {
    FILE *in = fopen(archive_name, "rb");
    if (!in) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        exit(1);
    }

    char size_str[11] = {0};
    if (fread(size_str, 1, 10, in) != 10) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(in);
        exit(1);
    }

    int header_length = atoi(size_str);
    if (header_length <= 0) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(in);
        exit(1);
    }

    char *header = (char *)malloc(header_length + 1);
    if (fread(header, 1, header_length, in) != (size_t)header_length) {         
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        free(header);
        fclose(in);
        exit(1);
    }
    header[header_length] = '\0';

    if (target_dir) {
        struct stat st = {0};
        if (stat(target_dir, &st) == -1) {
            mkdir(target_dir, 0700);
        }
        chdir(target_dir);
    }

    char *token = strtok(header, "|");
    while (token != NULL) {
        char filename[256];
        int perms;
        long size;

        if (sscanf(token, "%[^,],%o,%ld", filename, &perms, &size) == 3) {
            FILE *out = fopen(filename, "wb");
            if (out) {
                char buffer[4096];
                long bytes_left = size;
                
                while (bytes_left > 0) {
                    size_t to_read = (size_t)bytes_left < sizeof(buffer) ? (size_t)bytes_left : sizeof(buffer);
                    size_t read_bytes = fread(buffer, 1, to_read, in);
                    if (read_bytes == 0) break;
                    fwrite(buffer, 1, read_bytes, out);
                    bytes_left -= read_bytes;
                }
                fclose(out);
                chmod(filename, perms);
            }
        }
        token = strtok(NULL, "|");
    }

    free(header);
    fclose(in);
    printf("Dosyalar başarıyla çıkarıldı.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Kullanım:\n");
        printf("Arşivleme: %s -b dosya1.txt dosya2.txt -o arsiv.sau\n", argv[0]);
        printf("Çıkarma: %s -a arsiv.sau [hedef_dizin]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        char *files[MAX_FILES];
        int file_count = 0;
        char *archive_name = "a.sau"; // Varsayılan

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                archive_name = argv[i + 1];
                i++;
            } else {
                if (file_count < MAX_FILES) {
                    files[file_count++] = argv[i];
                }
            }
        }
        archive_files(file_count, files, archive_name);
    } 
    else if (strcmp(argv[1], "-a") == 0) {
        char *archive_name = argv[2];
        // .sau uzantısı kontrolü
        if (!strstr(archive_name, ".sau")) {
             printf("Arşiv dosyası uygunsuz veya bozuk!\n");
             return 1;
        }
        char *target_dir = (argc > 3) ? argv[3] : NULL;
        extract_archive(archive_name, target_dir);
    } 
    else {
        printf("Geçersiz parametre!\n");
    }

    return 0;
}