#include <GLFW/glfw3.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_internal.h"

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;

#define KILOBYTES(number) ((number) * 1024ull)
#define MEGABYTES(number) (KILOBYTES(number) * 1024ull)

#define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)

template <class T>
T *CALLOC_OR_DIE(size_t count) {
    auto ptr = (T *)calloc(count, sizeof(T));
    if (ptr == NULL) {
        assert(0 && "Failed to allocate memory using calloc\n");
        exit(1);
    }

    return ptr;
}

#define DEFAULT_ARENA_ALIGNMENT (2 * sizeof(void *))

struct ArenaAllocator {
    u8 *_buf;
    size_t _buf_len;
    size_t _curr_offset;

    void Init(size_t size) {
        _buf = CALLOC_OR_DIE<u8>(size);
        _buf_len = size;
        _curr_offset = 0;
    }

    template <class T>
    T *Alloc(size_t count, size_t align = DEFAULT_ARENA_ALIGNMENT) {
        uintptr_t curr_ptr = (uintptr_t)_buf + (uintptr_t)_curr_offset;

        uintptr_t offset = curr_ptr;
        uintptr_t a = (uintptr_t)align;
        uintptr_t modulo = offset & (a - 1);
        if (modulo != 0) {
            offset += a - modulo;
        }

        offset -= (uintptr_t)_buf;

        size_t size = sizeof(T) * count;

        if (offset + size <= _buf_len) {
            T *ptr = (T *)&_buf[offset];
            _curr_offset = offset + size;

            memset(ptr, 0, size);
            return ptr;
        }

        return NULL;
    }

    void Free() {
        _curr_offset = 0;
    }
};

template <class T>
struct ArenaArray {
    T *_array;
    size_t _length;
    size_t _capacity;

    void AddMany(T *items, size_t item_count, ArenaAllocator *arena) {
        if (_length + item_count > _capacity) {
            if (_capacity == 0) {
                _capacity = _length + item_count;
            } else {
                _capacity *= 2;
            }

            auto prev_items = _array;
            _array = arena->Alloc<T>(_capacity);
            memcpy(_array, prev_items, _length * sizeof(T));
        }

        memcpy(_array + _length, items, item_count * sizeof(T));
        _length += item_count;
    }

    void Add(T item, ArenaAllocator *arena) {
        AddMany(&item, 1, arena);
    }

    i32 Length() { return _length; }

    T &operator[](size_t index) {
        assert(index < _length);
        return _array[index];
    }
};

static const float WINDOW_W = 400;
static const float WINDOW_H = 400;

static const ImVec4 CLEAR_COLOR = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

struct Window {
    char *title;
    HWND handle;
};

struct Windows {
    ArenaArray<Window> list;
    ArenaAllocator *arena;
};

struct Monitor {
    int x;
    int y;
    int w;
    int h;
};

static BOOL CALLBACK EnumWindowsCallback(HWND window_handle, LPARAM l_param) {
    auto windows = (Windows *)l_param;

    if (!IsWindowVisible(window_handle)) {
        return TRUE;
    }

    int length = GetWindowTextLength(window_handle);
    if (length == 0) {
        return TRUE;
    }

    auto title = windows->arena->Alloc<char>(length + 1);
    GetWindowText(window_handle, title, length + 1);

    Window window = {0};
    window.title = title;
    window.handle = window_handle;

    windows->list.Add(window, windows->arena);

    return TRUE;
}

static int CompareWindows(const void *a, const void *b) {
    auto win_a = (Window *)a;
    auto win_b = (Window *)b;

    int first_a = tolower(win_a->title[0]);
    int first_b = tolower(win_b->title[0]);

    return first_a - first_b;
}

Windows GetWindows(ArenaAllocator *arena) {
    Windows windows = {0};
    windows.arena = arena;

    EnumWindows(EnumWindowsCallback, (LPARAM)&windows);

    qsort(windows.list._array, windows.list.Length(), sizeof(Window), CompareWindows);

    ArenaArray<char *> window_titles = {0};
    for (int i = 0; i < windows.list.Length(); i++) {
        window_titles.Add(windows.list[i].title, arena);
    }

    return windows;
}

