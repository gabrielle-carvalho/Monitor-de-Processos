#include <iostream>
#include <fstream>      // Para leitura de arquivos (ifstream)
#include <string>
#include <vector>
#include <dirent.h>     // Para navegar em diretórios (opendir, readdir)
#include <csignal>      // Para a função kill()
#include <algorithm>    // Para std::all_of
#include <iomanip>      // Para formatação da saída (setw)
#include <limits>       // Para limpar o buffer de entrada
#include <cstring>      // Para strcmp

void listarProcessos() {
    const char* proc_dir = "/proc";
    DIR* dir = opendir(proc_dir);
    if (dir == nullptr) {
        perror("Erro ao abrir o diretorio /proc");
        return;
    }
    std::cout << "\n------------------------------------------------------------------" << std::endl;
    std::cout << std::left << std::setw(10) << "PID"
              << std::left << std::setw(40) << "Nome do Executavel"
              << std::left << std::setw(15) << "Memoria (MB)" << std::endl;
    std::cout << "------------------------------------------------------------------" << std::endl;

    struct dirent* entry;
    // Itera sobre todas as entradas do diretório /proc
    while ((entry = readdir(dir)) != nullptr) {
        // Verifica se o nome da entrada é composto apenas por dígitos (um PID)
        if (entry->d_type == DT_DIR) {
            char* endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr == '\0') { // Conversão para número foi bem-sucedida
                std::string status_path = std::string(proc_dir) + "/" + entry->d_name + "/status";
                std::ifstream status_file(status_path);

                if (status_file) {
                    std::string line;
                    std::string nome;
                    double memoria_kb = 0.0;

                    // Lê o arquivo /proc/[pid]/status linha por linha
                    while (std::getline(status_file, line)) {
                        if (line.rfind("Name:", 0) == 0) {
                            nome = line.substr(6); // Pega a substring após "Name:"
                            nome.erase(0, nome.find_first_not_of(" \t")); // Remove espaços em branco
                        } else if (line.rfind("VmRSS:", 0) == 0) {
                            // VmRSS é o "Resident Set Size", memória física (RAM) usada.
                            memoria_kb = std::stod(line.substr(7));
                        }
                    }

                    std::cout << std::left << std::setw(10) << pid
                              << std::left << std::setw(40) << nome
                              << std::left << std::setw(15) << std::fixed << std::setprecision(2) << (memoria_kb / 1024.0)
                              << std::endl;
                }
            }
        }
    }

    closedir(dir); // Libera os recursos do diretório
    std::cout << "------------------------------------------------------------------" << std::endl;
}

// Função para encerrar um processo dado o seu PID
void encerrarProcesso() {
    pid_t pid;
    std::cout << "\nDigite o PID do processo que deseja encerrar: ";
    std::cin >> pid;

    // Validação de entrada
    if (std::cin.fail()) {
        std::cerr << "Entrada invalida. Por favor, digite um numero." << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return;
    }

    // A chamada de sistema kill() envia um sinal para um processo.
    // SIGKILL é um sinal que não pode ser ignorado e encerra o processo imediatamente.
    if (kill(pid, SIGKILL) == 0) {
        std::cout << "Sinal SIGKILL enviado com sucesso para o processo com PID " << pid << "." << std::endl;
    } else {
        // perror imprime a mensagem de erro do sistema correspondente a `errno`
        perror("Erro ao enviar o sinal");
        std::cerr << "Verifique se o PID e valido e se voce tem permissao para encerrar o processo." << std::endl;
    }
}

// Função principal que exibe o menu e gerencia a interação com o usuário
int main() {
    int escolha = 0;
    while (true) {
        std::cout << "\n===== Monitor de Processos (Linux) =====" << std::endl;
        std::cout << "1. Listar processos em execucao" << std::endl;
        std::cout << "2. Encerrar um processo (por PID)" << std::endl;
        std::cout << "3. Sair" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Escolha uma opcao: ";
        std::cin >> escolha;

        if (std::cin.fail()) {
            std::cerr << "Opcao invalida. Por favor, digite um numero." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        switch (escolha) {
            case 1:
                listarProcessos();
                break;
            case 2:
                encerrarProcesso();
                break;
            case 3:
                std::cout << "Saindo do programa..." << std::endl;
                return 0;
            default:
                std::cerr << "Opcao invalida. Tente novamente." << std::endl;
                break;
        }
    }

    return 0;
}
