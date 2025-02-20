#include <Windows.h>
#include <algorithm>

#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>

// Define Point structure
struct Point {
    int x = 0, y = 0;
    Point(int _x = 0, int _y = 0) : x(_x), y(_y) {}
};

// Define Block structure
struct Block {
    unsigned char value = 0;
    Point point;

    Block(int x = 0, int y = 0) : point(x, y) {}
    Block(Point p) : point(p) {}

    bool operator==(const Block& other) const {
        return point.x == other.point.x && point.y == other.point.y;
    }
};

class Link {
public:
    Link() = default;
    ~Link() { if (processHandle) CloseHandle(processHandle); }

    void run() {
        while (true) {
            handleKeyInput();
            if (!isRunning) {
                Sleep(100);
                continue;
            }

            if (!initializeGame()) {
                Sleep(1000);
                continue;
            }

            runGameLoop();
        }
    }

private:
    void handleKeyInput() {
        if (isKeyPressed(VK_F1) && !isRunning) {
            isRunning = true;
            std::cout << "Program started, press F2 to pause" << std::endl;
            Sleep(200);
        }
        else if (isKeyPressed(VK_F2) && isRunning) {
            isRunning = false;
            std::cout << "Program paused, press F1 to continue" << std::endl;
            Sleep(200);
        }
    }

    bool initializeGame() {
        if (!readWindow()) {
            std::cout << "Failed to read process" << std::endl;
            return false;
        }
        return true;
    }

    void runGameLoop() {
        while (isRunning) {
            if (isKeyPressed(VK_F2)) {
                isRunning = false;
                std::cout << "Program paused, press F1 to continue" << std::endl;
                Sleep(200);
                break;
            }

            if (!processGameState()) break;
            Sleep(1);
        }
    }

    bool processGameState() {
        if (!readBlocksFromProcessMemory()) {
            std::cout << "Failed to read memory" << std::endl;
            return true;
        }

        if (blocks_.empty()) return false;

        find_matching_coordinates();
        eiminationBox();
        return true;
    }

    bool readWindow() {
        windowHandle = FindWindowW(nullptr, L"QQ游戏 - 连连看角色版");
        if (!windowHandle) {
            std::cout << "Target process not found" << std::endl;
            return false;
        }

        DWORD processId;
        GetWindowThreadProcessId(windowHandle, &processId);
        processHandle = OpenProcess(PROCESS_VM_READ, FALSE, processId);

        return processHandle != nullptr;
    }

    void find_matching_coordinates() {
        blockspair_.clear();
        std::unordered_map<unsigned char, std::vector<Block>> valueToBlocksMap;

        // Group by value
        for (const auto& block : blocks_) {
            valueToBlocksMap[block.value].push_back(block);
        }

        // Matching
        for (const auto& [value, blocks] : valueToBlocksMap) {
            for (size_t i = 0; i < blocks.size(); ++i) {
                for (size_t j = i + 1; j < blocks.size(); ++j) {
                    if (can_connect(blocks[i], blocks[j])) {
                        blockspair_.emplace_back(blocks[i], blocks[j]);
                    }
                }
            }
        }
    }

    // Read blocks from process memory
    bool readBlocksFromProcessMemory() {
        if (processHandle == nullptr) {
            std::cout << "Cannot get process handle" << std::endl;
            return false;
        }
        blocks_.clear();
        // Use smart pointer to manage memory
        size_t regionSize = END_ADDRESS - START_ADDRESS + 1;
        std::unique_ptr<unsigned char[]> buffer(new unsigned char[regionSize]);

        // Read memory
        SIZE_T bytesRead;
        if (!ReadProcessMemory(processHandle,
            reinterpret_cast<LPCVOID>(START_ADDRESS), buffer.get(),
            regionSize, &bytesRead)) {
            return false;
        }

        // Process memory data
        for (int row = 0; row < COLUMN_COUNT; ++row) {
            for (int col = 0; col < NUM_COLS; ++col) {
                int index = row * NUM_COLS + col;
                unsigned char value = buffer[index];
                if (value != 0) {
                    Block block;
                    block.value = value;
                    block.point.x = col;
                    block.point.y = row;
                    blocks_.emplace_back(block);
                }
            }
        }
        return true;
    }

    void click(int index_x, int index_y) {
        // Get process window position info
        RECT rect;
        GetWindowRect(windowHandle, &rect);
        int windowWidth = rect.right - rect.left;
        int windowHeight = rect.bottom - rect.top;

        // Calculate window center coordinates
        int topleft_x = rect.left + 25;
        int topleft_y = rect.top + 195;

        // Move mouse to process window center coordinates
        topleft_x += (index_x * 31);
        topleft_y += (index_y * 35);

        SetCursorPos(topleft_x, topleft_y); // Set mouse position

        // Simulate mouse left button press and release
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    }

