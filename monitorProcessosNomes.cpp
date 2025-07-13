#include <iostream>
#include <fstream>      // Para abrir e ler arquivos (ex: /proc/[pid]/status)
#include <string>
#include <vector>
#include <dirent.h>     // Para acessar diretórios, como /proc
#include <csignal>      // Para enviar sinais a processos (SIGTERM, SIGKILL)
#include <unistd.h>     // Para sysconf e funções POSIX
#include <sys/types.h>
#include <sstream>      // Para manipular strings como fluxo (stream)
#include <iomanip>      // Para formatar saída (alinhamento, precisão)
#include <map>          // Para armazenar processos agrupados por nome
#include <algorithm>
#include <cerrno>       // Para ler erros de sistema (errno)
#include <cstring>      // Para converter erro em texto (strerror)

using namespace std;
struct Processo {
    int pid;
    string nome;
    float memoriaKB; // Memória RAM usada (VmRSS)
    float usoCPU;    // Tempo de CPU consumido (convertido para segundos)
};

/**
 * Lê o uso de memória RAM real (VmRSS) do processo com o PID informado.
 * A informação vem do arquivo /proc/[pid]/status.
 */
float obterMemoriaKB(int pid) {
    ifstream file("/proc/" + to_string(pid) + "/status");  // Abre arquivo de status do processo
    string line;

    while (getline(file, line)) {
        if (line.rfind("VmRSS:", 0) == 0) { // Verifica se a linha começa com "VmRSS:"
            istringstream iss(line);      // Transforma linha em stream para dividir os elementos
            string label, unit;
            float memKB;
            iss >> label >> memKB >> unit; // Extrai as três partes: "VmRSS:", valor numérico e unidade "kB"
            return memKB;  // Retorna só o valor numérico (em KB)
        }
    }
    return 0.0; // Retorna 0 se não encontrou a linha desejada
}

/**
 * Lê o tempo total de CPU usado por um processo a partir do arquivo /proc/[pid]/stat.
 * Esse tempo é convertido de "clock ticks" para segundos.
 */
float obterUsoCPU(int pid) {
    ifstream file("/proc/" + to_string(pid) + "/stat");
    if (!file) return 0.0;

    string lixo;
    unsigned long utime, stime;

    file >> lixo >> lixo;  // Ignora os dois primeiros campos (PID e nome do processo)

    for (int i = 0; i < 11; i++) file >> lixo;    // Pula os próximos 11 campos, que não interessam

    file >> utime >> stime;    // Lê o tempo que o processo gastou no modo usuário (utime) e kernel (stime)

    float total_time = utime + stime;

    return (total_time / sysconf(_SC_CLK_TCK));    // Converte de "clock ticks" para segundos, com base no sistema
}

/**
 * Lê o nome do processo a partir do arquivo /proc/[pid]/comm.
 */
string obterNome(int pid) {
    ifstream file("/proc/" + to_string(pid) + "/comm");
    string nome;
    getline(file, nome);  // A primeira linha já é o nome do processo
    return nome;
}

/**
 * Percorre o diretório /proc, identifica os processos em execução,
 * agrupa por nome e coleta os dados de PID, memória e CPU.
 */
map<string, vector<Processo>> listar_processos() {
    map<string, vector<Processo>> grupos;
    DIR* dir = opendir("/proc");
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) { // Só considera entradas numéricas, que são os PIDs
        if (isdigit(entry->d_name[0])) {
            int pid = atoi(entry->d_name);
            string nome = obterNome(pid);
            float mem = obterMemoriaKB(pid);
            float cpu = obterUsoCPU(pid);
            grupos[nome].push_back({pid, nome, mem, cpu}); // Agrupa processos com o mesmo nome (ex: vários processos do Spotify)

        }
    }

    closedir(dir);
    return grupos;
}

/**
 * Exibe na tela os processos agrupados por nome.
 * Mostra PID, memória usada e tempo de CPU.
 */
