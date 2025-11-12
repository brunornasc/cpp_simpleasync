#include <iostream>
#include "simpleasync.h"

int trabalho(int id);
void exemploAllOf_multiplosRetornos();
void exemploAllOf();
void exemploRunAsync();
void exemploSupplyAsync();

int main(int argc, char **argv) {

    // simpleasync::runAsync([]() -> void {
    //    std::cout << "thread " << std::this_thread::get_id() << " executando o trabalho... \n";
    // });
    exemploSupplyAsync();

    std::this_thread::sleep_for(std::chrono::seconds(5));

    //std::cout << "thread principal " << std::this_thread::get_id() << std::endl;
}

void exemploSupplyAsync() {
    auto task = simpleasync::supplyAsync([]() {
        return "string 1";
    }).then([](std::string str) {
       std::cout << "string recebida: " << str << std::endl;
        return 56;
    }).then([](int i) {
        std::cout << "inteiro recebido: " << i << std::endl;
        return 2.5;
    });

    std::cout << task.get();
}

void exemploRunAsync() {
    simpleasync::runAsync([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "[Thread " << std::this_thread::get_id() << "] testando runAsync \n";
    });
}

void exemploAllOf_multiplosRetornos() {
    auto f1 = simpleasync::supplyAsync([]() { return 42; });
    auto f2 = simpleasync::supplyAsync([]() { return std::string("hello"); });
    auto f3 = simpleasync::supplyAsync([]() { return 3.14; });

    auto combined = simpleasync::allOf(std::move(f1), std::move(f2), std::move(f3));
    auto [num, str, pi] = combined.get();

    std::cout << num << ", " << str << ", " << pi << "\n";
}

void exemploAllOf() {
    std::vector<simpleasync::Future<int>> futures;

    for (int i = 0; i < 5; ++i) {
        futures.push_back(
            simpleasync::supplyAsync([i]() {
                return trabalho(i);
            })
        );
    }

    // Aguarda todas terminarem
    std::cout << "[Main] Aguardando todas as tasks...\n";
    auto combined = simpleasync::allOf(std::move(futures));

    std::vector<int> results = combined.get();

    std::cout << "[Main] Resultados:\n";
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "  Tarefa " << i << ": " << results[i] << "\n";
    }
}

int trabalho(int id) {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Iniciando trabalho " << id << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Finalizando trabalho " << id << "\n";
    return id * 10;
}