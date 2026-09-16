void printMemoryMap(){
        BlockHeader* current = head;
        std:: cout << "\n=== Current Memory Map ===\n";

        int blockCount = 0;
        size_t totalFree = 0;
        size_t totalUsed = 0;

        while (current != nullptr){
            std::cout << "[";
            if (current->isFree){
                std::cout << "FREE";
                totalFree += current->size;
            }
            else {
                std::cout << "USED";
                totalUsed += current->size;
            }
            std::cout << ": " << current->size << "B] ";

            if (current->next != nullptr){
                std::cout << "->";
            }

            blockCount++;
            current = current->next;
        }

        std::cout << "\nBlocks: " << blockCount
                  << " | Used Data: " << totalUsed << "B"
                  << " | Free Space: " << totalFree << "B\n";
        std::cout << "===========================\n";
    }
