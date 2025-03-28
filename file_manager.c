#include <ncurses.h>
#include <panel.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>


#define MAX_FILES 1000
#define MAX_PATH 1024

typedef struct {
    char name[256];
    int is_dir;
    off_t size;
    time_t mtime;
} FileItem;

typedef struct {
    WINDOW *win;
    PANEL *panel;
    char current_path[MAX_PATH];
    FileItem files[MAX_FILES];
    int file_count;
    int selected_idx;
    int start_idx;
    int active;
} FilePanel;

// Khai báo prototype
void init_colors();
void init_panel(FilePanel *p, int height, int width, int y, int x, const char *path);
void read_directory(FilePanel *p);
void display_panel(FilePanel *p);
void display_bottom_menu();
void handle_key(int key, FilePanel *left, FilePanel *right, FilePanel **active);

int main() {
    // Khởi tạo ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_colors();
    curs_set(0);
    
    // Lấy kích thước màn hình
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Tính toán kích thước
    int panel_height = max_y - 4; // Để dành 1 dòng cho header và 3 dòng cho footer
    int panel_width = max_x / 2;
    
    // Tạo hai panel
    FilePanel left_panel, right_panel;
    init_panel(&left_panel, panel_height, panel_width, 1, 0, ".");
    init_panel(&right_panel, panel_height, panel_width, 1, panel_width, ".");
    
    left_panel.active = 1;
    right_panel.active = 0;
    FilePanel *active_panel = &left_panel;
    
    // Vẽ header menu
    attron(COLOR_PAIR(3));
    mvhline(0, 0, ' ', max_x);
    mvprintw(0, 2, "Left");
    mvprintw(0, 20, "File");
    mvprintw(0, 35, "Command");
    mvprintw(0, 50, "Options");
    mvprintw(0, 65, "Right");
    attroff(COLOR_PAIR(3));
    
    // Hiển thị panel và menu
    update_panels();
    display_panel(&left_panel);
    display_panel(&right_panel);
    display_bottom_menu();
    doupdate();
    
    // Vòng lặp chính
    int ch;
    while ((ch = getch()) != 'q' && ch != KEY_F(10) && ch != KEY_F(9)) {
        handle_key(ch, &left_panel, &right_panel, &active_panel);
        
        display_panel(&left_panel);
        display_panel(&right_panel);
        display_bottom_menu();
        
        doupdate();
    }
    
    endwin();
    return 0;
}

void init_colors() {
    init_pair(1, COLOR_WHITE, COLOR_BLUE);    // Panel bình thường
    init_pair(2, COLOR_BLACK, COLOR_CYAN);    // Panel được chọn
    init_pair(3, COLOR_BLACK, COLOR_CYAN);    // Header menu
    init_pair(4, COLOR_BLACK, COLOR_CYAN);    // Footer menu và nút thường
    init_pair(5, COLOR_YELLOW, COLOR_BLUE);   // Thư mục
    init_pair(6, COLOR_BLACK, COLOR_YELLOW);  // Nút được chọn trong dialog thường
    init_pair(7, COLOR_WHITE, COLOR_RED);     // Màu nền đỏ cho dialog delete
}


void init_panel(FilePanel *p, int height, int width, int y, int x, const char *path) {
    p->win = newwin(height, width, y, x);
    p->panel = new_panel(p->win);
    
    wbkgd(p->win, COLOR_PAIR(1));
    box(p->win, 0, 0);
    
    strcpy(p->current_path, path);
    p->selected_idx = 0;
    p->start_idx = 0;
    p->file_count = 0;
    
    read_directory(p);
}

