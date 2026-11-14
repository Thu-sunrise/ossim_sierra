# OS Simulation Project (CO2018 - Operating Systems)

> **Môn học**: Hệ điều hành (CO2018) – Trường Đại học Bách Khoa ĐHQG-HCM (HCMUT)  
> **Phiên bản mô phỏng**: Sierra Release  
> **Ngôn ngữ**: C (POSIX Threads / Multithreading)

---

## 📌 1. Giới thiệu tổng quan (Overview)

Dự án này là một chương trình mô phỏng các cơ chế cốt lõi của một hệ điều hành đa nhiệm (Multitasking Operating System) và đa nhân (Multicore CPU). Hệ thống được lập trình bằng ngôn ngữ C, sử dụng thư viện `pthread` để giả lập sự vận hành song song của nhiều nhân CPU, một bộ nạp tiến trình (Loader), và một bộ định thời (Timer) điều phối nhịp thời gian rời rạc (discrete time slots).

### Các tính năng cốt lõi được mô phỏng:
1. **Quản lý & Điều phối CPU (CPU & Process Management)**:
   - Mô phỏng thực thi các tiến trình trên nhiều lõi CPU đồng thời.
   - Thuật toán lập lịch hàng đợi đa cấp (**Multi-Level Queue - MLQ**) dựa trên độ ưu tiên và phân bổ slot thời gian (`MAX_PRIO = 140`).
   - Quản lý trạng thái và chu kỳ sống của tiến trình qua cấu trúc `pcb_t` (Process Control Block).
2. **Quản lý Bộ nhớ ảo & Phân trang hai cấp (Virtual Memory & 2-Level Paging)**:
   - Quản lý không gian địa chỉ logic/ảo 20-bit: 5-bit Page Directory (Level 1), 5-bit Page Table (Level 2), 10-bit Offset (Kích thước khung trang 1KB).
   - Mô phỏng bộ nhớ vật lý RAM (`mram`) và tối đa 4 thiết bị lưu trữ hoán đổi Swap (`mswp`).
   - Quản lý bảng trang (PTE) với các cờ `PRESENT`, `SWAPPED`, `DIRTY`, và số hiệu khung trang `FPN`.
   - Cơ chế cấp phát vùng nhớ động (`liballoc`, `libfree`), mở rộng vùng nhớ qua `sys_memmap` (tương tự cơ chế `sbrk`).
3. **Bộ xử lý lời gọi hệ thống (System Call Dispatcher)**:
   - Hệ thống chuyển đổi bẫy ngắt phần mềm để xử lý các lời gọi hệ thống từ user-space vào kernel-space:
     - `sys_listsyscall` (ID: 0): Liệt kê danh sách các system call hỗ trợ.
     - `sys_memmap` (ID: 17): Thay đổi kích thước và ánh xạ vùng nhớ heap.
     - `sys_killall` (ID: 101): Tìm kiếm và chấm dứt tất cả các tiến trình có tên chỉ định, giải phóng tài nguyên nhớ và loại bỏ khỏi hàng đợi chạy.
     - `sys_xxxhandler` (ID: 440): Xử lý mở rộng.

---

## 🏗️ 2. Cấu trúc thư mục (Directory Structure)

