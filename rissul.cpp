#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <limits>

// --- OS 감지 및 헤더 설정 ---
#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <unistd.h>
#endif

// --- 게임 설정 상수 ---
const int MAP_HEIGHT = 14;
const int MAP_WIDTH = 40;
const int STARTING_QUOTA = 5;
const int QUOTA_CYCLE_DAYS = 3;

class Map;
class Player;
class Monster;

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void sleepMs(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

char getInput() {
    char input;
#ifdef _WIN32
    input = _getch();
#else
    std::cout << " > ";
    std::cin >> input;
#endif
    return tolower(input);
}

// 윈도우용 엔터 대기 처리 함수
void waitForKeyWindows() {
#ifdef _WIN32
    while (_kbhit()) _getch(); // 버퍼 비우기
    _getch(); // 키 입력 대기
#endif
}

class Monster {
public:
    int x, y;
    char symbol;
    Monster(int startX, int startY, char sym) : x(startX), y(startY), symbol(sym) {}
    virtual ~Monster() {}
    virtual void update(Player& player, Map& map) = 0;
};

class Player {
public:
    int x, y, hp, scrapInBag;
    Player(int startX, int startY) : x(startX), y(startY), hp(100), scrapInBag(0) {}
    bool isAlive() { return hp > 0; }
    void takeDamage(int amount) { hp = (hp - amount < 0) ? 0 : hp - amount; }
    void move(char input, Map& map);
};

class Map {
public:
    char grid[MAP_HEIGHT][MAP_WIDTH];
    std::vector<Monster*> monsters;
    int startX, startY;

    Map() : startX(1), startY(1) {}
    ~Map() { for (auto m : monsters) delete m; monsters.clear(); }

    bool isWall(int x, int y) {
        if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return true;
        return grid[y][x] == '#';
    }
    char getChar(int x, int y) { return (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) ? ' ' : grid[y][x]; }
    void setChar(int x, int y, char c) { if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) grid[y][x] = c; }
    
    void updateMonsters(Player& player);
    bool load(std::string filename);
    void render(Player& player) {
        for (int y = 0; y < MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAP_WIDTH; ++x) {
                if (x == player.x && y == player.y) std::cout << '@';
                else std::cout << grid[y][x];
            }
            std::cout << std::endl;
        }
    }
};

class Stalker : public Monster {
public:
    Stalker(int startX, int startY) : Monster(startX, startY, 'M') {}
    void update(Player& player, Map& map) override {
        int dx = player.x - x, dy = player.y - y;
        int dist = std::abs(dx) + std::abs(dy);

        if (dist < 6 && dist > 0) {
            int newX = x, newY = y;
            if (std::abs(dx) > std::abs(dy)) newX += (dx > 0) ? 1 : -1;
            else newY += (dy > 0) ? 1 : -1;

            if (!map.isWall(newX, newY)) {
                if (newX == player.x && newY == player.y) player.takeDamage(25);
                else if (map.getChar(newX, newY) == '.') {
                    map.setChar(x, y, '.'); x = newX; y = newY; map.setChar(x, y, symbol);
                }
            }
        } else {
            if (rand() % 2 == 0) return;
            int dir = rand() % 4;
            int newX = x, newY = y;
            if (dir == 0) newY--; else if (dir == 1) newY++; else if (dir == 2) newX--; else newX++;

            if (!map.isWall(newX, newY) && map.getChar(newX, newY) == '.') {
                map.setChar(x, y, '.'); x = newX; y = newY; map.setChar(x, y, symbol);
            }
        }
    }
};

bool Map::load(std::string filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    for (auto m : monsters) delete m; monsters.clear();
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        std::string line;
        if (!std::getline(file, line)) break;
        for (int x = 0; x < MAP_WIDTH; ++x) {
            if (x < line.length()) {
                grid[y][x] = line[x];
                if (line[x] == 'E') { startX = x; startY = y; }
                else if (line[x] == 'M') monsters.push_back(new Stalker(x, y));
            } else grid[y][x] = ' ';
        }
    }
    return true;
}
void Map::updateMonsters(Player& player) { for (auto m : monsters) m->update(player, *this); }
void Player::move(char input, Map& map) {
    int newX = x, newY = y;
    if (input == 'w') newY--; else if (input == 's') newY++; else if (input == 'a') newX--; else if (input == 'd') newX++;
    if (!map.isWall(newX, newY)) {
        x = newX; y = newY;
        if (map.getChar(x, y) == '$') { scrapInBag++; map.setChar(x, y, '.'); }
    }
}

class Game {
    Player player; Map map;
    int day, quota, totalScrap;
    bool gameRunning;

    void waitForKey() {
        std::cout << "\nPress Enter to continue..." << std::endl;
#ifdef _WIN32
        waitForKeyWindows();
#else
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::string dummy; std::getline(std::cin, dummy);
#endif
    }

    void displayIntro() {
        clearScreen();
        std::cout << "Welcome to 'Scrap Hunter' (Integrated Version)" << std::endl;
        std::cout << "Controls: W, A, S, D" << std::endl;
        std::cout << "          'e': return, 'q': quit" << std::endl;
        waitForKey();
    }

    void startDay() {
        std::string mapFiles[] = { "map1.txt", "map2.txt", "map3.txt" };
        std::string selectedMap = mapFiles[rand() % 3];
        std::cout << "Loading " << selectedMap << "..." << std::endl;
        sleepMs(1000);
        
        if (!map.load(selectedMap)) { gameRunning = false; return; }
        player.x = map.startX; player.y = map.startY; player.scrapInBag = 0;

        while (gameRunning && player.isAlive()) {
            clearScreen();
            std::cout << "Day: " << day << " | Quota: " << totalScrap << "/" << quota << std::endl;
            std::cout << "HP: " << player.hp << " | Scrap: " << player.scrapInBag << std::endl;
            map.render(player);
            std::cout << "Command (w/a/s/d/e/q): ";
            
            char input = getInput();
            if (input == 'q') { gameRunning = false; return; }
            if (input == 'e' && player.x == map.startX && player.y == map.startY) {
                totalScrap += player.scrapInBag;
                clearScreen(); std::cout << "Day Ended. Scraps saved." << std::endl; sleepMs(2000);
                return;
            }
            if (input == 'w' || input == 'a' || input == 's' || input == 'd') player.move(input, map);
            map.updateMonsters(player);

            if (!player.isAlive()) {
                clearScreen(); std::cout << "YOU DIED." << std::endl; 
                player.scrapInBag = 0; player.hp = 100; sleepMs(3000);
            }
            sleepMs(50);
        }
    }

public:
    Game() : player(1, 1), day(1), quota(STARTING_QUOTA), totalScrap(0), gameRunning(true) {}
    void run() {
        displayIntro();
        while (gameRunning) {
            if ((day - 1) % QUOTA_CYCLE_DAYS == 0 && day > 1) {
                if (totalScrap >= quota) {
                    clearScreen(); std::cout << "QUOTA MET!" << std::endl;
                    totalScrap = 0; quota = (int)(quota * 1.5) + 2; sleepMs(3000);
                } else {
                    clearScreen(); std::cout << "FIRED." << std::endl; break;
                }
            }
            startDay(); day++;
        }
        std::cout << "Game Over." << std::endl;
    }
};

int main() { srand(time(NULL)); Game game; game.run(); return 0; }