void read_directory(FilePanel *p) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[MAX_PATH];
    
    p->file_count = 0;
    
    // Thêm ".." để quay lại thư mục cha
    strcpy(p->files[p->file_count].name, "..");
    p->files[p->file_count].is_dir = 1;
    p->files[p->file_count].size = 4096;
    p->files[p->file_count].mtime = time(NULL);
    p->file_count++;
    
    if ((dir = opendir(p->current_path)) == NULL) {
        mvwprintw(p->win, 1, 1, "Không thể mở thư mục!");
        return;
    }
    
    while ((entry = readdir(dir)) != NULL && p->file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
            
        strcpy(p->files[p->file_count].name, entry->d_name);
        
        snprintf(full_path, MAX_PATH, "%s/%s", p->current_path, entry->d_name);
        if (stat(full_path, &st) == 0) {
            p->files[p->file_count].is_dir = S_ISDIR(st.st_mode);
            p->files[p->file_count].size = st.st_size;
            p->files[p->file_count].mtime = st.st_mtime;
        }
        
        p->file_count++;
    }
    
    closedir(dir);
}

void display_panel(FilePanel *p) {
    int i;
    int height, width;
    struct tm *timeinfo;
    char date_str[20];
    
    werase(p->win);
    wbkgd(p->win, COLOR_PAIR(p->active ? 2 : 1));
    box(p->win, 0, 0);
    
    getmaxyx(p->win, height, width);
    
    // Header cho danh sách file
    mvwprintw(p->win, 1, 2, "Name");
    mvwprintw(p->win, 1, width - 32, "Size");
    mvwprintw(p->win, 1, width - 16, "Modify time");
    
    // Hiển thị file
    int display_count = height - 3; // Để trừ header và border
    
    // Đảm bảo start_idx không vượt quá giới hạn
    if (p->file_count > display_count) {
        if (p->start_idx > p->file_count - display_count)
            p->start_idx = p->file_count - display_count;
    } else {
        p->start_idx = 0;
    }
    
    // Đảm bảo selected_idx luôn nằm trong vùng hiển thị
    if (p->selected_idx < p->start_idx)
        p->start_idx = p->selected_idx;
    if (p->selected_idx >= p->start_idx + display_count)
        p->start_idx = p->selected_idx - display_count + 1;
    
    for (i = 0; i < display_count && i + p->start_idx < p->file_count; i++) {
        FileItem *file = &p->files[i + p->start_idx];
        timeinfo = localtime(&file->mtime);
        strftime(date_str, 20, "%b %d %H:%M", timeinfo);
        
        // Highlight file được chọn
        if (i + p->start_idx == p->selected_idx)
            wattron(p->win, A_REVERSE);
            
        // Với thư mục, sử dụng màu đặc biệt
        if (file->is_dir)
            wattron(p->win, COLOR_PAIR(5));
            
        mvwprintw(p->win, i + 2, 2, "%-15s", file->name);
        
        if (strcmp(file->name, "..") == 0)
            mvwprintw(p->win, i + 2, width - 32, "UP--DIR");
        else
            mvwprintw(p->win, i + 2, width - 32, "%4ldK", file->size / 1024);
            
        mvwprintw(p->win, i + 2, width - 16, "%s", date_str);
        
        if (file->is_dir)
            wattroff(p->win, COLOR_PAIR(5));
            
        if (i + p->start_idx == p->selected_idx)
            wattroff(p->win, A_REVERSE);
    }
    
    // Vẽ thanh cuộn nếu cần
    if (p->file_count > display_count) {
        int scrollbar_height = height - 2;
        
        // Tính toán vị trí thanh cuộn
        double ratio = (double)p->start_idx / (p->file_count - display_count);
        int scrollbar_pos = 1 + (int)(ratio * (scrollbar_height - 1));
        
        // Tính toán kích thước thanh cuộn
        int scrollbar_size = (display_count * scrollbar_height) / p->file_count;
        if (scrollbar_size < 1) scrollbar_size = 1;
        if (scrollbar_pos + scrollbar_size > scrollbar_height)
            scrollbar_size = scrollbar_height - scrollbar_pos + 1;
        
        // Vẽ đường dẫn thanh cuộn
        for (i = 1; i <= scrollbar_height; i++) {
            mvwaddch(p->win, i, width - 1, ACS_VLINE);
        }
        
        // Vẽ thanh cuộn
        wattron(p->win, A_REVERSE);
        for (i = 0; i < scrollbar_size && scrollbar_pos + i <= scrollbar_height; i++) {
            mvwaddch(p->win, scrollbar_pos + i, width - 1, ' ');
        }
        wattroff(p->win, A_REVERSE);
    }
    
    // Hiển thị đường dẫn hiện tại ở dưới panel
    mvwprintw(p->win, height - 1, 2, "%s", p->current_path);
    
    wrefresh(p->win);
}



