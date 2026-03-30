#include <windows.h> 
#include <map>
#include <ctime>   // time()
#include <cstdlib> // rand() и srand()
#include <cstdio> // для конфига
#include <fstream>  // для метода 3 (fstream)
#include <iostream> // для вывода в консоль (задание 3)
#include <chrono>   // для точного замера времени (задание 3)
#include <vector>
#include <iomanip>

UINT WM_GAME_UPDATE = 0;
HANDLE hMapFile = NULL;

// структура конфига (бинарная)
struct ConfigData {
    int N;  // размер клетки
    int windowW; // ширина окна
    int windowH; // длина окна
    COLORREF bgColor; // цвет заднего фона
    int gridColorOffset; // цвет сетки
    int fieldSize;       // количество клеток
};

#define MAX_FIELD_SIZE 100 // максимально допустимое кол-во клеток

struct SharedData {
    int activeWindows;                 // счетчик запущенных копий
    int cells[MAX_FIELD_SIZE][MAX_FIELD_SIZE]; // массив игрового поля (0-пусто 1-круг 2-крест)
    COLORREF bgColor;     // цвет фона
    int gridColorOffset;   // цвет сетки
};

SharedData* g_pSharedData = nullptr;


// глобальный экземпляр конфига (по умолчанию)
ConfigData g_config = { 40, 320, 240, RGB(0, 0, 255), 0, 10};
int g_method = 2; // переменная для хранения метода чтения/записи (по умолчанию - fopen)


COLORREF GetRainbowColor(int offset) {
    BYTE r, g, b;
    // разделяем 256 на 3 фазы
    if (offset < 85) {
        r = 255 - offset * 3;
        g = offset * 3;
        b = 0;
    }
    else if (offset < 170) {
        offset -= 85;
        r = 0;
        g = 255 - offset * 3;
        b = offset * 3;
    }
    else {
        offset -= 170;
        r = offset * 3;
        g = 0;
        b = 255 - offset * 3;
    }
    return RGB(r, g, b); // возвращаем цвет
}

//void SaveConfig() {
//    FILE* f;
//    if (fopen_s(&f, "config.txt", "w") == 0) { // открываем файл для перезаписи
//        fprintf(f, "%d %d %d %u %d", g_config.N, g_config.windowW, g_config.windowH, g_config.bgColor, g_config.gridColorOffset); // записываем числа через пробел
//        fclose(f);
//    }
//}
//
//void LoadConfig() {
//    FILE* f;
//    if (fopen_s(&f, "config.txt", "r") == 0) {
//        fscanf_s(f, "%d %d %d %u %d", &g_config.N, &g_config.windowW, &g_config.windowH, &g_config.bgColor, &g_config.gridColorOffset);
//        fclose(f);
//    }
//}

// 2) - файловые переменные
void SaveConfigMethod2() {
    FILE* f;
    if (fopen_s(&f, "config.bin", "wb") == 0) { // wb - write binary (не строкой)
        fwrite(&g_config, sizeof(ConfigData), 1, f);
        fclose(f);
    }
}

void LoadConfigMethod2(const char* filename, void* buffer, size_t size) {
    FILE* f;
    if (fopen_s(&f, filename, "rb") == 0) { // rb - read binary (не строкой)
        fread(buffer, 1, size, f); // куда, сколько, размер одного, файл
        fclose(f);
    }
}

// 3) - при помощи потоков ввода вывода
void SaveConfigMethod3() {
    std::ofstream f("config.bin", std::ios::binary); // класс потока для записи
    if (f.is_open()) {
        f.write((char*)&g_config, sizeof(ConfigData));
        f.close();
    }
}

void LoadConfigMethod3(const char* filename, void* buffer, size_t size) {
    std::ifstream f(filename, std::ios::binary); // класс потока для чтения
    if (f.is_open()) {
        f.read((char*)buffer, size);
        f.close();
    }
}

