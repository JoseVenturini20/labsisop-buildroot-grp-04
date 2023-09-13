#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <time.h>

/*
dar ifconfig para escutar no endereço ip diferente pq está na localhost
Precisa escutar na interface
no caso da máquina target na interface
->eth0
ou trocar pra
-> 0.0.0.0 - broadcast
*/

#define BUFLEN 1024 // Max length of buffer
#define PORT 8000   // The port on which to listen for incoming data

void die(char *s)
{
    perror(s);
    exit(1);
}

void get_cpu_usage(double* usage) {
    long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    long long total_time, idle_time, total_time_diff, idle_time_diff;

    FILE* f = fopen("/proc/stat", "r");
    if (f == NULL) {
        *usage = -1.0;
        return;
    }

    fscanf(f, "cpu %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);
    fclose(f);

    total_time = user + nice + system + idle + iowait + irq + softirq + steal;
    idle_time = idle + iowait;

    static long long prev_total_time = 0;
    static long long prev_idle_time = 0;

    total_time_diff = total_time - prev_total_time;
    idle_time_diff = idle_time - prev_idle_time;

    *usage = 100.0 * (1.0 - ((double)idle_time_diff / (double)total_time_diff));

    prev_total_time = total_time;
    prev_idle_time = idle_time;
}


int main(void)
{
    struct sockaddr_in si_me, si_other;
    int s, i, slen = sizeof(si_other), conn, child = 0;
    char buf[BUFLEN];
    pid_t pid;
    ssize_t recv_len;

    /* create a TCP socket */
    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        die("socket");

    /* zero out the structure */
    memset((char *)&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    /* bind socket to port */
    if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
        die("bind");

    /* allow 10 requests to queue up */
    if (listen(s, 10) == -1)
        die("listen");

    /* keep listening for data */
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        printf("Waiting a connection...");
        fflush(stdout);
        conn = accept(s, (struct sockaddr *)&si_other, &slen);
        if (conn < 0)
            die("accept");

        if ((pid = fork()) < 0)
            die("fork");
        else if (pid == 0)
        {
            close(s);
            printf("Client connected: %s:%d\n", inet_ntoa(si_other.sin_addr),
                   ntohs(si_other.sin_port));

            /* Try to receive some data, this is a blocking call */
            recv_len = read(conn, buf, BUFLEN);
            if (recv_len < 0)
                die("read");

            /* Print details of the client/peer and the data received */
            printf("Data: %s\n", buf);

            if (strstr(buf, "GET"))
            {
                /* Now reply to the client with the same data */
                char http_ok[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\nServer: Test\r\n\r\n";
                if (write(conn, http_ok, strlen(http_ok)) < 0)
                    die("write");

                
                char html_response[BUFLEN];//Cria a 'string' da resposta html
                memset(html_response, 0, sizeof(html_response));
                strcat(html_response, "<html>\n<head>\n<title>System Information</title>\n</head>\n<body>\n");
                strcat(html_response, "<h1>System Information</h1>\n");

                //Tempo
                time_t agora;
                struct tm *infoTempo;

                time(&agora);
                infoTempo = localtime(&agora);

                //Data e hora
                int ano = infoTempo->tm_year + 1900;
                int mes = infoTempo->tm_mon + 1;
                int dia = infoTempo->tm_mday;
                int hora = infoTempo->tm_hour;
                int minuto = infoTempo->tm_min;
                int segundo = infoTempo->tm_sec;

                char time_string[BUFLEN];
                memset(time_string, 0, sizeof(time_string));
                snprintf(time_string, sizeof(time_string), "<p>Data e hora: %d-%02d-%02d %02d:%02d:%02d<p>\n", ano, mes, dia, hora, minuto, segundo);
                strcat(html_response, time_string);

                //Struct do sysinfo -> man sysinfo
                struct sysinfo info;
                if (sysinfo(&info) != 0)
                    die("sysinfo");

                //Uptime
                long int uptime = info.uptime;
                char uptime_string[BUFLEN];
                memset(uptime_string, 0, sizeof(uptime_string));
                snprintf(uptime_string, sizeof(uptime_string), "<p>Uptime: %ld seconds</p>\n", uptime);
                strcat(html_response, uptime_string);

                //Hostname e versao
                struct utsname uname_info;
                if (uname(&uname_info) != 0)
                    die("uname");

                char uname_string[BUFLEN];
                memset(uname_string, 0, sizeof(uname_string));
                snprintf(uname_string, sizeof(uname_string), "<p>System Version: %s</p>\n", uname_info.release);
                strcat(html_response, uname_string);
                
               
                //Memoria
                long total_memory = info.totalram / (1024 * 1024);//Multiplica pra virar MB
                long used_memory = (info.totalram - info.freeram) / (1024 * 1024);

                char memory_string[BUFLEN];
                memset(memory_string, 0, sizeof(memory_string));
                snprintf(memory_string, sizeof(memory_string), "<p>Used Memory: %ld MB</p>\n<p>Total Memory: %ld MB</p>\n", used_memory, total_memory);
                strcat(html_response, memory_string);


                //Processador 
                FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
                if (cpuinfo == NULL)
                    die("fopen");

                char cpu_model[256] = "";
                char line[256];
                while (fgets(line, sizeof(line), cpuinfo))
                {
                    if (strstr(line, "model name"))
                    {
                        char *colon = strchr(line, ':');
                        if (colon)
                        {
                            strncpy(cpu_model, colon + 2, sizeof(cpu_model) - 1);
                            break;
                        }
                    }
                }
                char cpu_model_string[BUFLEN];
                memset(cpu_model_string, 0, sizeof(cpu_model_string));
                snprintf(cpu_model_string, sizeof(cpu_model_string), "<p>Modelo do Processador: %s<p>\n", cpu_model);
                strcat(html_response, cpu_model_string);


                fclose(cpuinfo);

                double cpu_usage;
                get_cpu_usage(&cpu_usage);
                char cpu_usage_string[BUFLEN];
                memset(cpu_usage_string, 0, sizeof(cpu_usage_string));
                snprintf(cpu_usage_string, sizeof(cpu_usage_string), "<p>Capacidade ocupada do processador: %.2lf%%</p>\n", cpu_usage);
                strcat(html_response, cpu_usage_string);

                
                DIR *dir;
                struct dirent *entry;
                char path[512];
				//Isso aqui so com chatGPT
                // Get list of running processes
                // Abra o diretório /proc novamente
                dir = opendir("/proc");
                if (dir == NULL)
                {
                    die("Erro ao abrir o diretório /proc");
                }

                // Percorra os diretórios numerados (representando processos)
                while ((entry = readdir(dir)) != NULL)
                {
                    // Verifique se o nome do diretório é um número (PID)
                    if (atoi(entry->d_name) != 0)
                    {
                        // Construa o caminho completo para o arquivo de status do processo
                        sprintf(path, "/proc/%s/status", entry->d_name);

                        FILE *status_file = fopen(path, "r");
                        if (status_file)
                        {
                            char line[256];
                            char process_name[256] = "";
                            // Leia o nome do processo a partir do arquivo status
                            while (fgets(line, sizeof(line), status_file))
                            {
                                if (strstr(line, "Name:"))
                                {
                                    char *colon = strchr(line, ':');
                                    if (colon)
                                    {
                                        strncpy(process_name, colon + 2, sizeof(process_name) - 1);
                                        break;
                                    }
                                }
                            }
                            fclose(status_file);
                            // Exiba o PID e o nome do processo no arquivo HTML
                            char process_info_str[BUFLEN];
                            memset(process_info_str, 0, sizeof(process_info_str));
                            snprintf(process_info_str, sizeof(process_info_str), "<p>PID: %s, Nome do Processo: %s</p>\n", entry->d_name, process_name);
                            strcat(html_response, process_info_str);
                        }
                    }
                }

                closedir(dir);

                /* now reply to the client with the HTML data */
                if (write(conn, html_response, strlen(html_response)) < 0)
                    die("write");

                exit(0);
            }
        }
        /* close the connection */
        close(conn);
        child++;
        while (child)
        {
            pid = waitpid((pid_t)-1, NULL, WNOHANG);
            if (pid < 0)
                die("?");
            else if (pid == 0)
                break;
            else
                child--;
        }
    }

    close(s);
    return 0;
}