void display_bottom_menu() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Vẽ thanh trạng thái và menu
    attron(COLOR_PAIR(4));
    mvhline(max_y - 2, 0, ' ', max_x);
    mvhline(max_y - 1, 0, ' ', max_x);
    
    mvprintw(max_y - 1, 1, "F1 Help");
    mvprintw(max_y - 1, 10, "F2 Menu");
    mvprintw(max_y - 1, 19, "F3 View");
    mvprintw(max_y - 1, 28, "F4 Edit");
    mvprintw(max_y - 1, 37, "F5 Copy");
    mvprintw(max_y - 1, 46, "F6 Move");
    mvprintw(max_y - 1, 55, "F7 Mkdir");
    mvprintw(max_y - 1, 65, "F8 Delete");
    mvprintw(max_y - 1, 76, "F9 Quit");
    
    attroff(COLOR_PAIR(4));
    refresh();
}
// Hàm hiển thị hộp thoại tạo thư mục
WINDOW *create_dialog_window(int height, int width, int y, int x, const char *title) {
    WINDOW *win = newwin(height, width, y, x);
    wbkgd(win, COLOR_PAIR(3));
    box(win, 0, 0);
    mvwprintw(win, 0, (width - strlen(title)) / 2, "%s", title);
    wrefresh(win);
    return win;
}

