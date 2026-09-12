# Đối chiếu tính năng theo nền tảng

Mục đích: một chỗ duy nhất để thấy tính năng nào có/thiếu ở đâu, khỏi mò từng thư mục.
Kiến trúc chung: `engine/` (C, dùng chung) + UI native mỗi nền tảng (`linux/`, `windows/`, `macos/`).

Ký hiệu: ✅ có · ⚠️ một phần · ❌ chưa có · ➖ hệ điều hành lo.

## Gõ (lõi engine dùng chung)

| Tính năng | Linux | Windows | macOS |
|---|---|---|---|
| Telex / VNI / dấu hiện đại-truyền thống | ✅ | ✅ | ✅ |
| Hoàn tác phím tắt (`ww`→`w`) | ✅ | ✅ | ✅ |
| Macro/emoji mở rộng khi commit | ✅ | ✅ | ✅ |
| Giữ phần gõ dở khi click sang chỗ khác | ✅ | ✅ (composition TSF) | ✅ (marked text) |

## Hiển thị và gợi ý

| Tính năng | Linux | Windows | macOS |
|---|---|---|---|
| Gạch đỏ chính tả khi gõ | ✅ | ❌ | ❌ |
| Bảng gợi ý AI inline | ✅ | ❌ (balloon thay thế) | ❌ |
| Nhận gợi ý + học từ đã chọn | ✅ (Tab/số/Enter) | ✅ (click balloon) | ❌ |
| Gợi ý emoji khi gõ `:sm` | ✅ | ❌ | ❌ |

## Cấu hình (cùng định dạng file cả 3 nền tảng)

| Tính năng | Linux | Windows | macOS |
|---|---|---|---|
| Đổi Telex↔VNI trong UI | ✅ (tray + Setup) | ✅ (tray + Setup) | ❌ (sửa file tay) |
| Chuẩn dấu / chính tả trong UI | ✅ | ✅ | ❌ |
| Cấu hình AI trong UI | ✅ | ✅ | ❌ |
| Quản lý macro/emoji trong UI | ❌ | ❌ | ✅ (duy nhất) |

## Khay, menu, hệ thống

| Tính năng | Linux | Windows | macOS |
|---|---|---|---|
| Indicator/menu riêng | ✅ AppIndicator | ✅ tray icon | ➖ menu input hệ thống |
| Chuyển Việt/Anh | ✅ nguồn Super+Space | ✅ Win+Space | ✅ menu input |
| Tự khởi động | ✅ xdg autostart | ✅ registry/task | ❌ |
| Bỏ qua ô mật khẩu | ✅ | ⚠️ hệ thống (một phần) | ⚠️ hệ thống |
| Nhật ký chẩn đoán | ✅ `debug/crash.log` | ✅ `update.log` | ❌ |

## Cập nhật theo release GitHub

| Tính năng | Linux | Windows | macOS |
|---|---|---|---|
| Tự kiểm tra định kỳ | ✅ | ✅ | ✅ |
| Kiểm tra tay trong UI | ✅ menu tray | ✅ menu tray | ❌ (có hàm, chưa nút bấm) |
| Cài đặt từ UI | ✅ `pkexec dpkg -i` | ✅ silent + thoát app | ⚠️ tải về, chép tay + logout/login |

## Backlog rút từ bảng trên (ưu tiên giảm dần)

1. **macOS: chọn Telex/VNI + cấu hình AI trong Setup** — engine đã đọc config, chỉ thiếu UI ghi (editor macro đã có sẵn pattern).
2. **macOS: nút Kiểm tra cập nhật** — hàm `GoTiengVietCheckForUpdates` đã có, chỉ thiếu chỗ bấm.
3. **Gạch đỏ + gợi ý inline cho TSF/macOS** — việc lớn (display attributes / marked-text attrs).
4. **macOS: autostart + gỡ cài đặt** (docs trước, code sau nếu cần).
5. Gợi ý emoji `:sm` cho Windows/macOS.
6. Bật/tắt TSF rõ ràng khi hồ sơ ngôn ngữ bị tắt tay trong Settings (đọc README phần Windows).