// 4) - WinApi
void SaveConfigMethod4() {
    // открываем или создаем файл
    HANDLE hFile = CreateFile(
        L"config.bin",           // имя файла
        GENERIC_WRITE,           // параметр того, что мы хотим писать
        0,                       // не делимся файлом с другими
        NULL,                    // защита
        CREATE_ALWAYS,           // всегда создавать новый
        FILE_ATTRIBUTE_NORMAL,   // обычный файл
        NULL                     // без шаблона
    );

    if (hFile != INVALID_HANDLE_VALUE) { // если файл создался
        DWORD bytesWritten; // сколько байт записали
        // записываем блок памяти
        WriteFile(hFile, &g_config, sizeof(ConfigData), &bytesWritten, NULL);
        CloseHandle(hFile); // закрываем файл
    }
}

void LoadConfigMethod4(const wchar_t* filename, void* buffer, size_t size) {
    HANDLE hFile = CreateFile(
        filename,
        GENERIC_READ,            // параметр того, что мы хотим читать
        FILE_SHARE_READ,         // разрешаем читать другим программам
        NULL,
        OPEN_EXISTING,           // открыть только если файл существует
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) { // если файл отрылся
        DWORD bytesRead; // сколько байт прочитали
        // читаем данные в структуру
        ReadFile(hFile, buffer, (DWORD)size, &bytesRead, NULL);
        CloseHandle(hFile); // закрываем файл
    }
}

// 1) - отображение файла на память
void SaveConfigMethod1() {
    // создаем или открываем файл
    HANDLE hFile = CreateFile(
        L"config.bin",           // имя файла
        GENERIC_READ | GENERIC_WRITE, // параметр того, что мы хотим делать с файлом, т к PAGE_READWRITE
        0,                       // не делимся файлом с другими
        NULL,                    // защита
        CREATE_ALWAYS,           // всегда создавать новый
        FILE_ATTRIBUTE_NORMAL,   // обычный файл
        NULL                     // без шаблона
    );
    if (hFile == INVALID_HANDLE_VALUE) return;

    // создаем объект отображения (файл как оперативная память)
    HANDLE hMap = CreateFileMapping(
        hFile, // сам файл
        NULL, // безопасность
        PAGE_READWRITE, // чтение и запись
        0,  // макс размер старшая часть 
        sizeof(ConfigData), // макс размер младшая часть 
        NULL // имя объекта отображения для других программ
    );

    if (hMap != NULL) {
        // проецируем объект в программу, чтобы мы могли к ним обращаться
        void* pData = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ConfigData)); // 0 0 - смещение

        if (pData != NULL) {
            // копируем данные
            memcpy(pData, &g_config, sizeof(ConfigData));

            // убираем связь указателя с файлом
            UnmapViewOfFile(pData);
        }
        CloseHandle(hMap); // закрываем объект отображения
    }
    CloseHandle(hFile); // закрываем сам файл
}

void LoadConfigMethod1(const wchar_t* filename, void* buffer, size_t size) {
    // открываем существующий файл только для чтения
    HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) return;

    // создаем объект отображения только для чтения
    HANDLE hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

    if (hMap != NULL) {
        // проецируем объект в программу, чтобы мы могли к ним обращаться
        void* pData = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);

        if (pData != NULL) {
            // копируем из файловой памяти в структуру
            memcpy(buffer, pData, size);

            UnmapViewOfFile(pData);
        }
        CloseHandle(hMap);
    }
    CloseHandle(hFile);
}

