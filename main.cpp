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

    // Add new board representation
    struct GameBoard {
        static constexpr int ROWS = 11;
        static constexpr int COLS = 19;
        std::vector<std::vector<unsigned char>> board;
        
        GameBoard() : board(ROWS, std::vector<unsigned char>(COLS, 0)) {}
        
        unsigned char& at(int y, int x) { 
            return board[y][x]; 
        }
        
        bool isValid(int y, int x) const {
            return y >= 0 && y < ROWS && x >= 0 && x < COLS;
        }
        
        bool isEmpty(int y, int x) const {
            return board[y][x] == 0;
        }
    };
    
    GameBoard gameBoard;

    // Optimized matching algorithm
    void find_matching_coordinates() {
        blockspair_.clear();
        
        // Sort blocks by value for faster matching
        std::unordered_map<unsigned char, std::vector<Point>> valueMap;
        for (const auto& block : blocks_) {
            valueMap[block.value].push_back(block.point);
        }

        // Process each value group
        for (const auto& [value, points] : valueMap) {
            findBestMatches(points);
        }
    }

    // Find optimal matches for a group of same-value points
    void findBestMatches(const std::vector<Point>& points) {
        std::vector<bool> matched(points.size(), false);
        
        // Process points from left to right and top to bottom
        for (size_t i = 0; i < points.size(); i++) {
            if (matched[i]) continue;
            
            // Find the best match for current point
            int bestMatchIndex = -1;
            int minTurns = 3;  // Maximum possible turns
            int minDistance = INT_MAX;
            
            for (size_t j = i + 1; j < points.size(); j++) {
                if (matched[j]) continue;
                
                int turns;
                if (canConnect(points[i], points[j], turns)) {
                    int distance = calculateDistance(points[i], points[j]);
                    // Prefer matches with fewer turns and shorter distance
                    if (turns < minTurns || (turns == minTurns && distance < minDistance)) {
                        minTurns = turns;
                        minDistance = distance;
                        bestMatchIndex = j;
                    }
                }
            }
            
            // Add the best match if found
            if (bestMatchIndex != -1) {
                Block b1(points[i].x, points[i].y);
                Block b2(points[bestMatchIndex].x, points[bestMatchIndex].y);
                b1.value = b2.value = gameBoard.at(points[i].y, points[i].x);
                blockspair_.emplace_back(b1, b2);
                matched[i] = matched[bestMatchIndex] = true;
            }
        }
    }

    // Calculate Manhattan distance between two points
    int calculateDistance(const Point& p1, const Point& p2) {
        return std::abs(p2.x - p1.x) + std::abs(p2.y - p1.y);
    }

    // Optimized connection check
    bool canConnect(const Point& p1, const Point& p2, int& turns) {
        // Direct line check
        if (p1.x == p2.x || p1.y == p2.y) {
            if (checkStraightLine(p1, p2)) {
                turns = 0;
                return true;
            }
            return false;
        }

        // One turn check
        if (checkOneTurn(p1, p2)) {
            turns = 1;
            return true;
        }

        // Two turns check
        if (checkTwoTurns(p1, p2)) {
            turns = 2;
            return true;
        }

        return false;
    }

    bool checkStraightLine(const Point& p1, const Point& p2) {
        int minX = std::min<int>(p1.x, p2.x);
        int maxX = std::max<int>(p1.x, p2.x);
        int minY = std::min<int>(p1.y, p2.y);
        int maxY = std::max<int>(p1.y, p2.y);

        // Check horizontal line
        if (p1.y == p2.y) {
            for (int x = minX + 1; x < maxX; x++) {
                if (!gameBoard.isEmpty(p1.y, x)) return false;
            }
            return true;
        }

        // Check vertical line
        if (p1.x == p2.x) {
            for (int y = minY + 1; y < maxY; y++) {
                if (!gameBoard.isEmpty(y, p1.x)) return false;
            }
            return true;
        }

        return false;
    }

    bool checkOneTurn(const Point& p1, const Point& p2) {
        // Check turn at (p1.x, p2.y)
        if (gameBoard.isEmpty(p2.y, p1.x) &&
            checkStraightLine(p1, Point(p1.x, p2.y)) &&
            checkStraightLine(Point(p1.x, p2.y), p2)) {
            return true;
        }

        // Check turn at (p2.x, p1.y)
        if (gameBoard.isEmpty(p1.y, p2.x) &&
            checkStraightLine(p1, Point(p2.x, p1.y)) &&
            checkStraightLine(Point(p2.x, p1.y), p2)) {
            return true;
        }

        return false;
    }

    bool checkTwoTurns(const Point& p1, const Point& p2) {
        // Check all possible middle points
        for (int x = 0; x < NUM_COLS; x++) {
            if (x != p1.x && x != p2.x &&
                gameBoard.isEmpty(p1.y, x) && gameBoard.isEmpty(p2.y, x) &&
                checkStraightLine(p1, Point(x, p1.y)) &&
                checkStraightLine(Point(x, p1.y), Point(x, p2.y)) &&
                checkStraightLine(Point(x, p2.y), p2)) {
                return true;
            }
        }

        for (int y = 0; y < COLUMN_COUNT; y++) {
            if (y != p1.y && y != p2.y &&
                gameBoard.isEmpty(y, p1.x) && gameBoard.isEmpty(y, p2.x) &&
                checkStraightLine(p1, Point(p1.x, y)) &&
                checkStraightLine(Point(p1.x, y), Point(p2.x, y)) &&
                checkStraightLine(Point(p2.x, y), p2)) {
                return true;
            }
        }

        return false;
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