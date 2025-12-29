#include "httplib.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cmath>
using namespace std;

// ========== 您原本的結構體和函式 ==========

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
    vector<TradeRecord> trades;
};

// 計算簡單移動平均 (SMA) - 完全保留您的邏輯
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

// 執行單一組合的回測 - 完全保留您的邏輯
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

// ========== 新增：全域資料儲存 ==========
vector<string> allDates;
vector<long double> allCloses;
bool dataLoaded = false;
string targetStock = "MMM";

// ========== 新增：載入 CSV 函式 ==========
bool loadCSVData(const string& filepath) {
    if (dataLoaded) return true;

    ifstream file(filepath);
    if (!file.is_open()) {
        cerr << "❌ 無法打開檔案: " << filepath << endl;
        return false;
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
        cerr << "❌ 找不到股票: " << targetStock << endl;
        return false;
    }

    // 讀取資料
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

    dataLoaded = true;
    cout << "✓ 資料載入完成，共 " << allDates.size() << " 筆 " << targetStock << " 資料\n";
    return true;
}

// ========== 新增：參數優化函式 ==========
vector<TradeResult> optimizeParameters(const string& startDate, const string& endDate,
    long double initialCash, int minMA, int maxMA) {
    // 找出日期範圍
    int startIdx = -1, endIdx = -1;
    for (int i = 0; i < allDates.size(); i++) {
        if (allDates[i] == startDate) startIdx = i;
        if (allDates[i] == endDate) endIdx = i;
    }

    if (startIdx == -1 || endIdx == -1) {
        cerr << "❌ 找不到指定的日期範圍\n";
        return vector<TradeResult>();
    }

    vector<TradeResult> allResults;

    cout << "🔄 開始測試所有參數組合...\n";

    for (int shortMA = minMA; shortMA <= maxMA; shortMA++) {
        for (int longMA = minMA; longMA <= maxMA; longMA++) {
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

            TradeResult result = backtest(dates, closes, shortMA, longMA,
                initialCash, dataStartIdx, outputStartIdx);

            allResults.push_back(result);
        }
    }

    // 排序
    sort(allResults.begin(), allResults.end(),
        [](const TradeResult& a, const TradeResult& b) {
            if (abs(a.finalValue - b.finalValue) > 0.0001) {
                return a.finalValue > b.finalValue;
            }
            int diffA = abs(a.longMA - a.shortMA);
            int diffB = abs(b.longMA - b.shortMA);
            if (diffA != diffB) {
                return diffA > diffB;
            }
            if (a.shortMA != b.shortMA) {
                return a.shortMA < b.shortMA;
            }
            return a.longMA < b.longMA;
        });

    cout << "✓ 測試完成，共 " << allResults.size() << " 組策略\n";
    return allResults;
}

