void runSimulation(AllocationStrategy strategy, std::string strategyName) {
    std::cout << "\n======================================================\n";
    std::cout << ">>> RUNNING STRATEGY: " << strategyName << " <<<\n";
    std::cout << "========================================================\n";

    MemoryAllocator allocator(1024);

    void *p1 = allocator.allocate(200, strategy);
    void *p2 = allocator.allocate(50, strategy);
    void *p3 = allocator.allocate(150, strategy);
    void *p4 = allocator.allocate(50, strategy);
    void *p5 = allocator.allocate(300, strategy);

    allocator.deallocate(p1); 
    allocator.deallocate(p3); 
    allocator.deallocate(p5); 

    std::cout << "\n--- Before Test Allocation ---\n";
    std::cout << "Available Holes: 200B (First), 150B (Smallest), 300B (Largest)\n";
    allocator.printMemoryMap();

    std::cout << "\n>>> ACTION: Allocating 100 bytes...\n";
    void *testPtr = allocator.allocate(100, strategy);

    std::cout << "\n--- After Test Allocation ---\n";
    allocator.printMemoryMap();
}

int main() {
    std::cout << "============== Allocation Strategy Comparison ==============\n";

    runSimulation(AllocationStrategy::FIRST_FIT, "FIRST-FIT STRATEGY");
    runSimulation(AllocationStrategy::BEST_FIT, "BEST-FIT STRATEGY");
    runSimulation(AllocationStrategy::WORST_FIT, "WORST-FIT STRATEGY");

    return 0;
}