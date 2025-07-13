#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>     // Para listar diretórios (/proc)
#include <csignal>      // Para enviar sinais (kill)
#include <unistd.h>     // Para sysconf, access, sleep
#include <sys/types.h>
#include <sstream>      // Para manipulação de strings
#include <iomanip>      // Para formatação da saída
#include <algorithm>
#include <cerrno>       // Para capturar códigos de erro
#include <cstring>      // Para a função strerror

struct Processo {
    int pid;
    std::string nome;
    float memoriaKB;
    float tempoCPU_s; // Tempo de CPU consumido em segundos
};

Processo obterDadosProcesso(int pid) {
    Processo p = {pid, "", 0.0, 0.0};

    // 1. Obter nome do processo de /proc/[pid]/comm
    std::ifstream comm_file("/proc/" + std::to_string(pid) + "/comm");
    if (comm_file.good()) {
        std::getline(comm_file, p.nome);
    }

    // 2. Obter uso de memória de /proc/[pid]/status
    std::ifstream status_file("/proc/" + std::to_string(pid) + "/status");
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line);
            std::string key;
            iss >> key >> p.memoriaKB;
            break;
        }
    }

    // 3. Obter tempo de CPU de /proc/[pid]/stat
    std::ifstream stat_file("/proc/" + std::to_string(pid) + "/stat");
    if (stat_file.good()) {
        std::string value;
        unsigned long utime = 0, stime = 0;
        // Os campos de tempo de usuário (utime) e kernel (stime) são o 14º e 15º.
        for (int i = 1; i < 14; ++i) {
            stat_file >> value;
        }
        stat_file >> utime >> stime; // Lemos os valores de utime e stime
        
        // Convertendo de "clock ticks" para segundos
        long ticks_per_sec = sysconf(_SC_CLK_TCK);
        p.tempoCPU_s = static_cast<float>(utime + stime) / ticks_per_sec;
    }

    return p;
}

std::vector<Processo> listarProcessos() {
    std::vector<Processo> processos;
    DIR* dir = opendir("/proc");
    if (!dir) {
        std::cerr << "Erro: Não foi possível abrir o diretório /proc." << std::endl;
        return processos;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (isdigit(entry->d_name[0])) {
            int pid = std::atoi(entry->d_name);
            processos.push_back(obterDadosProcesso(pid));
        }
    }
    closedir(dir);
    return processos;
}

void exibirProcessos(const std::vector<Processo>& processos) {
    std::cout << std::left << std::setw(25) << "Nome do Executável"
              << std::setw(10) << "PID"
              << std::setw(15) << "Memória (KB)"
              << std::setw(15) << "Tempo CPU (s)" << std::endl;
    std::cout << std::string(65, '-') << std::endl;

    for (const auto& p : processos) {
        if (!p.nome.empty()) {
            std::cout << std::left << std::setw(25) << p.nome
                      << std::setw(10) << p.pid
                      << std::fixed << std::setprecision(2) << std::setw(15) << p.memoriaKB
                      << std::setw(15) << p.tempoCPU_s
                      << std::endl;
        }
    }
}

void encerrarProcesso(int pid) {
    if (kill(pid, SIGTERM) == 0) {
        std::cout << "Enviando sinal SIGTERM para o processo " << pid << "..." << std::endl;
        sleep(1); 
        if (access(("/proc/" + std::to_string(pid)).c_str(), F_OK) != 0) {
            std::cout << "Processo " << pid << " encerrado com sucesso." << std::endl;
            return;
        }
        std::cout << "Processo " << pid << " não respondeu ao SIGTERM. Tentando SIGKILL..." << std::endl;
    }

    if (kill(pid, SIGKILL) == 0) {
        std::cout << "Processo " << pid << " encerrado com SIGKILL." << std::endl;
    } else {
        std::cerr << "Falha ao encerrar processo " << pid << ". Erro: " << strerror(errno) << std::endl;
    }
}

void menu() {
    while (true) {
        std::cout << "\n--- Monitor de Processos ---\n";
        std::cout << "1. Listar processos em execução\n";
        std::cout << "2. Encerrar um processo por PID\n";
        std::cout << "3. Sair\n";
        std::cout << "Escolha uma opção: ";
        int opcao;
        std::cin >> opcao;

        if (std::cin.fail()) {
            std::cout << "Opção inválida. Por favor, digite um número.\n";
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }

        switch (opcao) {
            case 1: {
                auto processos = listarProcessos();
                exibirProcessos(processos);
                break;
            }
            case 2: {
                std::cout << "Digite o PID do processo a ser encerrado: ";
                int pid;
                std::cin >> pid;
                if (!std::cin.fail()) {
                    encerrarProcesso(pid);
                } else {
                    std::cout << "PID inválido.\n";
                    std::cin.clear();
                    std::cin.ignore(10000, '\n');
                }
                break;
            }
            case 3:
                std::cout << "Saindo...\n";
                return;
            default:
                std::cout << "Opção inválida. Tente novamente.\n";
                break;
        }
    }
}

int main() {
    menu();
    return 0;
}