void handle_mkdir(FilePanel *p) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Kích thước và vị trí của hộp thoại
    int dialog_height = 10;
    int dialog_width = 50;
    int dialog_y = (max_y - dialog_height) / 2;
    int dialog_x = (max_x - dialog_width) / 2;
    
    // Tạo hộp thoại
    WINDOW *dialog = newwin(dialog_height, dialog_width, dialog_y, dialog_x);
    wbkgd(dialog, COLOR_PAIR(3));  // Màu nền xanh nhạt
    box(dialog, 0, 0);
    
    // Tiêu đề hộp thoại
    mvwprintw(dialog, 0, (dialog_width - 17) / 2, " Make directory");
    
    // Hiển thị nhãn
    mvwprintw(dialog, 3, 3, "Nhập tên thư mục:");
    
    // Tạo ô nhập liệu
    WINDOW *input_win = newwin(1, 34, dialog_y + 5, dialog_x + 8);
    wbkgd(input_win, COLOR_PAIR(2));  // Màu khác để nổi bật
    box(input_win, 0, 0);
    
    // Tạo nút OK
    WINDOW *ok_button = newwin(3, 10, dialog_y + dialog_height - 4, dialog_x + 10);
    wbkgd(ok_button, COLOR_PAIR(4));  // Màu nút bình thường
    box(ok_button, 0, 0);
    mvwprintw(ok_button, 1, 3, "Ok");
    
    // Tạo nút Cancel
    WINDOW *cancel_button = newwin(3, 10, dialog_y + dialog_height - 4, dialog_x + 30);
    wbkgd(cancel_button, COLOR_PAIR(4));  // Màu nút bình thường
    box(cancel_button, 0, 0);
    mvwprintw(cancel_button, 1, 2, "Cancel");
    
    // Hiển thị dialog và các nút
    wrefresh(dialog);
    wrefresh(input_win);
    wrefresh(ok_button);
    wrefresh(cancel_button);
    
    // Chuẩn bị cho việc nhập liệu
    char dirname[256] = {0};
    int pos = 0;
    int ch;
    int max_input = 32;
    
    // Trạng thái focus: 0 = nhập liệu, 1 = nút OK, 2 = nút Cancel
    int focus_state = 0;
    
    // Khởi tạo focus ban đầu tại ô nhập liệu
    curs_set(1);  // Hiện con trỏ
    keypad(input_win, TRUE);
    keypad(dialog, TRUE);
    wmove(input_win, 0, 0);
    wrefresh(input_win);
    
    // Vòng lặp xử lý nhập liệu và điều hướng
    while (1) {
        if (focus_state == 0) {
            // Đang ở chế độ nhập liệu
            wmove(input_win, 0, pos);
            wrefresh(input_win);
            ch = wgetch(input_win);
            
            if (ch == '\n') {
                // Enter tại ô nhập liệu = OK
                if (strlen(dirname) > 0) {
                    // Tạo thư mục
                    char new_dir_path[MAX_PATH];
                    snprintf(new_dir_path, MAX_PATH, "%s/%s", p->current_path, dirname);
                    
                    if (mkdir(new_dir_path, 0755) == 0) {
                        read_directory(p);
                        break;
                    } else {
                        mvwprintw(dialog, dialog_height - 2, 2, "Error: Can not create directory!");
                        wrefresh(dialog);
                        napms(1500);
                    }
                }
            } else if (ch == KEY_DOWN) {
                // Chuyển xuống nút OK
                focus_state = 1;
                curs_set(0);  // Ẩn con trỏ
                
                // Tô vàng nút OK
                wbkgd(ok_button, COLOR_PAIR(5));  // Màu vàng cho nút được chọn
                wrefresh(ok_button);
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (pos > 0) {
                    // Xóa ký tự
                    pos--;
                    dirname[pos] = '\0';
                    mvwaddch(input_win, 0, pos, ' ');
                    wmove(input_win, 0, pos);
                    wrefresh(input_win);
                }
            } else if (ch == 27) {  // ESC
                // Hủy và thoát
                break;
            } else if (pos < max_input && ch >= 32 && ch <= 126) {
                // Nhập ký tự bình thường
                dirname[pos++] = ch;
                dirname[pos] = '\0';
                waddch(input_win, ch);
                wrefresh(input_win);
            }
        } else if (focus_state == 1) {
            // Đang ở nút OK
            ch = wgetch(dialog);
            
            if (ch == '\n') {
                // Enter tại nút OK
                if (strlen(dirname) > 0) {
                    // Tạo thư mục
                    char new_dir_path[MAX_PATH];
                    snprintf(new_dir_path, MAX_PATH, "%s/%s", p->current_path, dirname);
                    
                    if (mkdir(new_dir_path, 0755) == 0) {
                        read_directory(p);
                        break;
                    } else {
                        mvwprintw(dialog, dialog_height - 2, 2, "Lỗi: Không thể tạo thư mục!");
                        wrefresh(dialog);
                        napms(1500);
                    }
                }
            } else if (ch == KEY_UP) {
                // Chuyển lên ô nhập liệu
                focus_state = 0;
                curs_set(1);  // Hiển thị con trỏ nhập liệu
                
                // Bỏ tô vàng nút OK
                wbkgd(ok_button, COLOR_PAIR(4));  // Màu mặc định cho nút
                wrefresh(ok_button);
                
                // Focus lại ô nhập liệu
                wmove(input_win, 0, pos);
                wrefresh(input_win);
            } else if (ch == KEY_RIGHT) {
                // Chuyển sang nút Cancel
                focus_state = 2;
                
                // Đổi màu các nút
                wbkgd(ok_button, COLOR_PAIR(4));  // Màu mặc định
                wbkgd(cancel_button, COLOR_PAIR(5));  // Màu vàng
                wrefresh(ok_button);
                wrefresh(cancel_button);
            } else if (ch == 27) {  // ESC
                // Hủy và thoát
                break;
            }
        } else if (focus_state == 2) {
            // Đang ở nút Cancel
            ch = wgetch(dialog);
            
            if (ch == '\n' || ch == 27) {
                // Enter hoặc ESC tại nút Cancel = thoát
                break;
            } else if (ch == KEY_UP) {
                // Chuyển lên ô nhập liệu
                focus_state = 0;
                curs_set(1);  // Hiển thị con trỏ nhập liệu
                
                // Bỏ tô vàng nút Cancel
                wbkgd(cancel_button, COLOR_PAIR(4));  // Màu mặc định cho nút
                wrefresh(cancel_button);
                
                // Focus lại ô nhập liệu
                wmove(input_win, 0, pos);
                wrefresh(input_win);
            } else if (ch == KEY_LEFT) {
                // Chuyển sang nút OK
                focus_state = 1;
                
                // Đổi màu các nút
                wbkgd(cancel_button, COLOR_PAIR(4));  // Màu mặc định
                wbkgd(ok_button, COLOR_PAIR(5));  // Màu vàng
                wrefresh(cancel_button);
                wrefresh(ok_button);
            }
        }
    }
    
    // Dọn dẹp
    curs_set(0);  // Ẩn con trỏ
    delwin(input_win);
    delwin(ok_button);
    delwin(cancel_button);
    delwin(dialog);
    touchwin(stdscr);
    refresh();
    
    // Vẽ lại panel
    display_panel(p);
}
void handle_delete(FilePanel *p) {
    if (p->selected_idx < 0 || p->selected_idx >= p->file_count)
        return;
        
    // Bỏ qua trường hợp ".."
    if (strcmp(p->files[p->selected_idx].name, "..") == 0)
        return;
        
    FileItem *selected_file = &p->files[p->selected_idx];
    int is_dir = selected_file->is_dir;
    
    // Lấy kích thước màn hình
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Kích thước và vị trí hộp thoại
    int dialog_height = 6;
    int dialog_width = 40;
    int dialog_y = (max_y - dialog_height) / 2;
    int dialog_x = (max_x - dialog_width) / 2;
    
    // Tạo hộp thoại màu đỏ
    WINDOW *dialog = newwin(dialog_height, dialog_width, dialog_y, dialog_x);
    wbkgd(dialog, COLOR_PAIR(7));  // Màu nền đỏ
    box(dialog, 0, 0);
    
    // Tiêu đề hộp thoại
    mvwprintw(dialog, 0, (dialog_width - 6) / 2, " Delete ");
    
    // Thông báo xác nhận xóa
    char message[256];
    if (is_dir) {
        snprintf(message, sizeof(message), "Delete directory \"%s\"?", selected_file->name);
    } else {
        snprintf(message, sizeof(message), "Delete file \"%s\"?", selected_file->name);
    }
    mvwprintw(dialog, 2, (dialog_width - strlen(message)) / 2, "%s", message);
    
    // Tạo nút Yes
    WINDOW *yes_button = newwin(1, 7, dialog_y + 4, dialog_x + 10);
    wbkgd(yes_button, COLOR_PAIR(7));  // Màu nền đỏ
    
    // Tạo nút No
    WINDOW *no_button = newwin(1, 6, dialog_y + 4, dialog_x + 24);
    wbkgd(no_button, COLOR_PAIR(7));  // Màu nền đỏ
    
    // Trạng thái focus: 0 = Yes, 1 = No
    int focus_state = 1;  // Mặc định focus vào No (an toàn hơn)
    
    // Hiển thị nút với focus ban đầu
    wattron(yes_button, A_REVERSE);
    wattron(no_button, A_REVERSE);
    
    if (focus_state == 0) {
        wattroff(no_button, A_REVERSE);
    } else {
        wattroff(yes_button, A_REVERSE);
    }
    
    // Vẽ nội dung nút
    mvwprintw(yes_button, 0, 1, " Yes ");
    mvwprintw(no_button, 0, 1, " No ");
    
    // Hiển thị hộp thoại và các nút
    wrefresh(dialog);
    wrefresh(yes_button);
    wrefresh(no_button);
    
    // Xử lý phím
    keypad(dialog, TRUE);
    int ch;
    
    while (1) {
        ch = wgetch(dialog);
        
        if (ch == KEY_LEFT || ch == KEY_RIGHT) {
            // Chuyển đổi focus giữa hai nút
            focus_state = !focus_state;
            
            // Cập nhật hiển thị nút
            if (focus_state == 0) {
                wattron(yes_button, A_REVERSE);
                wattroff(no_button, A_REVERSE);
            } else {
                wattroff(yes_button, A_REVERSE);
                wattron(no_button, A_REVERSE);
            }
            
            // Vẽ lại nội dung nút
            mvwprintw(yes_button, 0, 1, " Yes ");
            mvwprintw(no_button, 0, 1, " No ");
            
            wrefresh(yes_button);
            wrefresh(no_button);
        } else if (ch == '\n') {
            // Enter = xác nhận lựa chọn hiện tại
            if (focus_state == 0) {  // Chọn Yes
                // Xóa thư mục hoặc file
                char path[MAX_PATH];
                snprintf(path, MAX_PATH, "%s/%s", p->current_path, selected_file->name);
                
                int delete_success = 0;
                if (is_dir) {
                    delete_success = rmdir(path) == 0;
                } else {
                    delete_success = unlink(path) == 0;
                }
                
                if (!delete_success) {
                    // Hiển thị thông báo lỗi
                    if (is_dir) {
                        mvwprintw(dialog, 4, 5, "Error: Directory not empty!");
                    } else {
                        mvwprintw(dialog, 4, 8, "Error: Cannot delete file!");
                    }
                    wrefresh(dialog);
                    napms(1500);
                } else {
                    // Cập nhật lại danh sách file
                    read_directory(p);
                }
                break;
            } else {  // Chọn No
                break;
            }
        } else if (ch == 27) {  // ESC = Cancel
            break;
        }
    }
    
    // Dọn dẹp
    delwin(yes_button);
    delwin(no_button);
    delwin(dialog);
    touchwin(stdscr);
    refresh();
    
    // Vẽ lại panel
    display_panel(p);
}


