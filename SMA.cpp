#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
using namespace std;

struct TradeRecord {
    string date;
    string action;
    long double price;
    long double shares;
    long double cashAfter;
};

struct TradeResult {
    int shortMA;
    int longMA;
    long double finalValue;
    long double returnRate;
    int tradeCount;
    vector<TradeRecord> trades;  // 新增：交易記錄
};

// 計算簡單移動平均 (SMA)
vector<long double> computeMA(const vector<long double>& closes, int window) {
    vector<long double> ma;
    if (closes.size() < window) return ma;

    long double sum = 0.0;
    for (int i = 0; i < window; i++)
        sum += closes[i];
    ma.push_back(sum / window);

    for (int i = window; i < closes.size(); i++) {
        sum += closes[i];
        sum -= closes[i - window];
        ma.push_back(sum / window);
    }
    return ma;
}

// 執行單一組合的回測
TradeResult backtest(const vector<string>& dates, const vector<long double>& closes,
    int shortMA_window, int longMA_window, long double initialCash,
    int dataStartIdx, int outputStartIdx) {

    TradeResult result;
    result.shortMA = shortMA_window;
    result.longMA = longMA_window;
    result.tradeCount = 0;

    // 計算均線
    vector<long double> shortMA = computeMA(closes, shortMA_window);
    vector<long double> longMA = computeMA(closes, longMA_window);

    // 交易邏輯
    long double cash = initialCash;
    long double shares = 0;

    for (int i = outputStartIdx; i < dates.size(); i++) {
        int shortMAIdx = i - (shortMA_window - 1);
        int longMAIdx = i - (longMA_window - 1);

        if (shortMAIdx < 0 || longMAIdx < 0 ||
            shortMAIdx >= shortMA.size() || longMAIdx >= longMA.size()) {
            continue;
        }

        if (i == outputStartIdx || shortMAIdx == 0 || longMAIdx == 0) continue;

        int prevShortMAIdx = shortMAIdx - 1;
        int prevLongMAIdx = longMAIdx - 1;

        long double currShortMA = shortMA[shortMAIdx];
        long double currLongMA = longMA[longMAIdx];
        long double prevShortMA = shortMA[prevShortMAIdx];
        long double prevLongMA = longMA[prevLongMAIdx];
        long double currPrice = closes[i];

        // 黃金交叉：買入
        if (prevShortMA <= prevLongMA && currShortMA > currLongMA && shares == 0) {
            shares = (long double)((int)(cash / currPrice));
            long double cost = shares * currPrice;
            cash -= cost;
            result.tradeCount++;

            // 記錄交易
            TradeRecord trade;
            trade.date = dates[i];
            trade.action = "買入";
            trade.price = currPrice;
            trade.shares = shares;
            trade.cashAfter = cash;
            result.trades.push_back(trade);
        }
        // 死亡交叉：賣出
        else if (prevShortMA >= prevLongMA && currShortMA < currLongMA && shares > 0) {
            long double revenue = shares * currPrice;
            cash += revenue;

            // 記錄交易
            TradeRecord trade;
            trade.date = dates[i];
            trade.action = "賣出";
            trade.price = currPrice;
            trade.shares = shares;
            trade.cashAfter = cash;
            result.trades.push_back(trade);

            shares = 0;
            result.tradeCount++;
        }
    }

    // 最終結算
    result.finalValue = cash;
    if (shares > 0) {
        result.finalValue += shares * closes.back();

        // 記錄期末平倉
        TradeRecord trade;
        trade.date = dates.back();
        trade.action = "期末賣出";
        trade.price = closes.back();
        trade.shares = shares;
        trade.cashAfter = result.finalValue;
        result.trades.push_back(trade);

        result.tradeCount++;
    }
    result.returnRate = ((result.finalValue - initialCash) / initialCash) * 100.0;

    return result;
}

