#!/bin/bash

# Đọc thông tin nhiệt độ & tốc độ quạt hiện tại
fan_info=$(nbfc status | grep -E "Temperature|Fan Speed")

# Hiển thị giao diện chọn chế độ
mode=$(yad --title="Điều khiển quạt" \
  --text="$fan_info" \
  --form \
  --field="Chế độ:CB" \
  --field="Tốc độ (RPM):NUM" \
  "Tự động!Thủ công" "3000")

# Nếu người dùng cancel thì thoát
[ $? -ne 0 ] && exit 0

# Parse kết quả
mode_selected=$(echo "$mode" | cut -d"|" -f1)
speed=$(echo "$mode" | cut -d"|" -f2)

# Gửi lệnh tương ứng
if [ "$mode_selected" = "Tự động" ]; then
  nbfc set -a
  echo "Chế độ quạt đã được đặt thành Tự động."
else
  nbfc manual "$speed"
fi