void handle_key(int key, FilePanel *left, FilePanel *right, FilePanel **active) {
    FilePanel *p = *active;
    int height, width;
    getmaxyx(p->win, height, width);
    int display_count = height - 3; // Số file có thể hiển thị trong panel
    
    switch(key) {
        case KEY_UP:
            if (p->selected_idx > 0) {
                p->selected_idx--;
                if (p->selected_idx < p->start_idx)
                    p->start_idx = p->selected_idx;
            }
            break;
                
        case KEY_DOWN:
            if (p->selected_idx < p->file_count - 1) {
                p->selected_idx++;
                if (p->selected_idx >= p->start_idx + display_count)
                    p->start_idx = p->selected_idx - display_count + 1;
                if (p->start_idx > p->file_count - display_count && p->file_count > display_count)
                    p->start_idx = p->file_count - display_count;
            }
            break;
        
        case KEY_NPAGE: // Page Down
            p->selected_idx += display_count;
            if (p->selected_idx >= p->file_count)
                p->selected_idx = p->file_count - 1;
            if (p->selected_idx >= p->start_idx + display_count)
                p->start_idx = p->selected_idx - display_count + 1;
            if (p->start_idx > p->file_count - display_count && p->file_count > display_count)
                p->start_idx = p->file_count - display_count;
            if (p->start_idx < 0)
                p->start_idx = 0;
            break;
            
        case KEY_PPAGE: // Page Up
            p->selected_idx -= display_count;
            if (p->selected_idx < 0)
                p->selected_idx = 0;
            if (p->selected_idx < p->start_idx)
                p->start_idx = p->selected_idx;
            break;
                
        case '\t':  // Tab để chuyển panel
            if (p == left) {
                left->active = 0;
                right->active = 1;
                *active = right;
            } else {
                right->active = 0;
                left->active = 1;
                *active = left;
            }
            break;
                
        case '\n':  // Enter để vào thư mục
            if (p->files[p->selected_idx].is_dir) {
                if (strcmp(p->files[p->selected_idx].name, "..") == 0) {
                    // Xử lý đường dẫn "."
                    if (strcmp(p->current_path, ".") == 0) {
                        // Lấy đường dẫn đầy đủ
                        char full_path[MAX_PATH];
                        getcwd(full_path, sizeof(full_path));
                        
                        // Lấy thư mục cha
                        char *last_slash = strrchr(full_path, '/');
                        if (last_slash != NULL && last_slash != full_path)
                            *last_slash = '\0';
                        else
                            strcpy(full_path, "/");
                            
                        strcpy(p->current_path, full_path);
                    } 
                    // Xử lý các đường dẫn khác
                    else {
                        // Tìm dấu "/" cuối cùng
                        char *last_slash = strrchr(p->current_path, '/');
                        
                        // Đường dẫn là "/"
                        if (strcmp(p->current_path, "/") == 0) {
                            // Giữ nguyên "/"
                        }
                        // Đường dẫn "/something" -> "/"
                        else if (last_slash == p->current_path) {
                            strcpy(p->current_path, "/");
                        }
                        // Cắt sau dấu "/" cuối
                        else if (last_slash != NULL) {
                            *last_slash = '\0';
                        } 
                        // Không có dấu "/" (đường dẫn tương đối)
                        else {
                            getcwd(p->current_path, MAX_PATH);
                        }
                    }
                } else {
                    // Xử lý vào thư mục thông thường
                    char new_path[MAX_PATH];
                    
                    // Nếu đường dẫn hiện tại kết thúc bằng "/", không thêm "/"
                    if (p->current_path[strlen(p->current_path)-1] == '/')
                        snprintf(new_path, MAX_PATH, "%s%s", 
                                p->current_path, p->files[p->selected_idx].name);
                    else
                        snprintf(new_path, MAX_PATH, "%s/%s", 
                                p->current_path, p->files[p->selected_idx].name);
                                
                    strcpy(p->current_path, new_path);
                }
                
                // Đặt lại vị trí và đọc thư mục mới
                p->selected_idx = 0;
                p->start_idx = 0;
                read_directory(p);
            }
            break;

        
        case KEY_F(1):
            // Xử lý F1: Hiển thị trợ giúp (chưa triển khai chi tiết)
            break;
        
        case KEY_F(2):
            // Xử lý F2: Mở menu (chưa triển khai chi tiết)
            break;
        
        case KEY_F(3):
            // Xử lý F3: Xem tệp (chưa triển khai chi tiết)
            break;
        
        case KEY_F(4):
            // Xử lý F4: Chỉnh sửa (chưa triển khai chi tiết)
            break;
        
        case KEY_F(5):
            // Xử lý F5: Sao chép (chưa triển khai chi tiết)
            break;
        
        case KEY_F(6):
            // Xử lý F6: Di chuyển (chưa triển khai chi tiết)
            break;
        
        case KEY_F(7):
            handle_mkdir(p);
            // Xử lý F7: Tạo thư mục (chưa triển khai chi tiết)
            break;
        
        case KEY_F(8):
            handle_delete(p);
            // Xử lý F8: Xóa (chưa triển khai chi tiết)
            break;
        
        case KEY_F(9):
            // Xử lý F9: Thoát (đã được xử lý trong main)
            break;
    }
}