// ========== Web API 伺服器 ==========
int main() {
    using namespace httplib;

    // 載入資料
    cout << "========================================\n";
    cout << "🚀 SMA 雙均線回測系統 API Server\n";
    cout << "========================================\n";

    if (!loadCSVData("2014-2025_30stock.csv")) {
        cerr << "❌ 資料載入失敗！程式結束。\n";
        return 1;
    }

    Server svr;

    // CORS 設定
    svr.set_post_routing_handler([](const auto& req, auto& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        });

    svr.Options(".*", [](const auto& req, auto& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return res.set_content("", "text/plain");
        });

    // API 1: 健康檢查
    svr.Get("/", [](const Request& req, Response& res) {
        res.set_content("SMA Trading Strategy API is running! ✓", "text/plain");
        });

    // API 2: 單一策略回測
    svr.Get("/backtest", [](const Request& req, Response& res) {
        // 取得參數
        int shortMA = req.has_param("shortMA") ? stoi(req.get_param_value("shortMA")) : 5;
        int longMA = req.has_param("longMA") ? stoi(req.get_param_value("longMA")) : 20;
        string startDate = req.has_param("startDate") ? req.get_param_value("startDate") : "1/2/2023";
        string endDate = req.has_param("endDate") ? req.get_param_value("endDate") : "12/29/2023";
        long double initialCash = 10000.0;

        cout << "📊 收到回測請求: ShortMA=" << shortMA << ", LongMA=" << longMA << "\n";

        // 找出日期範圍
        int startIdx = -1, endIdx = -1;
        for (int i = 0; i < allDates.size(); i++) {
            if (allDates[i] == startDate) startIdx = i;
            if (allDates[i] == endDate) endIdx = i;
        }

        if (startIdx == -1 || endIdx == -1) {
            res.set_content("{\"error\": \"找不到指定的日期範圍\"}", "application/json");
            return;
        }

        // 準備資料
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

        // 建立 JSON 回應
        stringstream json;
        json << fixed << setprecision(2);
        json << "{";
        json << "\"shortMA\": " << result.shortMA << ",";
        json << "\"longMA\": " << result.longMA << ",";
        json << "\"initialCash\": " << initialCash << ",";
        json << "\"finalValue\": " << result.finalValue << ",";
        json << "\"returnRate\": " << result.returnRate << ",";
        json << "\"tradeCount\": " << result.tradeCount << ",";
        json << "\"trades\": [";

        for (int i = 0; i < result.trades.size(); i++) {
            if (i > 0) json << ",";
            json << "{";
            json << "\"date\": \"" << result.trades[i].date << "\",";
            json << "\"action\": \"" << result.trades[i].action << "\",";
            json << "\"price\": " << result.trades[i].price << ",";
            json << "\"shares\": " << (int)result.trades[i].shares << ",";
            json << "\"cashAfter\": " << result.trades[i].cashAfter;
            json << "}";
        }

        json << "]}";

        cout << "✓ 回測完成\n";
        res.set_content(json.str(), "application/json");
        });

    // API 3: 參數優化
    svr.Get("/optimize", [](const Request& req, Response& res) {
        string startDate = req.has_param("startDate") ? req.get_param_value("startDate") : "1/2/2023";
        string endDate = req.has_param("endDate") ? req.get_param_value("endDate") : "12/29/2023";
        int minMA = req.has_param("minMA") ? stoi(req.get_param_value("minMA")) : 1;
        int maxMA = req.has_param("maxMA") ? stoi(req.get_param_value("maxMA")) : 50;
        int topN = req.has_param("topN") ? stoi(req.get_param_value("topN")) : 20;
        long double initialCash = 10000.0;

        cout << "🔍 收到優化請求: MA範圍 " << minMA << "-" << maxMA << "\n";

        vector<TradeResult> results = optimizeParameters(startDate, endDate, initialCash, minMA, maxMA);

        if (results.empty()) {
            res.set_content("{\"error\": \"優化失敗\"}", "application/json");
            return;
        }

        // 建立 JSON 回應（只返回前 N 名）
        stringstream json;
        json << fixed << setprecision(2);
        json << "{\"results\": [";

        int limit = min(topN, (int)results.size());
        for (int i = 0; i < limit; i++) {
            if (i > 0) json << ",";
            json << "{";
            json << "\"rank\": " << (i + 1) << ",";
            json << "\"shortMA\": " << results[i].shortMA << ",";
            json << "\"longMA\": " << results[i].longMA << ",";
            json << "\"finalValue\": " << results[i].finalValue << ",";
            json << "\"returnRate\": " << results[i].returnRate << ",";
            json << "\"tradeCount\": " << results[i].tradeCount;
            json << "}";
        }

        json << "], \"total\": " << results.size() << "}";

        cout << "✓ 優化完成\n";
        res.set_content(json.str(), "application/json");
        });

    // 啟動伺服器
    const char* port_env = getenv("PORT");
    int port = port_env ? atoi(port_env) : 10000;

    cout << "----------------------------------------\n";
    cout << "Server running at: http://0.0.0.0:" << port << "\n";
    cout << "API Endpoints:\n";
    cout << "  GET  /              - 健康檢查\n";
    cout << "  GET  /backtest      - 單一策略回測\n";
    cout << "  GET  /optimize      - 參數優化\n";
    cout << "========================================\n";

    svr.listen("0.0.0.0", port);

    return 0;
}