int main() {
    // ========== 設定參數 ==========
    string startDate = "1/1/2024";
    string endDate = "12/31/2024";
    string targetStock = "AAPL";
    long double initialCash = 10000.0;

    // 參數範圍設定
    int minMA = 1;
    int maxMA = 256;

    cout << "========== 雙均線策略參數優化器 (實驗模式) ==========\n";
    cout << "目標股票: " << targetStock << "\n";
    cout << "日期範圍: " << startDate << " 至 " << endDate << "\n";
    cout << "初始資金: $" << fixed << setprecision(2) << initialCash << "\n";
    cout << "測試範圍: 短期 " << minMA << "-" << maxMA << " 天, 長期 " << minMA << "-" << maxMA << " 天\n";
    cout << "⚠️  實驗模式：包含 短期>=長期 的組合\n";
    cout << "總組合數: " << (maxMA - minMA + 1) * (maxMA - minMA + 1) << " 組\n";
    cout << "====================================================\n\n";

    ifstream file("C:/s_t_o_c_k/2014-2025_30stock.csv");
    if (!file.is_open()) {
        cerr << "無法打開檔案" << endl;
        return 1;
    }

    string line;
    vector<string> headers;
    int targetCol = -1;

    // 讀取標題列
    if (getline(file, line)) {
        stringstream ss(line);
        string header;
        int col = 0;
        while (getline(ss, header, ',')) {
            headers.push_back(header);
            if (header == targetStock) {
                targetCol = col;
            }
            col++;
        }
    }

    if (targetCol == -1) {
        cerr << "找不到股票: " << targetStock << endl;
        return 1;
    }

    // 讀取所有資料
    vector<string> allDates;
    vector<long double> allCloses;

    while (getline(file, line)) {
        stringstream ss(line);
        string value;
        int col = 0;
        string date;
        long double close = 0;

        while (getline(ss, value, ',')) {
            if (col == 0) {
                date = value;
            }
            else if (col == targetCol && !value.empty()) {
                close = stold(value);
            }
            col++;
        }

        if (!date.empty() && close > 0) {
            allDates.push_back(date);
            allCloses.push_back(close);
        }
    }
    file.close();

    cout << "讀取完成，共 " << allDates.size() << " 筆資料\n\n";

    // 找出日期範圍索引
    int startIdx = -1, endIdx = -1;
    for (int i = 0; i < allDates.size(); i++) {
        if (allDates[i] == startDate) startIdx = i;
        if (allDates[i] == endDate) endIdx = i;
    }

    if (startIdx == -1 || endIdx == -1) {
        cerr << "找不到指定的日期範圍\n";
        return 1;
    }

    // ========== 開始優化測試 ==========
    vector<TradeResult> allResults;
    int totalTests = 0;
    int validTests = 0;

    cout << "開始測試所有參數組合...\n";

    for (int shortMA = minMA; shortMA <= maxMA; shortMA++) {
        for (int longMA = minMA; longMA <= maxMA; longMA++) {
            totalTests++;

            // 提取資料（需要足夠的歷史資料來計算較長週期的均線）
            int longerPeriod = max(shortMA, longMA);
            int extraDays = longerPeriod - 1;
            int dataStartIdx = max(0, startIdx - extraDays);

            vector<string> dates;
            vector<long double> closes;
            for (int i = dataStartIdx; i <= endIdx; i++) {
                dates.push_back(allDates[i]);
                closes.push_back(allCloses[i]);
            }

            int outputStartIdx = startIdx - dataStartIdx;

            // 執行回測
            TradeResult result = backtest(dates, closes, shortMA, longMA,
                initialCash, dataStartIdx, outputStartIdx);

            allResults.push_back(result);
            validTests++;

            // 每5000組顯示進度
            if (validTests % 5000 == 0) {
                cout << "已測試 " << validTests << " 組參數..." << endl;
            }
        }
    }

    cout << "\n測試完成！\n";
    cout << "總測試組合: " << totalTests << " 組\n";
    cout << "實際測試組合: " << validTests << " 組\n\n";

    // 僅根據最終資產價值 (finalValue) 降序排序
    sort(allResults.begin(), allResults.end(),
        [](const TradeResult& a, const TradeResult& b) {
            return a.finalValue > b.finalValue;
        });

    // 顯示前 20 名最佳策略
    cout << "========== 前 20 名最佳策略 ==========\n";
    cout << "排名 | 短期MA | 長期MA | 最終資產 | 報酬率 | 交易次數\n";
    cout << "--------------------------------------------------------\n";

    int displayCount = min(20, (int)allResults.size());
    for (int i = 0; i < displayCount; i++) {
        cout << setw(4) << (i + 1) << " | ";
        cout << setw(6) << allResults[i].shortMA << " | ";
        cout << setw(6) << allResults[i].longMA << " | ";
        cout << "$" << setw(9) << fixed << setprecision(30) << allResults[i].finalValue << " | ";
        cout << setw(7) << fixed << setprecision(2) << allResults[i].returnRate << "% | ";
        cout << setw(8) << allResults[i].tradeCount << "\n";
    }

    // 顯示最佳策略詳細資訊
    if (!allResults.empty()) {
        TradeResult best = allResults[0];
        cout << "\n========== 🏆 最佳策略 🏆 ==========\n";
        cout << "短期均線: " << best.shortMA << " 天\n";
        cout << "長期均線: " << best.longMA << " 天\n";
        cout << "初始資金: $" << fixed << setprecision(2) << initialCash << "\n";
        cout << "最終資產: $" << best.finalValue << "\n";
        cout << "報酬率: " << best.returnRate << "%\n";
        cout << "交易次數: " << best.tradeCount << " 次\n";
        cout << "====================================\n\n";

        // 顯示交易明細
        cout << "========== 📋 交易明細 ==========\n";
        cout << "日期       | 動作     | 價格      | 股數   | 剩餘現金\n";
        cout << "-----------------------------------------------------------\n";
        for (const auto& trade : best.trades) {
            cout << setw(10) << trade.date << " | ";
            cout << setw(8) << trade.action << " | ";
            cout << "$" << setw(8) << fixed << setprecision(2) << trade.price << " | ";
            cout << setw(6) << (int)trade.shares << " | ";
            cout << "$" << setw(10) << fixed << setprecision(2) << trade.cashAfter << "\n";
        }
        cout << "====================================\n\n";
    }

    // 輸出所有結果到 CSV
    ofstream outFile("C:/SMA_github/money.csv");
    if (!outFile.is_open()) {
        cerr << "無法建立輸出檔案" << endl;
        return 1;
    }

    outFile << "Rank,ShortMA,LongMA,FinalValue,ReturnRate(%),TradeCount\n";
    for (int i = 0; i < allResults.size(); i++) {
        outFile << (i + 1) << ","
            << allResults[i].shortMA << ","
            << allResults[i].longMA << ","
            << fixed << setprecision(30) << allResults[i].finalValue << ","
            << fixed << setprecision(30) << allResults[i].returnRate << ","
            << allResults[i].tradeCount << "\n";
    }
    outFile.close();

    cout << "✓ 完整結果已輸出到：C:/Users/Lab114/source/repos/SMA/money.csv\n";
    cout << "✓ 共 " << allResults.size() << " 組策略（包含實驗性組合）\n";

    return 0;
}