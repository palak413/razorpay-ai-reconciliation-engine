#include <iostream>
#include <chrono>
#include <vector>

int main() {
    std::cout << "Records | Time (ms) | Throughput (rec/sec)\n";
    std::cout << "----------------------------------------\n";
    
    std::vector<int> test_sizes = {100, 1000, 10000, 100000};
    for (int size : test_sizes) {
        auto start = std::chrono::steady_clock::now();
        
        volatile double checksum = 0.0;
        for (int i = 0; i < size; ++i) {
            checksum += i * 1.05;
        }
        
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        double ms = duration.count();
        double throughput = size / (ms / 1000.0);
        
        std::cout << size << " | " << ms << " | " << throughput << "\n";
    }
    return 0;
}