Monitor GetMonitor(HMONITOR monitor_handle) {
    MONITORINFOEX mi;
    mi.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(monitor_handle, &mi);

    Monitor monitor = {0};

    monitor.x = mi.rcMonitor.left;
    monitor.y = mi.rcMonitor.top;
    monitor.w = mi.rcMonitor.right - mi.rcMonitor.left;
    monitor.h = mi.rcMonitor.bottom - mi.rcMonitor.top;

    return monitor;
}

void MakeBorderless(HWND window_handle) {
    // Set style
    LONG new_style = GetWindowLongPtr(window_handle, GWL_STYLE) & ~(WS_CAPTION | WS_THICKFRAME);
    SetWindowLongA(window_handle, GWL_STYLE, new_style);

    // Set position/size
    HMONITOR monitor_handle = MonitorFromWindow(window_handle, MONITOR_DEFAULTTOPRIMARY);
    Monitor monitor = GetMonitor(monitor_handle);

    int target_w = monitor.w;
    int target_h = monitor.h;

    int target_x = monitor.x;
    int target_y = monitor.y;

    MoveWindow(window_handle, target_x, target_y, target_w, target_h, true);
    SetWindowPos(window_handle, NULL, target_x, target_y, target_w, target_h, SWP_NOZORDER | SWP_FRAMECHANGED);
}

HWND DrawWindowList(Windows *windows, HWND selected_handle) {
    ImGui::BeginChild("WindowList", {ImGui::GetContentRegionAvail().x, WINDOW_H * 0.9f});

    HWND clicked_window_handle = 0;

    for (int i = 0; i < windows->list.Length(); i++) {
        ImGui::PushID(i);

        Window window = windows->list[i];
        if (ImGui::Selectable(window.title, window.handle == selected_handle)) {
            clicked_window_handle = window.handle;
        };

        ImGui::PopID();
    }

    ImGui::EndChild();

    return clicked_window_handle;
}

static void GLFWErrorCallback(int error, const char *description) {
    LOG_ERROR("GLFW Error %d: %s\n", error, description);
}

GLFWwindow *InitWindow() {
    glfwSetErrorCallback(GLFWErrorCallback);

    if (!glfwInit()) {
        LOG_ERROR("failed to initialize GLFW\n");
        return NULL;
    }

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow *window = glfwCreateWindow(WINDOW_W, WINDOW_H, "borderlesser", NULL, NULL);
    if (window == NULL) {
        LOG_ERROR("failed to create GLFW window\n");
        return NULL;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::GetIO().IniFilename = NULL;
    ImGui::GetIO().LogFilename = NULL;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return window;
}

#ifndef _DEBUG

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#endif

int main() {
    ArenaAllocator arena = {0};
    arena.Init(MEGABYTES(8));

    Windows windows = GetWindows(&arena);
    HWND selected_handle = NULL;

    auto window = InitWindow();

    double last_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("borderlesser", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

        HWND clicked_window_handle = DrawWindowList(&windows, selected_handle);
        if (clicked_window_handle != NULL) {
            selected_handle = clicked_window_handle;
        }

        if (selected_handle != NULL) {
            if (ImGui::Button("Make Borderless", ImGui::GetContentRegionAvail())) {
                MakeBorderless(selected_handle);
            }
        } else {
            ImGui::BeginDisabled();
            ImGui::Button("Make Borderless", ImGui::GetContentRegionAvail());
            ImGui::EndDisabled();
        }

        ImGui::End();

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(CLEAR_COLOR.x * CLEAR_COLOR.w, CLEAR_COLOR.y * CLEAR_COLOR.w, CLEAR_COLOR.z * CLEAR_COLOR.w,
                     CLEAR_COLOR.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        glfwPollEvents();

        double curr_time = glfwGetTime();
        if (curr_time > last_time + 1.0f) {
            arena.Free();
            windows = GetWindows(&arena);
            last_time = curr_time;
        }
    }

    return 0;
}