void RunBenchmark() {
    // открываем консоль
    AllocConsole();

    // привязываем вывод к окну консоли
    FILE* fConsole;
    freopen_s(&fConsole, "CONOUT$", "w", stdout);

    const size_t BIG_SIZE = 1024 * 1024; // 1024 К
    void* bigBuffer = malloc(BIG_SIZE); // выделяем память
    if (!bigBuffer) return;

    // создаем файл для теста
    FILE* fBench;
    if (fopen_s(&fBench, "bench.bin", "wb") == 0) {
        char* junk = (char*)malloc(BIG_SIZE); // мусор чтобы записывать в файл
        memset(junk, 'A', BIG_SIZE); // заполняем A
        fwrite(junk, 1, BIG_SIZE, fBench); // записываем данные в fBench
        fclose(fBench); 
        free(junk);
    }

    printf("%-10s | %-10s | %-15s\n", "Method", "Iter", "Time (ms)");
    printf("---------------------------------------------\n");

    double totalTimes[5] = { 0 }; // хранения суммы времен для методов 1-4

    for (int m = 1; m <= 4; m++) {
        for (int i = 1; i <= 10; i++) {
            auto start = std::chrono::high_resolution_clock::now(); // время начала

            // вызываем функции
            if (m == 1) LoadConfigMethod1(L"bench.bin", bigBuffer, BIG_SIZE);
            else if (m == 2) LoadConfigMethod2("bench.bin", bigBuffer, BIG_SIZE);
            else if (m == 3) LoadConfigMethod3("bench.bin", bigBuffer, BIG_SIZE);
            else if (m == 4) LoadConfigMethod4(L"bench.bin", bigBuffer, BIG_SIZE);

            auto end = std::chrono::high_resolution_clock::now(); // время концп
            std::chrono::duration<double, std::milli> timer = end - start; // milli - в мс ; разница во вермени

            double ms = timer.count(); // чистое число
            totalTimes[m] += ms;

            printf("Method %-3d  | %-10d | %-15.4f ms\n", m, i, ms);
        }
        printf("---------------------------------------------\n");
    }

    // вывод итогов
    for (int m = 1; m <= 4; m++) {
        const char* methodName = "";

        if (m == 1) methodName = "Mapping";
        else if (m == 2) methodName = "fopen";
        else if (m == 3) methodName = "fstream";
        else if (m == 4) methodName = "WinAPI";

        double average = totalTimes[m] / 10.0;

        printf("Method %d (%s): %.4f ms\n", m, methodName, average);
    }

    free(bigBuffer);
}




LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam); // функция обработки событий

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    srand(static_cast<unsigned int>(time(NULL))); // сид для random

    WM_GAME_UPDATE = RegisterWindowMessage(L"UpdateMsg");

    bool runBench = false;
    int val = -1; // значения для N
    int valS = -1; // S кол-во клеток
    int nArgs;
    LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs); // массив строк командной строки, список аргументов
    if (NULL != szArglist) {

        // начинаем с 1, 0 аргумент это имя программы
        for (int i = 1; i < nArgs; i++) {

            // проверяем на флаг метода -m
            if (wcscmp(szArglist[i], L"-m") == 0 && i + 1 < nArgs) {
                g_method = _wtoi(szArglist[i + 1]);
                i++; // пропускаем само число, потому что мы его уже считали
            }
            else if (wcscmp(szArglist[i], L"-bench") == 0) runBench = true; // замер методов
            //  число без флага (N)
            else if (iswdigit(szArglist[i][0])) { // если символ - цифра
                val = _wtoi(szArglist[i]);
            }
            // Флаг -s для количества клеток
            else if (wcscmp(szArglist[i], L"-s") == 0 && i + 1 < nArgs) {
                valS = _wtoi(szArglist[i + 1]);
                i++;
            }
        }
        LocalFree(szArglist);
    }

    if (runBench) {
        RunBenchmark();
    }


    // закгрузили конфиг
    if (g_method == 1) LoadConfigMethod1(L"config.bin", &g_config, sizeof(ConfigData));
    else if (g_method == 2) LoadConfigMethod2("config.bin", &g_config, sizeof(ConfigData));
    else if (g_method == 3) LoadConfigMethod3("config.bin", &g_config, sizeof(ConfigData));
    else if (g_method == 4) LoadConfigMethod4(L"config.bin", &g_config, sizeof(ConfigData));
    else LoadConfigMethod2("config.bin", &g_config, sizeof(ConfigData)); // если указали неверный способ

    if (val != -1)
    {
        if (val < 30) val = 30; // порог
        g_config.N = val;
    }

    if (valS != -1)
    {
        if (valS < 3) valS = 3;
        else if (valS > MAX_FIELD_SIZE)
        {
            valS = MAX_FIELD_SIZE;
        }
        g_config.fieldSize = valS;
    }

    // создаем (открываем) объект в памяти
    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // используем файл подкачки (не создает реальный файл)
        NULL,                    // защита по умолчанию
        PAGE_READWRITE,          // чтение и запись
        0,
        sizeof(SharedData),
        L"Local\\SharedMem" 
    );

    if (hMapFile == NULL) {
        return 1;
    }

    // были ли мы первыми
    bool isFirst = (GetLastError() != ERROR_ALREADY_EXISTS);

    // получаем указатель на эту память
    g_pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));

    if (g_pSharedData == NULL) {
        CloseHandle(hMapFile);
        return 1;
    }

    // логика если окно - первое
    if (isFirst) {
        g_pSharedData->activeWindows = 1;
        memset(g_pSharedData->cells, 0, sizeof(g_pSharedData->cells)); // очищаем поле
        g_pSharedData->bgColor = g_config.bgColor; // записываем цвет фона в общую память
        g_pSharedData->gridColorOffset = g_config.gridColorOffset; // записываем стеки в общую память
    }
    else {
        g_pSharedData->activeWindows++;
        // обновляем данные конфига
        g_config.bgColor = g_pSharedData->bgColor;
        g_config.gridColorOffset = g_pSharedData->gridColorOffset;
    }

    const wchar_t CLASS_NAME[] = L"MyWinAPIClass";

    WNDCLASS wc = { }; // создали шаблон
    wc.lpfnWndProc = WindowProc; // какая функция обрабатывает события из окна
    wc.hInstance = hInstance;
    wc.style = CS_HREDRAW | CS_VREDRAW; // чтобы окно перерисовывалось при изменении его размеров
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(g_config.bgColor);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx( // создали окно
        0,
        CLASS_NAME, // имя шаблона
        L"Lab 1",
        WS_OVERLAPPEDWINDOW,            // стиль окна
        CW_USEDEFAULT, CW_USEDEFAULT,   // x y где создать окно
        g_config.windowW, g_config.windowH,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow); // nCmdShow - параметр, как показать окно

    
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);  // перенаправляет сообщения в функию обработки
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_GAME_UPDATE) {
        // синхронизируем конфиг
        g_config.bgColor = g_pSharedData->bgColor;
        g_config.gridColorOffset = g_pSharedData->gridColorOffset;

        // обновляем кисть фона
        HBRUSH hNewBrush = CreateSolidBrush(g_config.bgColor);
        HBRUSH hOldBrush = (HBRUSH)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hNewBrush);
        if (hOldBrush) DeleteObject(hOldBrush);

        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps; // хранит инфу какую часть холста нужно перерисовывать
        HDC hdc = BeginPaint(hwnd, &ps); // получаем холст

        RECT rect;
        GetClientRect(hwnd, &rect); // узнали рамер окна, в котором рисуем

        COLORREF gridColor = GetRainbowColor(g_pSharedData->gridColorOffset); // получаем цвет
        HPEN hLinePen = CreatePen(PS_SOLID, 5, gridColor); // создали кисть для клеток
        HPEN hOldPen = (HPEN)SelectObject(hdc, hLinePen); // взяли кисть
        int width = rect.right; // ширина холста
        int height = rect.bottom; // длина холста
        int cellSize = g_config.N; // размер клетки

        int fSize = g_config.fieldSize;

        // рисуем сеткупо количеству клеток
        for (int i = 0; i <= fSize; i++) {
            // вертикальные
            MoveToEx(hdc, i * cellSize, 0, NULL);
            LineTo(hdc, i * cellSize, fSize * cellSize);
            // горизонтальные
            MoveToEx(hdc, 0, i * cellSize, NULL);
            LineTo(hdc, fSize * cellSize, i * cellSize);
        }

        // рисуем фигуры из массива
        for (int y = 0; y < fSize; y++) {
            for (int x = 0; x < fSize; x++) {
                int type = g_pSharedData->cells[x][y];
                if (type == 0) continue;

                int left = x * cellSize;
                int top = y * cellSize;
                int right = left + cellSize;
                int bottom = top + cellSize;

                HPEN hWhitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                HPEN hTempPen = (HPEN)SelectObject(hdc, hWhitePen);

                if (type == 1) Ellipse(hdc, left + 5, top + 5, right - 5, bottom - 5);
                else if (type == 2) {
                    MoveToEx(hdc, left + 5, top + 5, NULL);
                    LineTo(hdc, right - 5, bottom - 5);
                    MoveToEx(hdc, right - 5, top + 5, NULL);
                    LineTo(hdc, left + 5, bottom - 5);
                }
                SelectObject(hdc, hTempPen);
                DeleteObject(hWhitePen);
            }
        }

        SelectObject(hdc, hOldPen); // вернули системное перо
        DeleteObject(hLinePen);

        EndPaint(hwnd, &ps);
        return 0;

    }
    case WM_LBUTTONDOWN: // круг
    case WM_RBUTTONDOWN: { // крестик
        int mouseX = LOWORD(lParam); // x
        int mouseY = HIWORD(lParam); // y

        // переводим пиксели в индексы ячеек
        int cellX = mouseX / g_config.N;
        int cellY = mouseY / g_config.N;

        // тип фигуры
        int type;
        if (uMsg == WM_LBUTTONDOWN) type = 1;
        else type = 2;

        // проверяем что клик попал в границы игрового поля
        if (cellX >= 0 && cellX < g_config.fieldSize && cellY >= 0 && cellY < g_config.fieldSize) {
            g_pSharedData->cells[cellX][cellY] = type;
            PostMessage(HWND_BROADCAST, WM_GAME_UPDATE, 0, 0); // оповещаем окна в системе
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    case WM_KEYDOWN: {

        if (wParam == VK_ESCAPE) { // нажатие esc
            DestroyWindow(hwnd);
            return 0;
        }

        if (wParam == 'Q') {
            if (GetKeyState(VK_CONTROL) < 0) { // нажат ли сtrl, если < 0, то зажат
                DestroyWindow(hwnd);
                return 0;
            }
        }

        if (wParam == 'C') {
            if (GetKeyState(VK_SHIFT) < 0) {
                STARTUPINFO si; // инфа по запуску окна
                PROCESS_INFORMATION pi;  // данные о процессе

                ZeroMemory(&si, sizeof(si)); // очищаем, чтобы параметры были по умолчанию
                si.cb = sizeof(si); // указываем размер структуры
                ZeroMemory(&pi, sizeof(pi));

                // создаем процесс
                if (CreateProcess(L"C:\\Windows\\System32\\notepad.exe",
                    NULL,           // аргументы командной строки
                    NULL,           // аргументы безопасности процесса
                    NULL,           // аргументы безопасности потока
                    FALSE,          // наследование дескрипторов
                    0,              // флаги создания
                    NULL,           // окружение
                    NULL,           // каталог
                    &si,            // указатель на STARTUPINFO
                    &pi))           // указатель на PROCESS_INFORMATION
                {
                    // закрываем дескрипторы, убираем связь нашего приложения с блокнотом
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                }
                else {
                    // Если не получилось (например, путь неверный)
                    MessageBox(hwnd, L"Не удалось запустить Блокнот", L"Ошибка", MB_OK | MB_ICONERROR);
                }
            }
            return 0;
        }

        if (wParam == VK_RETURN)
        {
            int r = rand() % 256;
            int g = rand() % 256;
            int b = rand() % 256;
            g_pSharedData->bgColor = RGB(r, g, b);

            PostMessage(HWND_BROADCAST, WM_GAME_UPDATE, 0, 0);

            return 0;
        }
        return 0;
    }
    case WM_DESTROY:
    {
        RECT rect;
        GetWindowRect(hwnd, &rect);
        g_config.windowW = rect.right - rect.left;
        g_config.windowH = rect.bottom - rect.top;


        if (g_pSharedData) {
            g_pSharedData->activeWindows--; // если окон больше нет, система сама удалит Mapping

            if (g_pSharedData->activeWindows == 0)
            {
                g_config.bgColor = g_pSharedData->bgColor;
                g_config.gridColorOffset = g_pSharedData->gridColorOffset;

                // сохраняем конгфиг
                if (g_method == 1) SaveConfigMethod1();
                else if (g_method == 2) SaveConfigMethod2();
                else if (g_method == 3) SaveConfigMethod3();
                else if (g_method == 4) SaveConfigMethod4();
            }
            UnmapViewOfFile(g_pSharedData);
        }


        if (hMapFile != NULL) {
            CloseHandle(hMapFile);
            hMapFile = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam); // значение прокрутки колеса

        if (zDelta > 0) {
            g_pSharedData->gridColorOffset = (g_pSharedData->gridColorOffset + 15) % 256;
        }
        else {
            g_pSharedData->gridColorOffset = (g_pSharedData->gridColorOffset - 15 + 256) % 256;
        }

        PostMessage(HWND_BROADCAST, WM_GAME_UPDATE, 0, 0);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam); // авто обработка остальных действий
    }
    return 0;
}

