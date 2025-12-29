# 第一階段：編譯
FROM gcc:latest AS builder
WORKDIR /app
COPY . .
# 假設你的 SMA.cpp 不需要額外函式庫，直接編譯
RUN g++ -o sma_app SMA.cpp

# 第二階段：運行環境
FROM debian:stable-slim
WORKDIR /root/
# 從編譯階段拷貝執行檔和數據
COPY --from=builder /app/sma_app .
COPY --from=builder /app/money.csv .

# 暴露 Render 需要的 Port (通常是 10000)
EXPOSE 10000

# 執行程式
CMD ["./sma_app"]