```text
├── include/              # Header files định nghĩa các cấu trúc dữ liệu và API
│   ├── common.h          # Định nghĩa pcb_t, page_table_t, trans_table_t, opcodes
│   ├── cpu.h             # Giao diện CPU và hàm run(proc)
│   ├── libmem.h          # Thư viện quản lý bộ nhớ tiến trình (liballoc, libfree, libread, libwrite)
│   ├── loader.h          # Giao diện nạp tiến trình từ file script
│   ├── mm.h              # Cấu trúc mm_struct, vm_area_struct, vm_rg_struct, PTE
│   ├── os-cfg.h          # Cấu hình tính năng hệ thống (MLQ_SCHED, MM_PAGING, ...)
│   ├── queue.h           # Cấu trúc hàng đợi tiến trình
│   ├── sched.h           # Giao diện bộ lập lịch
│   ├── syscall.h         # Định nghĩa các hằng số và khung thanh ghi system call
│   └── timer.h           # Bộ đếm xung nhịp và đồng bộ luồng
├── src/                  # Mã nguồn C
│   ├── cpu.c             # Mô phỏng tập lệnh CPU (CALC, ALLOC, FREE, READ, WRITE, SYSCALL)
│   ├── libmem.c          # Quản lý heap, vùng nhớ động, đọc/ghi ô nhớ logic
│   ├── loader.c          # Nạp file mô tả mã tiến trình vào PCB
│   ├── mm.c              # Khởi tạo và cập nhật PTE, quản lý bảng trang 2 cấp
│   ├── mm-memphy.c       # Thao tác trên thiết bị nhớ vật lý giả lập (RAM, SWAP)
│   ├── mm-vm.c           # Quản lý không gian địa chỉ ảo và vùng nhớ VMA
│   ├── os.c              # Hàm main(), quản lý vòng lặp luồng CPU và Loader
│   ├── queue.c           # Hàng đợi tiến trình an toàn luồng
│   ├── sched.c           # Triển khai thuật toán lập lịch MLQ
│   ├── syscall.c         # Bộ điều phối System Call
│   ├── sys_killall.c     # Triển khai lời gọi hệ thống killall theo tên tiến trình
│   ├── sys_listsyscall.c # Liệt kê danh sách system call
│   ├── sys_mem.c         # Cấp phát / giải phóng vùng nhớ heap
│   └── timer.c           # Cơ chế đồng bộ đa luồng theo chu kỳ thời gian
├── input/                # Các tệp kịch bản mô phỏng kiểm thử
│   ├── proc/             # Mã lệnh của các tiến trình giả lập (m0s, p0s, s0, ...)
│   └── os_*              # Cấu hình hệ thống (số CPU, time slot, dung lượng RAM/Swap, danh sách tiến trình)
├── output/               # Kết quả vết log (trace log) mẫu khi chạy hệ thống
└── Makefile              # Kịch bản biên dịch hệ thống
```

---

## ⚙️ 3. Tập lệnh mô phỏng của tiến trình (Process Instructions)

Mỗi file trong `input/proc/` đại diện cho một chương trình độc lập với các dòng lệnh:
- `calc`: Thực hiện tính toán trên CPU.
- `alloc <size> <reg_idx>`: Cấp phát `<size>` bytes bộ nhớ và lưu địa chỉ trả về vào thanh ghi `<reg_idx>`.
- `free <reg_idx>`: Giải phóng vùng nhớ trỏ bởi thanh ghi `<reg_idx>`.
- `read <reg_source> <offset> <reg_dest>`: Đọc byte dữ liệu từ địa chỉ `[reg_source] + offset` vào thanh ghi `reg_dest`.
- `write <data> <reg_dest> <offset>`: Ghi byte `data` vào địa chỉ `[reg_dest] + offset`.
- `syscall <id> <arg0> <arg1> <arg2>`: Thực thi lời gọi hệ thống tương ứng.

---

## 🚀 4. Hướng dẫn biên dịch & Chạy thử nghiệm (Build & Run)

### Yêu cầu hệ thống:
- Hệ điều hành Linux (Ubuntu / Debian / WSL) hoặc môi trường MinGW/MSYS2 trên Windows.
- Trình biên dịch: `gcc` hỗ trợ chuẩn `-pthread`.
- Tiện ích `make`.

### Biên dịch:
```bash
# Biên dịch toàn bộ hệ thống
make clean
make all
```

### Chạy mô phỏng:
Chạy chương trình với tham số là tên tệp cấu hình trong thư mục `input/`:
```bash
# Chạy với kịch bản MLQ và Paging
./os os_1_mlq_paging

# Chạy với cấu hình nhỏ 1K
./os os_1_mlq_paging_small_1K

# Chạy kiểm thử System Call
./os os_syscall
```

---

## 📝 5. Định dạng file cấu hình (Configuration Format)

Ví dụ file `input/os_1_mlq_paging`:
```text
2 4 8                      # [time_slot] [số lượng CPU] [số lượng tiến trình]
2048 2048 0 0 0            # [Kích thước RAM] [Kích thước SWAP 0..3]
1 p0s  130                 # [start_time] [tên file tiến trình] [độ ưu tiên prio]
2 s3   39
4 m1s  15
...
```

---

## 👥 Tác giả & Bản quyền
- **Môn học**: CO2018 - Hệ điều hành (HCMUT VNU-HCM)
- **Giảng viên hướng dẫn gốc**: TS. Phạm Đăng Nguyễn (pdnguyen)