    void eiminationBox() {
        SetWindowPos(windowHandle, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE); // Set window to top layer

        for (auto& x : blockspair_) {
            click(x.first.point.x, x.first.point.y);  // Click first block
            click(x.second.point.x, x.second.point.y); // Click second block
            Sleep(1); // Optional: Add delay between clicks
        }
    }

    bool can_connect(const Block& coord1, const Block& coord2) {
        int x1 = coord1.point.x, y1 = coord1.point.y;
        int x2 = coord2.point.x, y2 = coord2.point.y;

        // Check if in same row
        if (y1 == y2) {
            for (int x = std::min<int>(int(x1), int(x2)) + 1;
                x < std::max<int>(int(x1), int(x2)); ++x) {
                if (std::find(blocks_.begin(), blocks_.end(), Block(x, y1)) !=
                    blocks_.end())
                    break; // If there's a block in between, cannot connect
            }
            return true; // Can connect
        }

        // Check if in same column
        if (x1 == x2) {
            for (int y = std::min<int>(int(y1), int(y2)) + 1;
                y < std::max<int>(int(y1), int(y2)); ++y) {
                if (std::find(blocks_.begin(), blocks_.end(), Block(x1, y)) !=
                    blocks_.end())
                    break; // If there's a block in between, cannot connect
            }
            return true; // Can connect
        }

        // Check right angle
        if (std::find(blocks_.begin(), blocks_.end(), Block(int(x1), int(y2))) ==
            blocks_.end() &&
            can_connect(Block(int(x1), int(y1)), Block(int(x1), int(y2))) &&
            can_connect(Block(int(x1), int(y2)), Block(int(x2), int(y2)))) {
            return true; // Can connect
        }

        if (std::find(blocks_.begin(), blocks_.end(), Block(int(x2), int(y1))) ==
            blocks_.end() &&
            can_connect(Block(int(x1), int(y1)), Block(int(x2), int(y1))) &&
            can_connect(Block(int(x2), int(y1)), Block(int(x2), int(y2)))) {
            return true; // Can connect
        }

        // Check if can connect with two vertical lines and one horizontal line
        for (int i = 0; i < COLUMN_COUNT; ++i) {
            if (y1 == i || y2 == i) {
                continue; // Skip if y coordinates are same
            }
            if (std::find(blocks_.begin(), blocks_.end(), Block(int(x1), int(i))) ==
                blocks_.end() &&
                can_connect(Block(int(x1), int(y1)), Block(int(x1), int(i))) &&
                can_connect(Block(int(x1), int(i)), Block(int(x2), int(i))) &&
                std::find(blocks_.begin(), blocks_.end(), Block(int(x2), int(i))) ==
                blocks_.end() &&
                can_connect(Block(int(x2), int(i)), Block(int(x2), int(y2)))) {
                return true; // Can connect
            }
        }

        // Check if can connect with two horizontal lines and one vertical line
        for (int i = 0; i < NUM_COLS; ++i) {
            if (x1 == i || x2 == i) {
                continue; // Skip if x coordinates are same
            }
            if (std::find(blocks_.begin(), blocks_.end(), Block(int(i), int(y1))) ==
                blocks_.end() &&
                can_connect(Block(int(x1), int(y1)), Block(int(i), int(y1))) &&
                can_connect(Block(int(i), int(y1)), Block(int(i), int(y2))) &&
                std::find(blocks_.begin(), blocks_.end(), Block(int(i), int(y2))) ==
                blocks_.end() &&
                can_connect(Block(int(i), int(y2)), Block(int(x2), int(y2)))) {
                return true; // Can connect
            }
        }

        return false; // Cannot connect
    }

    // Check key press state
    bool isKeyPressed(int key) const {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    }

private:
    static constexpr int COLUMN_COUNT = 11;
    static constexpr int NUM_COLS = 19;
    static constexpr uintptr_t START_ADDRESS = 0x199F5C + 4;
    static constexpr uintptr_t END_ADDRESS = 0x19A02C + 4;

    std::vector<Block> blocks_;
    std::vector<std::pair<Block, Block>> blockspair_;
    HANDLE processHandle = nullptr;
    HWND windowHandle = nullptr;
    bool isRunning = false;
};

int main() {
    Link link;
    link.run();
    return 0;
}