#include <Windows.h>
#include <algorithm>

#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>

// 简化Point结构体
struct Point {
    int x = 0, y = 0;
    Point(int _x = 0, int _y = 0) : x(_x), y(_y) {}
};

// 简化Block结构体
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
            std::cout << "程序已启动，按F2暂停" << std::endl;
            Sleep(200);
        }
        else if (isKeyPressed(VK_F2) && isRunning) {
            isRunning = false;
            std::cout << "程序已暂停，按F1继续" << std::endl;
            Sleep(200);
        }
    }

    bool initializeGame() {
        if (!readWindow()) {
            std::cout << "读取进程失败" << std::endl;
            return false;
        }
        return true;
    }

    void runGameLoop() {
        while (isRunning) {
            if (isKeyPressed(VK_F2)) {
                isRunning = false;
                std::cout << "程序已暂停，按F1继续" << std::endl;
                Sleep(200);
                break;
            }

            if (!processGameState()) break;
            Sleep(1);
        }
    }

    bool processGameState() {
        if (!readBlocksFromProcessMemory()) {
            std::cout << "读取内存失败" << std::endl;
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
            std::cout << "未找到指定进程" << std::endl;
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

        // 按值分组
        for (const auto& block : blocks_) {
            valueToBlocksMap[block.value].push_back(block);
        }

        // 查找匹配
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

    // 读取进程内存中的方块数据
    bool readBlocksFromProcessMemory() {
        if (processHandle == nullptr) {
            std::cout << "无法获取进程句柄" << std::endl;
            return false;
        }
        blocks_.clear();
        // 使用智能指针管理内存
        size_t regionSize = END_ADDRESS - START_ADDRESS + 1;
        std::unique_ptr<unsigned char[]> buffer(new unsigned char[regionSize]);

        // 获取内存
        SIZE_T bytesRead;
        if (!ReadProcessMemory(processHandle,
            reinterpret_cast<LPCVOID>(START_ADDRESS), buffer.get(),
            regionSize, &bytesRead)) {
            return false;
        }

        // 处理内存数据
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
        // 获取进程窗口的位置信息
        RECT rect;
        GetWindowRect(windowHandle, &rect);
        int windowWidth = rect.right - rect.left;
        int windowHeight = rect.bottom - rect.top;

        // 计算窗口中心坐标
        int topleft_x = rect.left + 25;
        int topleft_y = rect.top + 195;

        // 将鼠标移动到进程窗口的中心坐标
        topleft_x += (index_x * 31);
        topleft_y += (index_y * 35);

        SetCursorPos(topleft_x, topleft_y); // 设置鼠标位置

        // 模拟鼠标左键按下和释放
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    }

    void eiminationBox() {
        SetWindowPos(windowHandle, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE); // 将窗口置于最上层

        for (auto& x : blockspair_) {
            click(x.first.point.x, x.first.point.y); // 点击第一个方块
            click(x.second.point.x, x.second.point.y); // 点击第二个方块
            Sleep(1); // 可选：在点击之间添加延迟
        }
    }

    bool can_connect(const Block& coord1, const Block& coord2) {
        int x1 = coord1.point.x, y1 = coord1.point.y;
        int x2 = coord2.point.x, y2 = coord2.point.y;

        // 检查是否在同一行
        if (y1 == y2) {
            for (int x = std::min<int>(int(x1), int(x2)) + 1;
                x < std::max<int>(int(x1), int(x2)); ++x) {
                if (std::find(blocks_.begin(), blocks_.end(), Block(x, y1)) !=
                    blocks_.end())
                    break; // 如果中间有方块，不能连接
            }
            return true; // 可以连接
        }

        // 检查是否在同一列
        if (x1 == x2) {
            for (int y = std::min<int>(int(y1), int(y2)) + 1;
                y < std::max<int>(int(y1), int(y2)); ++y) {
                if (std::find(blocks_.begin(), blocks_.end(), Block(x1, y)) !=
                    blocks_.end())
                    break; // 如果中间有方块，不能连接
            }
            return true; // 可以连接
        }

        // 判断直角
        if (std::find(blocks_.begin(), blocks_.end(), Block(int(x1), int(y2))) ==
            blocks_.end() &&
            can_connect(Block(int(x1), int(y1)), Block(int(x1), int(y2))) &&
            can_connect(Block(int(x1), int(y2)), Block(int(x2), int(y2)))) {
            return true; // 可以连接
        }

        if (std::find(blocks_.begin(), blocks_.end(), Block(int(x2), int(y1))) ==
            blocks_.end() &&
            can_connect(Block(int(x1), int(y1)), Block(int(x2), int(y1))) &&
            can_connect(Block(int(x2), int(y1)), Block(int(x2), int(y2)))) {
            return true; // 可以连接
        }

        // 判断两个竖线，一个横线能否连通
        for (int i = 0; i < COLUMN_COUNT; ++i) {
            if (y1 == i || y2 == i) {
                continue; // 如果y坐标相同，跳过
            }
            if (std::find(blocks_.begin(), blocks_.end(), Block(int(x1), int(i))) ==
                blocks_.end() &&
                can_connect(Block(int(x1), int(y1)), Block(int(x1), int(i))) &&
                can_connect(Block(int(x1), int(i)), Block(int(x2), int(i))) &&
                std::find(blocks_.begin(), blocks_.end(), Block(int(x2), int(i))) ==
                blocks_.end() &&
                can_connect(Block(int(x2), int(i)), Block(int(x2), int(y2)))) {
                return true; // 可以连接
            }
        }

        // 判断两个横线，一个竖线能否连通
        for (int i = 0; i < NUM_COLS; ++i) {
            if (x1 == i || x2 == i) {
                continue; // 如果x坐标相同，跳过
            }
            if (std::find(blocks_.begin(), blocks_.end(), Block(int(i), int(y1))) ==
                blocks_.end() &&
                can_connect(Block(int(x1), int(y1)), Block(int(i), int(y1))) &&
                can_connect(Block(int(i), int(y1)), Block(int(i), int(y2))) &&
                std::find(blocks_.begin(), blocks_.end(), Block(int(i), int(y2))) ==
                blocks_.end() &&
                can_connect(Block(int(i), int(y2)), Block(int(x2), int(y2)))) {
                return true; // 可以连接
            }
        }

        return false; // 不能连接
    }

    // 检查按键状态
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