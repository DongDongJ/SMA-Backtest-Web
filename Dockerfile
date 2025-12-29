# 使用 GCC 編譯環境
FROM gcc:latest

# 設定工作目錄
WORKDIR /app

# 複製所有檔案
COPY . .

# 編譯 C++ 程式
RUN g++ -std=c++11 -pthread SMA_web.cpp -o server

# 暴露端口（Render 會自動設定 PORT 環境變數）
EXPOSE 10000

# 執行程式
CMD ["./server"]
