#include <windows.h>
#include <stdio.h>
#include <string.h>

#define main console_calculator_main
#include "scientific_calculator.c"
#undef main

#define IDC_DISPLAY 1001
#define IDC_HISTORY 1002
#define IDC_STATUS 1003
#define IDC_MODE 1004
#define IDC_ANGLE 1005
#define IDC_BUTTON_BASE 1100

static HWND display_box;
static HWND history_box;
static HWND status_label;
static HWND mode_button;
static HWND angle_button;
static double app_answer;
static double app_memory;
static CalculatorMode app_mode = MODE_SCIENTIFIC;
static AngleMode app_angle = ANGLE_DEGREES;
static HBRUSH white_brush;
static HBRUSH panel_brush;
static HBRUSH blue_brush;
static HBRUSH soft_blue_brush;
static HFONT display_font;
static HFONT heading_font;
static HFONT button_font;

static void set_status(const char *text)
{
    SetWindowTextA(status_label, text);
}

static void set_display(const char *text)
{
    SetWindowTextA(display_box, text);
    SendMessageA(display_box, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
}

static void append_display(const char *text)
{
    int length = GetWindowTextLengthA(display_box);
    SendMessageA(display_box, EM_SETSEL, (WPARAM)length, (LPARAM)length);
    SendMessageA(display_box, EM_REPLACESEL, FALSE, (LPARAM)text);
}

static void load_history(void)
{
    FILE *file = fopen(HISTORY_FILE, "r");
    char line[HISTORY_LINE_SIZE];

    if (file == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' ||
                              line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        SendMessageA(history_box, LB_ADDSTRING, 0, (LPARAM)line);
    }
    fclose(file);
}

static void calculate(void)
{
    char expression[INPUT_SIZE];
    char error[128];
    char result_text[64];
    double result;

    GetWindowTextA(display_box, expression, sizeof(expression));
    if (expression[0] == '\0') {
        set_status("Type an expression first.");
        return;
    }
    if (!evaluate(expression, app_memory, app_angle, &result,
                  error, sizeof(error))) {
        set_status(error);
        return;
    }
    app_answer = result;
    snprintf(result_text, sizeof(result_text), "%.15g", result);
    set_display(result_text);
    save_history(expression, result);
    SendMessageA(history_box, LB_ADDSTRING, 0,
                 (LPARAM)(snprintf(result_text, sizeof(result_text),
                                   "%s = %.15g", expression, result),
                          result_text));
    set_status("Calculation saved to history.");
}

static void clear_history_list(void)
{
    clear_history();
    SendMessageA(history_box, LB_RESETCONTENT, 0, 0);
    set_status("History cleared.");
}

static void update_mode_buttons(void)
{
    SetWindowTextA(mode_button,
                   app_mode == MODE_SCIENTIFIC ? "Scientific mode"
                                                : "Standard mode");
    SetWindowTextA(angle_button,
                   app_angle == ANGLE_DEGREES ? "Degrees"
                                               : "Radians");
}

static HWND make_button(HWND parent, const char *text, int id,
                        int x, int y, int width, int height)
{
    HWND button = CreateWindowExA(0, "BUTTON", text,
                           WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON |
                               BS_OWNERDRAW,
                           x, y, width, height, parent, (HMENU)(INT_PTR)id,
                           GetModuleHandleA(NULL), NULL);
    SendMessageA(button, WM_SETFONT, (WPARAM)button_font, TRUE);
    return button;
}

static void draw_button(const DRAWITEMSTRUCT *item)
{
    HBRUSH brush;
    COLORREF text_color;
    char text[64];
    RECT rect = item->rcItem;
    int id = (int)item->CtlID;
    int inset = 2;

    if (id == 1200) {
        brush = blue_brush;
        text_color = RGB(255, 255, 255);
    } else if (id == IDC_MODE || id == IDC_ANGLE ||
               (id >= 1300 && id <= 1400)) {
        brush = soft_blue_brush;
        text_color = RGB(24, 84, 125);
    } else if ((id >= IDC_BUTTON_BASE + 3 && id <= IDC_BUTTON_BASE + 5) ||
               (id >= IDC_BUTTON_BASE + 9 && id <= IDC_BUTTON_BASE + 11) ||
               (id >= IDC_BUTTON_BASE + 15 && id <= IDC_BUTTON_BASE + 17) ||
               (id >= IDC_BUTTON_BASE + 21 && id <= IDC_BUTTON_BASE + 23)) {
        brush = soft_blue_brush;
        text_color = RGB(24, 84, 125);
    } else {
        brush = white_brush;
        text_color = RGB(35, 45, 60);
    }

    if (item->itemState & ODS_SELECTED) {
        inset = 3;
    }
    InflateRect(&rect, -inset, -inset);
    FillRect(item->hDC, &item->rcItem, panel_brush);
    SelectObject(item->hDC, brush);
    RoundRect(item->hDC, rect.left, rect.top, rect.right, rect.bottom, 14, 14);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text_color);
    GetWindowTextA(item->hwndItem, text, sizeof(text));
    DrawTextA(item->hDC, text, -1, &rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void create_controls(HWND window)
{
    static const char *button_texts[] = {
        "7", "8", "9", "/", "sin(", "cos(",
        "4", "5", "6", "*", "tan(", "sqrt(",
        "1", "2", "3", "-", "log(", "ln(",
        "0", ".", "(", ")", "^", "!",
        "pi", "e", "ans", "+", "abs(", "exp("
    };
    int index;
    int row;
    int column;

    display_box = CreateWindowExA(0, "EDIT", "",
                                  WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL |
                                      ES_RIGHT | ES_WANTRETURN,
                                  24, 26, 490, 44, window,
                                  (HMENU)(INT_PTR)IDC_DISPLAY,
                                  GetModuleHandleA(NULL), NULL);
    SetWindowRgn(display_box, CreateRoundRectRgn(0, 0, 490, 44, 16, 16),
                 TRUE);
    SendMessageA(display_box, WM_SETFONT, (WPARAM)display_font, TRUE);

    mode_button = make_button(window, "Scientific mode", IDC_MODE,
                              540, 24, 120, 28);
    angle_button = make_button(window, "Degrees", IDC_ANGLE,
                               665, 24, 100, 28);
    make_button(window, "=", 1200, 540, 58, 225, 48);

    for (index = 0; index < 30; index++) {
        row = index / 6;
        column = index % 6;
        make_button(window, button_texts[index], IDC_BUTTON_BASE + index,
                    24 + column * 84, 100 + row * 50, 76, 42);
    }

    make_button(window, "C", 1300, 24, 360, 76, 40);
    make_button(window, "Backspace", 1301, 108, 360, 160, 40);
    make_button(window, "M+", 1302, 276, 360, 76, 40);
    make_button(window, "M-", 1303, 360, 360, 76, 40);
    make_button(window, "MR", 1304, 444, 360, 76, 40);
    make_button(window, "MC", 1305, 528, 360, 76, 40);
    make_button(window, "Clear history", 1400, 612, 360, 153, 40);

    HWND history_heading = CreateWindowExA(0, "STATIC", "Saved history",
                                           WS_VISIBLE | WS_CHILD,
                                           540, 125, 225, 24, window, NULL,
                                           GetModuleHandleA(NULL), NULL);
    SendMessageA(history_heading, WM_SETFONT, (WPARAM)heading_font, TRUE);
    history_box = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                                  WS_VISIBLE | WS_CHILD | WS_VSCROLL |
                                      LBS_NOINTEGRALHEIGHT,
                                  540, 150, 225, 200, window,
                                  (HMENU)(INT_PTR)IDC_HISTORY,
                                  GetModuleHandleA(NULL), NULL);
    SendMessageA(history_box, WM_SETFONT, (WPARAM)button_font, TRUE);
    status_label = CreateWindowExA(0, "STATIC",
                                   "Ready. Type or click an expression.",
                                   WS_VISIBLE | WS_CHILD,
                                   24, 420, 741, 28, window,
                                   (HMENU)(INT_PTR)IDC_STATUS,
                                   GetModuleHandleA(NULL), NULL);
    SendMessageA(status_label, WM_SETFONT, (WPARAM)button_font, TRUE);
    load_history();
}

static void handle_button(int id)
{
    static const char *button_texts[] = {
        "7", "8", "9", "/", "sin(", "cos(",
        "4", "5", "6", "*", "tan(", "sqrt(",
        "1", "2", "3", "-", "log(", "ln(",
        "0", ".", "(", ")", "^", "!",
        "pi", "e", "ans", "+", "abs(", "exp("
    };
    char memory_text[64];

    if (id >= IDC_BUTTON_BASE && id < IDC_BUTTON_BASE + 30) {
        append_display(button_texts[id - IDC_BUTTON_BASE]);
        return;
    }
    if (id == 1200) {
        calculate();
    } else if (id == 1300) {
        set_display("");
        set_status("Display cleared.");
    } else if (id == 1301) {
        int length = GetWindowTextLengthA(display_box);
        if (length > 0) {
            SendMessageA(display_box, EM_SETSEL, length - 1, length);
            SendMessageA(display_box, EM_REPLACESEL, FALSE, (LPARAM)"");
        }
    } else if (id == 1302 || id == 1303) {
        app_memory += id == 1302 ? app_answer : -app_answer;
        snprintf(memory_text, sizeof(memory_text), "Memory: %.15g", app_memory);
        set_status(memory_text);
    } else if (id == 1304) {
        snprintf(memory_text, sizeof(memory_text), "%.15g", app_memory);
        set_display(memory_text);
        set_status("Memory recalled.");
    } else if (id == 1305) {
        app_memory = 0.0;
        set_status("Memory cleared.");
    } else if (id == IDC_MODE) {
        app_mode = app_mode == MODE_STANDARD ? MODE_SCIENTIFIC : MODE_STANDARD;
        update_mode_buttons();
        set_status("Calculator mode changed.");
    } else if (id == IDC_ANGLE) {
        app_angle = app_angle == ANGLE_DEGREES ? ANGLE_RADIANS : ANGLE_DEGREES;
        update_mode_buttons();
        set_status("Angle mode changed.");
    } else if (id == 1400) {
        clear_history_list();
    }
}

static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                    WPARAM w_param, LPARAM l_param)
{
    switch (message) {
    case WM_CREATE:
        create_controls(window);
        update_mode_buttons();
        return 0;
    case WM_DRAWITEM:
        draw_button((const DRAWITEMSTRUCT *)l_param);
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(w_param) == BN_CLICKED) {
            handle_button(LOWORD(w_param));
        }
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetBkColor((HDC)w_param, RGB(255, 255, 255));
        SetTextColor((HDC)w_param, RGB(35, 45, 60));
        return (LRESULT)white_brush;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        DeleteObject(white_brush);
        DeleteObject(panel_brush);
        DeleteObject(blue_brush);
        DeleteObject(soft_blue_brush);
        DeleteObject(display_font);
        DeleteObject(heading_font);
        DeleteObject(button_font);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(window, message, w_param, l_param);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   LPSTR command_line, int show_command)
{
    WNDCLASSA window_class = {0};
    HWND window;
    MSG message;

    (void)previous;
    (void)command_line;
    white_brush = CreateSolidBrush(RGB(255, 255, 255));
    panel_brush = CreateSolidBrush(RGB(241, 247, 252));
    blue_brush = CreateSolidBrush(RGB(55, 145, 210));
    soft_blue_brush = CreateSolidBrush(RGB(222, 241, 252));
    display_font = CreateFontA(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    heading_font = CreateFontA(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    button_font = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = "ScientificCalculatorApp";
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = panel_brush;
    RegisterClassA(&window_class);

    window = CreateWindowExA(0, window_class.lpszClassName,
                             "Simple Scientific Calculator",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                                 WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT, 810, 510,
                             NULL, NULL, instance, NULL);
    if (window == NULL) {
        return 1;
    }
    ShowWindow(window, show_command);
    UpdateWindow(window);
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    return (int)message.wParam;
}
