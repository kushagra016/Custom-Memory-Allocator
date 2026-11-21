#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <mutex>
#include <cstdlib>

#include "httplib.h"
#include "json.hpp"
#include "utils.h"
#include "allocator.h"

using json = nlohmann::json;

bool suppressOutput = false;
MemoryAllocator* globalAllocator = nullptr;
AllocationStrategy globalStrategy = AllocationStrategy::FIRST_FIT;
int globalCurrentId = 0;

std::mutex webRoutingMutex;
int globalAllocationCounter = 1;

struct DataBlock {
    void* operator new(size_t size) {
        return globalAllocator->allocate(size, globalStrategy);
    }
    
    void operator delete(void* ptr) {
        globalAllocator->deallocate(ptr);
    }
};

struct StandardNode {  
    char byte;
    StandardNode() {}
    ~StandardNode() {}
};

int main() {
    std::cout << "Initializing Custom Memory Allocator...\n";
    
    size_t poolSize = 10 * 1024 * 1024;
    MemoryAllocator allocFirst(poolSize, "FIRST-FIT STRATEGY");
    MemoryAllocator allocBest(poolSize, "BEST-FIT STRATEGY");
    MemoryAllocator allocWorst(poolSize, "WORST-FIT STRATEGY");

    std::vector<AllocationRecord> allocatedPointers;

    long long totalCustomTimeNs = 0;
    long long totalOSTimeNs = 0;
    int totalAllocOps = 0;
    int totalDeallocOps = 0;

    httplib::Server svr;

    svr.Options(".*", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 200;
    });

    svr.Get("/api/memory-map", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string requestedUnit = "KB";
        if (req.has_param("unit")) {
            requestedUnit = req.get_param_value("unit");
        }

        json response;
        response["firstFit"] = allocFirst.getMemoryMapJson(requestedUnit);
        response["bestFit"] = allocBest.getMemoryMapJson(requestedUnit);
        response["worstFit"] = allocWorst.getMemoryMapJson(requestedUnit);
        res.set_content(response.dump(), "application/json");
    });

    svr.Get("/api/analytics", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json response;
        response["firstFit"] = allocFirst.getAnalyticsJson();
        response["bestFit"] = allocBest.getAnalyticsJson();
        response["worstFit"] = allocWorst.getAnalyticsJson();
        res.set_content(response.dump(), "application/json");
    });

    svr.Get("/api/benchmark", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::lock_guard<std::mutex> routeLock(webRoutingMutex);

        int totalOps = totalAllocOps + totalDeallocOps;
        double osUs = totalOSTimeNs / 1000.0;
        double customUs = totalCustomTimeNs / 1000.0;
        double osMs = totalOSTimeNs / 1000000.0;
        double customMs = totalCustomTimeNs / 1000000.0;

        json response;
        response["totalOps"] = totalOps;
        response["allocOps"] = totalAllocOps;
        response["deallocOps"] = totalDeallocOps;
        response["osUs"] = osUs;
        response["osMs"] = osMs;
        response["customUs"] = customUs;
        response["customMs"] = customMs;
        response["customTimeNs"] = totalCustomTimeNs;
        response["osTimeNs"] = totalOSTimeNs;

        res.set_content(response.dump(), "application/json");
    });

    svr.Post("/api/allocate", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json reqData = json::parse(req.body);

        double val = reqData["val"];
        std::string unit = reqData["unit"];
        size_t requestedBytes = convertToBytes(val, unit);

        auto startOS = std::chrono::high_resolution_clock::now();
        void* ptrOS = ::operator new(requestedBytes);
        auto endOS = std::chrono::high_resolution_clock::now();
        long long osAllocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endOS - startOS).count();

        json response;
        std::lock_guard<std::mutex> routeLock(webRoutingMutex);
        int currentId = globalAllocationCounter++;

        auto t1 = std::chrono::high_resolution_clock::now();
        void* ptrF = allocFirst.allocate(requestedBytes, AllocationStrategy::FIRST_FIT, currentId);
        auto t2 = std::chrono::high_resolution_clock::now();
        long long customAllocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

        void* ptrB = allocBest.allocate(requestedBytes, AllocationStrategy::BEST_FIT, currentId);
        void* ptrW = allocWorst.allocate(requestedBytes, AllocationStrategy::WORST_FIT, currentId);

        if (ptrF && ptrB && ptrW) {
            allocatedPointers.push_back({ptrF, ptrB, ptrW, ptrOS, val});
            totalOSTimeNs += osAllocNs;
            totalCustomTimeNs += customAllocNs;
            totalAllocOps++;
            response["status"] = "SUCCESS";
        } else {
            ::operator delete(ptrOS);
            if (ptrF) allocFirst.deallocate(ptrF);
            if (ptrB) allocBest.deallocate(ptrB);
            if (ptrW) allocWorst.deallocate(ptrW);
            response["status"] = "FAILED";
        }
        res.set_content(response.dump(), "application/json");
    });

    svr.Post("/api/deallocate", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json reqData = json::parse(req.body);
        int targetId = reqData["id"];

        json response;
        response["status"] = "FAILED";

        std::lock_guard<std::mutex> routeLock(webRoutingMutex);
        void* primaryPtr = allocFirst.getPointerByUniqueId(targetId);

        if (primaryPtr) {
            auto it = std::find_if(allocatedPointers.begin(), allocatedPointers.end(),
                [&](const AllocationRecord& rec) { return rec.firstFitPtr == primaryPtr; });

            if (it != allocatedPointers.end()) {
                long long osDeallocNs = 0;
                if (it->osPtr) {
                    auto startOS = std::chrono::high_resolution_clock::now();
                    ::operator delete(it->osPtr);
                    auto endOS = std::chrono::high_resolution_clock::now();
                    osDeallocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endOS - startOS).count();
                }

                auto t1 = std::chrono::high_resolution_clock::now();
                allocFirst.deallocate(it->firstFitPtr);
                auto t2 = std::chrono::high_resolution_clock::now();
                long long customDeallocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

                allocBest.deallocate(it->bestFitPtr);
                allocWorst.deallocate(it->worstFitPtr);

                totalOSTimeNs += osDeallocNs;
                totalCustomTimeNs += customDeallocNs;
                totalDeallocOps++;

                allocatedPointers.erase(it);
                response["status"] = "SUCCESS";
            }
        }
        res.set_content(response.dump(), "application/json");
    });

    if (const char* env_p = std::getenv("PORT")) {
        int port = std::stoi(env_p);
        std::cout << "Starting cloud server on port " << port << "...\n";
        svr.listen("0.0.0.0", port);
        return 0;
    }

    std::thread serverThread([&svr]() {
        svr.listen("0.0.0.0", 8080);
    });
    std::cout << "Web server active in background on http://localhost:8080\n";


    std::string globalUnit;
    std::cout << "\nBefore we start, select your preferred operating unit (B, KB, MB, GB): ";
    std::cin >> globalUnit;
    
    for (char &c : globalUnit) c = std::toupper(c);
    if (globalUnit != "B" && globalUnit != "KB" && globalUnit != "MB" && globalUnit != "GB") {
        globalUnit = "B";
    }

    int stratChoice = 0;
    std::cout << "Select your primary Allocation Strategy:\n";
    std::cout << "1. First-Fit\n2. Best-Fit\n3. Worst-Fit\n";
    std::cout << "Enter choice (1-3): ";
    std::cin >> stratChoice;

    MemoryAllocator* primaryAlloc = &allocFirst;
    if (stratChoice == 2) primaryAlloc = &allocBest;
    else if (stratChoice == 3) primaryAlloc = &allocWorst;
    
    std::cout << "\n-------------------------------------------------\n";
    std::cout << "Allocated Memory Pool of " << std::fixed << std::setprecision(2) 
              << convertFromBytes(poolSize, globalUnit) << " " << globalUnit 
              << " upfront from the OS.\n";
    std::cout << "-------------------------------------------------\n";

    int choice = 0;
    while (choice != 6) {
        std::cout << "\n--- Custom Memory Allocator Menu ---\n";
        std::cout << "1. Allocate Memory\n";
        std::cout << "2. Deallocate Memory\n";
        std::cout << "3. Display Memory Map\n";
        std::cout << "4. Compare All Strategies\n";
        std::cout << "5. Run STL Performance Benchmark\n";
        std::cout << "6. Exit and Show Summary\n";
        std::cout << "Enter your choice: ";
        
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }

        switch (choice) {
            case 1: {
                double val;
                std::cout << "Enter amount to allocate (in " << globalUnit << "): ";
                std::cin >> val;

                size_t requestedBytes = convertToBytes(val, globalUnit);
                if (requestedBytes == 0) {
                    std::cout << "Please enter a value greater than 0.\n";
                    break;
                }
                
                auto startOS = std::chrono::high_resolution_clock::now();
                void* ptrOS = ::operator new(requestedBytes);
                auto endOS = std::chrono::high_resolution_clock::now();
                long long osAllocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endOS - startOS).count();

                void* ptrF = nullptr;
                void* ptrB = nullptr;
                void* ptrW = nullptr;
                long long customAllocNs = 0;

                globalAllocator = &allocFirst;
                globalStrategy = AllocationStrategy::FIRST_FIT;
                if (stratChoice == 1) {
                    auto t1 = std::chrono::high_resolution_clock::now();
                    ptrF = DataBlock::operator new(requestedBytes);
                    auto t2 = std::chrono::high_resolution_clock::now();
                    customAllocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
                } else {
                    ptrF = DataBlock::operator new(requestedBytes);
                }

                globalAllocator = &allocBest;
                globalStrategy = AllocationStrategy::BEST_FIT;
                if (stratChoice == 2) {
                    auto t1 = std::chrono::high_resolution_clock::now();
                    ptrB = DataBlock::operator new(requestedBytes);
                    auto t2 = std::chrono::high_resolution_clock::now();
                    customAllocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
                } else {
                    ptrB = DataBlock::operator new(requestedBytes);
                }

                globalAllocator = &allocWorst;
                globalStrategy = AllocationStrategy::WORST_FIT;
                if (stratChoice == 3) {
                    auto t1 = std::chrono::high_resolution_clock::now();
                    ptrW = DataBlock::operator new(requestedBytes);
                    auto t2 = std::chrono::high_resolution_clock::now();
                    customAllocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
                } else {
                    ptrW = DataBlock::operator new(requestedBytes);
                }

                if (ptrF && ptrB && ptrW) {
                    allocatedPointers.push_back({ptrF, ptrB, ptrW, ptrOS, val});
                    totalOSTimeNs += osAllocNs;
                    totalCustomTimeNs += customAllocNs;
                    totalAllocOps++;
                    std::cout << "Successfully allocated " << val << " " << globalUnit << ".\n";
                } else {
                    std::cout << "Allocation failed due to fragmentation.\n";
                    ::operator delete(ptrOS);
                    if (ptrF) { globalAllocator = &allocFirst; DataBlock::operator delete(ptrF); }
                    if (ptrB) { globalAllocator = &allocBest; DataBlock::operator delete(ptrB); }
                    if (ptrW) { globalAllocator = &allocWorst; DataBlock::operator delete(ptrW); }
                }
                break;
            }
            case 2: {
                if (allocatedPointers.empty()) {
                    std::cout << "No blocks currently allocated to free.\n";
                    break;
                }
                
                std::cout << "\n";
                primaryAlloc->printActiveBlockIds();
                
                std::cout << "Enter the Block ID to deallocate: ";
                int id;
                std::cin >> id;

                void* primaryPtr = primaryAlloc->getPointerFromBlockId(id);
                
                if (primaryPtr) {
                    auto it = std::find_if(allocatedPointers.begin(), allocatedPointers.end(),
                        [&](const AllocationRecord& rec) {
                            if (stratChoice == 1) return rec.firstFitPtr == primaryPtr;
                            if (stratChoice == 2) return rec.bestFitPtr == primaryPtr;
                            return rec.worstFitPtr == primaryPtr;
                        });

                    if (it != allocatedPointers.end()) {
                        auto startOS = std::chrono::high_resolution_clock::now();
                        ::operator delete(it->osPtr);
                        auto endOS = std::chrono::high_resolution_clock::now();
                        long long osDeallocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endOS - startOS).count();

                        long long customDeallocNs = 0;

                        globalAllocator = &allocFirst;
                        if (stratChoice == 1) {
                            auto t1 = std::chrono::high_resolution_clock::now();
                            DataBlock::operator delete(it->firstFitPtr);
                            auto t2 = std::chrono::high_resolution_clock::now();
                            customDeallocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
                        } else {
                            DataBlock::operator delete(it->firstFitPtr);
                        }

                        globalAllocator = &allocBest;
                        if (stratChoice == 2) {
                            auto t1 = std::chrono::high_resolution_clock::now();
                            DataBlock::operator delete(it->bestFitPtr);
                            auto t2 = std::chrono::high_resolution_clock::now();
                            customDeallocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
                        } else {
                            DataBlock::operator delete(it->bestFitPtr);
                        }

                        globalAllocator = &allocWorst;
                        if (stratChoice == 3) {
                            auto t1 = std::chrono::high_resolution_clock::now();
                            DataBlock::operator delete(it->worstFitPtr);
                            auto t2 = std::chrono::high_resolution_clock::now();
                            customDeallocNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
                        } else {
                            DataBlock::operator delete(it->worstFitPtr);
                        }

                        totalOSTimeNs += osDeallocNs;
                        totalCustomTimeNs += customDeallocNs;
                        totalDeallocOps++;

                        allocatedPointers.erase(it);
                        std::cout << "\nBlock " << id << " successfully deallocated.\n";
                    }
                } else {
                    std::cout << "Invalid Block ID or Block is already FREE.\n";
                }
                break;
            }
            case 3:
                primaryAlloc->printMemoryMap(globalUnit);
                break;
            case 4:
                std::cout << "\n========================================================\n";
                std::cout << "             STRATEGY COMPARISON VISUALIZER               \n";
                std::cout << "========================================================";
                allocFirst.printMemoryMap(globalUnit);
                std::cout << "\n--------------------------------------------------------";
                allocBest.printMemoryMap(globalUnit);
                std::cout << "\n--------------------------------------------------------";
                allocWorst.printMemoryMap(globalUnit);
                std::cout << "\n========================================================\n";
                break;
            case 5: {
                std::cout << "\n--- STL Performance Benchmark ---\n";
                int totalOps = totalAllocOps + totalDeallocOps;

                if (totalOps == 0) {
                    std::cout << "No operations performed yet. Allocate or deallocate memory first.\n";
                    break;
                }

                double osUs = totalOSTimeNs / 1000.0;
                double customUs = totalCustomTimeNs / 1000.0;

                double osMs = totalOSTimeNs / 1000000.0;
                double customMs = totalCustomTimeNs / 1000000.0;

                std::cout << "Measured Operations: " << totalOps
                          << " (" << totalAllocOps << " allocations, "
                          << totalDeallocOps << " deallocations)\n\n";

                std::cout << std::fixed << std::setprecision(4);
                std::cout << "[RESULTS]\n";
                std::cout << "Standard OS (new/delete): " << osUs << " us (" << osMs << " ms)\n";
                std::cout << "Custom Allocator Engine:  " << customUs << " us (" << customMs << " ms)\n\n";

                if (totalCustomTimeNs < totalOSTimeNs) {
                    double diffUs = osUs - customUs;
                    double diffMs = osMs - customMs;
                    std::cout << ">>> SUCCESS: Custom Allocator was faster by "
                              << diffUs << " us (" << diffMs << " ms)!\n";
                } else if (totalCustomTimeNs == totalOSTimeNs) {
                    std::cout << ">>> RESULT: Both allocators matched in execution time.\n";
                } else {
                    double diffUs = customUs - osUs;
                    double diffMs = customMs - osMs;
                    std::cout << ">>> NOTE: OS was faster by "
                              << diffUs << " us (" << diffMs << " ms) on these operations.\n";
                }
                break;
            }
            case 6:
                std::cout << "\nExiting System. Generating Final Summary...\n\n";
                primaryAlloc->printSummaryTable(globalUnit);
                break;
            default:
                std::cout << "Invalid choice. Please select an option from 1 to 5.\n";
        }
    }

    svr.stop();
    if (serverThread.joinable()) {
        serverThread.join();
    }

    return 0;
}