void exibir_processos(const map<string, vector<Processo>>& grupos) {
    cout << left << setw(25) << "Nome"
         << setw(10) << "PID"
         << setw(15) << "Memória (KB)"
         << setw(10) << "%CPU" << endl;
    cout << string(60, '-') << endl;
    for (const auto& [nome, lista] : grupos) {
        for (const Processo& p : lista) {
            cout << left << setw(25) << nome
                 << setw(10) << p.pid
                 << setw(15) << fixed << setprecision(2) << p.memoriaKB
                 << setw(10) << fixed << setprecision(2) << p.usoCPU << endl;
        }
    }
}

/**
 * @brief Tenta encerrar um processo por PID, e verifica se ele foi encerrado.
 * Primeiro envia SIGTERM. Se ainda existir, tenta SIGKILL.
 * Retorna true se o processo foi encerrado, false se falhou.
 */
bool encerrar_por_pid(int pid) {    // Envia SIGTERM
    if (kill(pid, SIGTERM) == 0) {
        sleep(1); // espera 1 segundo para o processo responder
        if (access(("/proc/" + to_string(pid)).c_str(), F_OK) != 0) {
            cout << "Processo " << pid << " encerrado com SIGTERM\n";
            return true;
        } else {
            cerr << "SIGTERM não foi suficiente para o processo " << pid << ". Tentando SIGKILL...\n";
        }
    } else {
        cerr << "Erro ao enviar SIGTERM ao processo " << pid << ": " << strerror(errno) << "\n";
    }

    if (kill(pid, SIGKILL) == 0) {    // Se ainda estiver rodando, tenta SIGKILL
        sleep(1);
        if (access(("/proc/" + to_string(pid)).c_str(), F_OK) != 0) {
            cout << "Processo " << pid << " encerrado com SIGKILL\n";
            return true;
        } else {
            cerr << "SIGKILL falhou: processo " << pid << " ainda existe \n";
        }
    } else {
        cerr << "Erro ao enviar SIGKILL ao processo " << pid << ": " << strerror(errno) << "\n";
    }

    return false;
}

/**
 * @brief Encerra todos os processos com determinado nome.
 * Mostra quantos foram encerrados com sucesso ou falharam.
 */
void encerrar_por_nome(const string& nome) {
    auto grupos = listar_processos();
    if (grupos.count(nome) == 0) {
        cout << "Nenhum processo com nome \"" << nome << "\" encontrado.\n";
        return;
    }

    int sucesso = 0;
    int falha = 0;

    for (const Processo& p : grupos[nome]) {
        bool ok = encerrar_por_pid(p.pid);
        if (ok) sucesso++;
        else falha++;
    }

    cout << "\nResumo: " << sucesso << " processos encerrados com sucesso, "
         << falha << " com falha.\n";
}

/**
 * Exibe um menu interativo no terminal.
 * Permite listar, encerrar por PID ou por nome.
 */
void menu() {
    while (true) {
        cout << "\n--- Monitor de Processos ---\n";
        cout << "1. Listar processos\n";
        cout << "2. Encerrar processo por PID\n";
        cout << "3. Encerrar todos os processos por nome\n";
        cout << "4. Sair\n";
        cout << "Escolha uma opção: ";
        int opcao;
        cin >> opcao;

        if (opcao == 1) {
            auto grupos = listar_processos();
            exibir_processos(grupos);
        } else if (opcao == 2) {
            cout << "Digite o PID: ";
            int pid;
            cin >> pid;
            encerrar_por_pid(pid);
        } else if (opcao == 3) {
            cout << "Digite o nome do processo: ";
            string nome;
            cin >> nome;
            encerrar_por_nome(nome);
        } else if (opcao == 4) {
            cout << "Saindo do monitor.\n";
            break;
        } else {
            cout << "Opção inválida.\n";
        }
    }
}

int main() {
    menu(); 
    return 0;
}
